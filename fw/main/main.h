#pragma once

#include "esp_err.h"

void wifi_init_sta();

esp_err_t update_time_from_nvs(void);
esp_err_t fetch_and_store_time_in_nvs(void*);

void http_get_task(void *_unused);

void i2s_init();

void audio_init();
