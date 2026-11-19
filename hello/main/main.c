// Bring-up app for the Guition JC3636K718 round knob display.
#include "audio.h"
#include "board.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "haptic.h"
#include "ui.h"

static const char *TAG = "hello";

void app_main(void)
{
    for (int i = 5; i > 0; i--) {
        ESP_LOGI(TAG, "knob hello starting in %d, PSRAM %u KB, reset reason %d", i,
                 (unsigned)(esp_psram_get_size() / 1024), (int)esp_reset_reason());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    board_init();
    ESP_LOGI(TAG, "step: board ok");
    haptic_init();
    ESP_LOGI(TAG, "step: haptic ok");
    audio_init();
    ESP_LOGI(TAG, "step: audio ok");
    ui_start();
    ESP_LOGI(TAG, "step: ui ok");
    board_backlight(0);
    for (int i = 0; i <= 100; i += 4) {
        board_backlight(i);
        vTaskDelay(pdMS_TO_TICKS(15));
    }
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "heap internal %u KB, psram %u KB, bat %.2f V, mic %.0f dBFS",
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
                 board_battery_volts(), audio_mic_dbfs());
    }
}
