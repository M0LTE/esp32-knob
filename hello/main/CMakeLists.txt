idf_component_register(SRCS "main.c" "board.c" "ui.c" "audio.c" "haptic.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_i2s esp_driver_gpio esp_driver_spi esp_driver_i2c esp_adc esp_lcd esp_timer esp_psram)
