#pragma once

#include "esp_err.h"

void wifi_init_sta();

esp_err_t update_time_from_nvs(void);
esp_err_t fetch_and_store_time_in_nvs(void*);

const char *simple_request(const char *url, const char *cookies);
void http_get_task(void *_unused);

void i2s_init();
esp_err_t i2s_enable();
esp_err_t i2s_read(int32_t *buf, size_t *n, size_t max_n);

void audio_init();
