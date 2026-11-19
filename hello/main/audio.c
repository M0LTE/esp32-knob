// Speaker out (PCM5100A I2S DAC + amp) and PDM microphone level.
#include "audio.h"

#include <math.h>
#include <string.h>

#include "board.h"
#include "driver/i2s_pdm.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "audio";

#define OUT_RATE 32000
#define MIC_RATE 16000

static i2s_chan_handle_t s_tx, s_rx;
static QueueHandle_t s_q;
static volatile float s_mic_db = -90;

typedef struct {
    float f1, f2, ms, vol;
} tone_t;

static void out_task(void *arg)
{
    static int16_t buf[256 * 2];
    tone_t t;
    for (;;) {
        if (xQueueReceive(s_q, &t, portMAX_DELAY) != pdTRUE) continue;
        gpio_set_level(PIN_PA_CTRL, 1);
        int total = (int)(OUT_RATE * t.ms / 1000);
        float p1 = 0, p2 = 0;
        for (int n = 0; n < total;) {
            int chunk = total - n < 256 ? total - n : 256;
            for (int i = 0; i < chunk; i++, n++) {
                float env = expf(-4.0f * n / total);
                float att = n < 64 ? n / 64.0f : 1.0f;
                float s = 0.6f * sinf(p1) + 0.4f * sinf(p2);
                p1 += 2 * (float)M_PI * t.f1 / OUT_RATE;
                p2 += 2 * (float)M_PI * t.f2 / OUT_RATE;
                int16_t v = (int16_t)(s * env * att * t.vol * 32767);
                buf[2 * i] = buf[2 * i + 1] = v;
            }
            size_t w;
            i2s_channel_write(s_tx, buf, chunk * 4, &w, portMAX_DELAY);
        }
        // Flush with silence so the amp does not pop on mute.
        memset(buf, 0, sizeof(buf));
        size_t w;
        for (int i = 0; i < 8; i++) i2s_channel_write(s_tx, buf, sizeof(buf), &w, portMAX_DELAY);
        if (uxQueueMessagesWaiting(s_q) == 0) gpio_set_level(PIN_PA_CTRL, 0);
    }
}

static void mic_task(void *arg)
{
    static int16_t buf[512];
    float dc = 0;
    for (;;) {
        size_t r = 0;
        if (i2s_channel_read(s_rx, buf, sizeof(buf), &r, portMAX_DELAY) != ESP_OK || r == 0) continue;
        int n = r / 2;
        double acc = 0;
        for (int i = 0; i < n; i++) {
            dc += 0.001f * (buf[i] - dc);
            float v = buf[i] - dc;
            acc += v * v;
        }
        float rms = sqrtf(acc / n) / 32768.0f;
        float db = 20 * log10f(rms + 1e-6f);
        s_mic_db = db > s_mic_db ? db : s_mic_db * 0.9f + db * 0.1f;
    }
}

float audio_mic_dbfs(void) { return s_mic_db; }

void audio_play(float f1, float f2, float ms, float vol)
{
    if (!s_q) return;
    tone_t t = {f1, f2, ms, vol};
    xQueueSend(s_q, &t, 0);
}

void audio_init(void)
{
    gpio_config_t pa = {.pin_bit_mask = 1ULL << PIN_PA_CTRL, .mode = GPIO_MODE_OUTPUT};
    gpio_config(&pa);
    gpio_set_level(PIN_PA_CTRL, 0);

    // PDM RX only exists on I2S0, so the DAC goes on I2S1.
    i2s_chan_config_t rx_chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    if (i2s_new_channel(&rx_chan, NULL, &s_rx) == ESP_OK) {
        i2s_pdm_rx_config_t pdm = {
            .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(MIC_RATE),
            .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
            .gpio_cfg = {.clk = PIN_MIC_CLK, .din = PIN_MIC_DATA},
        };
        if (i2s_channel_init_pdm_rx_mode(s_rx, &pdm) == ESP_OK && i2s_channel_enable(s_rx) == ESP_OK) {
            xTaskCreatePinnedToCore(mic_task, "mic", 4096, NULL, 4, NULL, 0);
        } else {
            ESP_LOGW(TAG, "mic init failed");
        }
    }

    i2s_chan_config_t tx_chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    tx_chan.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan, &s_tx, NULL));
    i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(OUT_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, .bclk = PIN_I2S_BCK, .ws = PIN_I2S_WS,
            .dout = PIN_I2S_DO, .din = I2S_GPIO_UNUSED,
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
    s_q = xQueueCreate(8, sizeof(tone_t));
    xTaskCreatePinnedToCore(out_task, "spk", 4096, NULL, 6, NULL, 0);
}
