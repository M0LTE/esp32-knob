// Board bring-up: QSPI display, touch, knob, LED ring, backlight, battery ADC.
#include "board.h"

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st77916.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "led_strip.h"

static const char *TAG = "board";

#include "st77916_init.h"

static lv_display_t *s_disp;
static i2c_master_bus_handle_t s_i2c;
static led_strip_handle_t s_leds;
static knob_step_cb_t s_knob_cb;
static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_adc_cali;

lv_display_t *board_display(void) { return s_disp; }
i2c_master_bus_handle_t board_i2c(void) { return s_i2c; }
void board_set_knob_cb(knob_step_cb_t cb) { s_knob_cb = cb; }

void board_backlight(uint8_t percent)
{
    if (percent > 100) percent = 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (1023 * percent) / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void backlight_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0, .freq_hz = 20000, .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    ledc_channel_config_t c = {
        .gpio_num = PIN_LCD_BLK, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0, .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&c));
}

static void i2c_init(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0, .sda_io_num = PIN_I2C_SDA, .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &s_i2c));
    // Log everything on the bus, useful for confirming the hardware map.
    for (uint8_t a = 0x08; a < 0x78; a++) {
        if (i2c_master_probe(s_i2c, a, 20) == ESP_OK) {
            ESP_LOGI(TAG, "i2c device at 0x%02x", a);
        }
    }
}

static void display_init(void)
{
    spi_bus_config_t bus = ST77916_PANEL_BUS_QSPI_CONFIG(PIN_LCD_SCK, PIN_LCD_D0, PIN_LCD_D1,
                                                          PIN_LCD_D2, PIN_LCD_D3,
                                                          LCD_H_RES * 40 * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = ST77916_PANEL_IO_QSPI_CONFIG(PIN_LCD_CS, NULL, NULL);
    io_cfg.pclk_hz = 40 * 1000 * 1000;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io));

    st77916_vendor_config_t vendor = {
        .init_cmds = factory_lcd_init_cmds,
        .init_cmds_size = sizeof(factory_lcd_init_cmds) / sizeof(factory_lcd_init_cmds[0]),
        .flags.use_qspi_interface = 1,
    };
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor,
    };
    esp_lcd_panel_handle_t panel;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));


    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 5;
    port_cfg.task_stack = 8192;
    port_cfg.task_affinity = 1;
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_H_RES * 40,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        // The factory init table already sets the scan direction; no extra mirroring.
        .rotation = {.swap_xy = false, .mirror_x = false, .mirror_y = false},
        .flags = {.buff_dma = true, .swap_bytes = true},
    };
    s_disp = lvgl_port_add_disp(&disp_cfg);

    esp_lcd_panel_io_handle_t tp_io;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_cfg.scl_speed_hz = 400000;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_i2c, &tp_io_cfg, &tp_io));
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES, .y_max = LCD_V_RES,
        .rst_gpio_num = PIN_TP_RST, .int_gpio_num = PIN_TP_INT,
        .flags = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
    };
    esp_lcd_touch_handle_t tp;
    if (esp_lcd_touch_new_i2c_cst816s(tp_io, &tp_cfg, &tp) == ESP_OK) {
        lvgl_port_touch_cfg_t t = {.disp = s_disp, .handle = tp};
        lvgl_port_add_touch(&t);
    } else {
        ESP_LOGW(TAG, "touch controller did not answer");
    }
}

// The bezel is not a quadrature encoder: each detent pulses one line low,
// GPIO2 for one direction and GPIO1 for the other (seen on the console, and
// matching the vendor's bidi_switch_knob.c). Poll both, debounce, and count
// a step when a line returns high after being low for a few samples.
#define KNOB_POLL_US 3000
#define KNOB_DEBOUNCE 2

typedef struct {
    gpio_num_t pin;
    int dir;
    uint8_t low_count;
} knob_line_t;

static knob_line_t s_lines[2] = {{PIN_KNOB_A, -1, 0}, {PIN_KNOB_B, +1, 0}};  // GPIO2 pulses on anticlockwise

static void knob_poll(void *arg)
{
    for (int i = 0; i < 2; i++) {
        knob_line_t *l = &s_lines[i];
        if (gpio_get_level(l->pin) == 0) {
            if (l->low_count < 255) l->low_count++;
        } else {
            if (l->low_count >= KNOB_DEBOUNCE && s_knob_cb) s_knob_cb(l->dir);
            l->low_count = 0;
        }
    }
}

static void knob_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_KNOB_A) | (1ULL << PIN_KNOB_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    const esp_timer_create_args_t t = {.callback = knob_poll, .name = "knob"};
    esp_timer_handle_t h;
    ESP_ERROR_CHECK(esp_timer_create(&t, &h));
    ESP_ERROR_CHECK(esp_timer_start_periodic(h, KNOB_POLL_US));
}

static void leds_init(void)
{
    led_strip_config_t strip = {
        .strip_gpio_num = PIN_RGB_DATA,
        .max_leds = RGB_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip, &rmt, &s_leds));
    led_strip_clear(s_leds);
}

void leds_set(int i, uint8_t r, uint8_t g, uint8_t b)
{
    if (s_leds && i >= 0 && i < RGB_LED_COUNT) led_strip_set_pixel(s_leds, i, r, g, b);
}

void leds_show(void)
{
    if (s_leds) led_strip_refresh(s_leds);
}

static void battery_init(void)
{
    adc_oneshot_unit_init_cfg_t u = {.unit_id = ADC_UNIT_1};
    if (adc_oneshot_new_unit(&u, &s_adc) != ESP_OK) return;
    adc_oneshot_chan_cfg_t c = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    adc_oneshot_config_channel(s_adc, ADC_CHANNEL_5, &c);  // GPIO6
    adc_cali_curve_fitting_config_t cc = {
        .unit_id = ADC_UNIT_1, .chan = ADC_CHANNEL_5, .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_curve_fitting(&cc, &s_adc_cali);
}

#ifndef BAT_DIVIDER
#define BAT_DIVIDER 2.0f
#endif

float board_battery_volts(void)
{
    if (!s_adc || !s_adc_cali) return 0;
    int raw, mv;
    if (adc_oneshot_read(s_adc, ADC_CHANNEL_5, &raw) != ESP_OK) return 0;
    if (adc_cali_raw_to_voltage(s_adc_cali, raw, &mv) != ESP_OK) return 0;
    return mv * BAT_DIVIDER / 1000.0f;
}

void board_init(void)
{
    ESP_LOGI(TAG, "init backlight_init");
    backlight_init();
    ESP_LOGI(TAG, "init i2c_init");
    i2c_init();
    ESP_LOGI(TAG, "init display_init");
    display_init();
    ESP_LOGI(TAG, "init knob_init");
    knob_init();
    ESP_LOGI(TAG, "init leds_init");
    leds_init();
    ESP_LOGI(TAG, "init battery_init");
    battery_init();
}
