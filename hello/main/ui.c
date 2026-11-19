// "Hello, world" bring-up screen: knob drives an arc, LED ring and haptics;
// tapping the glass makes a ripple, a chime and a click; the mic pulses the centre.
#include "ui.h"

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>

#include "audio.h"
#include "board.h"
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "haptic.h"

static lv_obj_t *s_arc, *s_value, *s_pulse, *s_status, *s_scr;
static atomic_int s_pending;       // knob steps not yet applied
static volatile int s_val = 50;    // 0..100
static volatile float s_level;     // mic level 0..1
static volatile uint32_t s_flash;  // tick count when the LED ring should flash white

static lv_color_t hue_color(int v)
{
    return lv_color_hsv_to_rgb((uint16_t)(200 + v * 1.6f) % 360, 85, 100);
}

static void apply_value(void)
{
    lv_color_t c = hue_color(s_val);
    lv_arc_set_value(s_arc, s_val);
    lv_obj_set_style_arc_color(s_arc, c, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_arc, c, LV_PART_KNOB);
    lv_obj_set_style_bg_color(s_pulse, c, 0);
    lv_label_set_text_fmt(s_value, "%d", s_val);
}

static void on_knob(int delta) { atomic_fetch_add(&s_pending, delta); }

static void ripple_done(lv_anim_t *a) { lv_obj_delete((lv_obj_t *)a->var); }
static void ripple_size(void *o, int32_t v)
{
    lv_obj_set_size(o, v, v);
    lv_obj_set_style_opa(o, (lv_opa_t)(255 - v * 255 / 260), 0);
}

static void on_press(lv_event_t *e)
{
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    lv_obj_t *r = lv_obj_create(s_scr);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(r, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(r, 4, 0);
    lv_obj_set_style_border_color(r, hue_color(s_val), 0);
    lv_obj_set_style_align(r, LV_ALIGN_CENTER, 0);
    lv_obj_set_pos(r, p.x - LCD_H_RES / 2, p.y - LCD_V_RES / 2);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, r);
    lv_anim_set_exec_cb(&a, ripple_size);
    lv_anim_set_values(&a, 10, 260);
    lv_anim_set_duration(&a, 600);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, ripple_done);
    lv_anim_start(&a);

    float base = 440.0f * powf(2.0f, (s_val - 50) / 25.0f);
    audio_play(base, base * 1.5f, 450, 0.35f);
    haptic_play(HAPTIC_STRONG_CLICK);
    s_flash = xTaskGetTickCount() + pdMS_TO_TICKS(120);
}

static void tick(lv_timer_t *t)
{
    int d = atomic_exchange(&s_pending, 0);
    if (d) {
        int nv = s_val + d;
        if (nv < 0) nv = 0;
        if (nv > 100) nv = 100;
        if (nv != s_val) {
            s_val = nv;
            apply_value();
            haptic_play(HAPTIC_SHARP_TICK);
            audio_play(2400, 3600, 12, 0.08f);
        } else {
            haptic_play(HAPTIC_BUZZ);  // hit the end stop
        }
    }

    float db = audio_mic_dbfs();
    float lvl = (db + 70) / 50;  // -70 dBFS .. -20 dBFS -> 0..1
    lvl = lvl < 0 ? 0 : lvl > 1 ? 1 : lvl;
    s_level = s_level * 0.6f + lvl * 0.4f;
    int sz = 150 + (int)(s_level * 120);
    lv_obj_set_size(s_pulse, sz, sz);
    lv_obj_set_style_bg_opa(s_pulse, (lv_opa_t)(25 + s_level * 90), 0);

    static int n;
    if (++n % 25 == 0) {
        char b[64];
        snprintf(b, sizeof b, "mic %.0f dB   bat %.2f V   ram %u k", db, board_battery_volts(),
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024));
        lv_label_set_text(s_status, b);
    }
}

static void boot_anim_exec(void *o, int32_t v)
{
    s_val = v;
    apply_value();
}

static void leds_task(void *arg)
{
    float phase = 0;
    for (;;) {
        phase += 0.05f;
        float breathe = 0.25f + 0.15f * sinf(phase);
        float pos = s_val * RGB_LED_COUNT / 100.0f;
        lv_color_t c = hue_color(s_val);
        bool flash = (int32_t)(s_flash - xTaskGetTickCount()) > 0;
        for (int i = 0; i < RGB_LED_COUNT; i++) {
            float dist = fabsf(i - pos);
            if (dist > RGB_LED_COUNT / 2.0f) dist = RGB_LED_COUNT - dist;
            float k = expf(-dist * dist / 1.5f) * 0.8f + breathe * 0.15f + s_level * 0.5f;
            if (k > 1) k = 1;
            const float max = 60;  // keep the ring gentle on the eyes and the battery
            if (flash) {
                leds_set(i, 120, 120, 120);
            } else {
                leds_set(i, c.red * k * max / 255, c.green * k * max / 255, c.blue * k * max / 255);
            }
        }
        leds_show();
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

void ui_start(void)
{
    lvgl_port_lock(0);
    s_scr = lv_screen_active();
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x05070d), 0);
    lv_obj_set_style_bg_grad_color(s_scr, lv_color_hex(0x121a2e), 0);
    lv_obj_set_style_bg_grad_dir(s_scr, LV_GRAD_DIR_VER, 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_scr, on_press, LV_EVENT_PRESSED, NULL);

    s_pulse = lv_obj_create(s_scr);
    lv_obj_remove_flag(s_pulse, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_pulse, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_pulse, 0, 0);
    lv_obj_set_size(s_pulse, 150, 150);
    lv_obj_center(s_pulse);

    s_arc = lv_arc_create(s_scr);
    lv_obj_set_size(s_arc, 340, 340);
    lv_obj_center(s_arc);
    lv_arc_set_bg_angles(s_arc, 135, 45);
    lv_arc_set_range(s_arc, 0, 100);
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_arc, 18, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 18, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x1c2438), LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_arc, 4, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(s_arc, 18, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(s_arc, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(s_arc, LV_OPA_40, LV_PART_KNOB);

    lv_obj_t *hello = lv_label_create(s_scr);
    lv_label_set_text(hello, "HELLO, WORLD");
    lv_obj_set_style_text_font(hello, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_letter_space(hello, 4, 0);
    lv_obj_set_style_text_color(hello, lv_color_hex(0xdfe6f5), 0);
    lv_obj_align(hello, LV_ALIGN_CENTER, 0, -62);

    s_value = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_value, lv_color_white(), 0);
    lv_obj_align(s_value, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *hint = lv_label_create(s_scr);
    lv_label_set_text(hint, "turn  -  tap  -  talk");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x8a96b0), 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 44);

    s_status = lv_label_create(s_scr);
    lv_label_set_text(s_status, "");
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(0x5d6a85), 0);
    lv_obj_align(s_status, LV_ALIGN_CENTER, 0, 112);

    apply_value();

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, boot_anim_exec);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_duration(&a, 700);
    lv_anim_set_playback_duration(&a, 500);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_timer_create(tick, 20, NULL);
    lvgl_port_unlock();

    board_set_knob_cb(on_knob);
    xTaskCreatePinnedToCore(leds_task, "leds", 3072, NULL, 3, NULL, 0);

    // Start-up chime: a rising major arpeggio.
    audio_play(523, 784, 160, 0.3f);
    audio_play(659, 988, 160, 0.3f);
    audio_play(784, 1175, 400, 0.3f);
    haptic_play(HAPTIC_STRONG_CLICK);
}
