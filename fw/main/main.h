#pragma once

#include "esp_err.h"

void wifi_init_sta();

esp_err_t update_time_from_nvs(void);
esp_err_t fetch_and_store_time_in_nvs(void*);

const char *simple_request(const char *url, const char *cookies);
typedef struct post_handle_t post_handle_t;
post_handle_t *post_create();
void post_open(const post_handle_t *p);
void post_write(const post_handle_t *p, void *data, size_t len);
void post_finish(const post_handle_t *p);
void http_test_task(void *_unused);

void i2s_init();
esp_err_t i2s_enable();
esp_err_t i2s_read(int32_t *buf, size_t *n, size_t max_n);

void audio_init();
int audio_wake_state();
void audio_clear_wake_state();
const int16_t *audio_speech_buffer();
int audio_speech_buffer_size();
