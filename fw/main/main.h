#pragma once

#include "esp_err.h"
#include <stdbool.h>

#include "pins.h"

void wifi_init_sta();

esp_err_t update_time_from_nvs(void);
esp_err_t fetch_and_store_time_in_nvs(void*);

const char *simple_request(const char *url, const char *cookies);
typedef struct post_handle_t post_handle_t;
post_handle_t *post_create();
void post_open(const post_handle_t *p);
void post_write(const post_handle_t *p, const void *data, size_t len);
const char *post_finish(const post_handle_t *p);
int http_test();

void i2s_init();
esp_err_t i2s_enable();
esp_err_t i2s_disable();
esp_err_t i2s_read(int32_t *buf, size_t *n, size_t max_n);

void audio_init();
void audio_pause();
void audio_resume();
void audio_push(const int16_t *buf, size_t size);
bool audio_can_sleep();
void audio_clear_can_sleep();
int audio_wake_state();
void audio_clear_wake_state();
const int16_t *audio_speech_buffer();
int audio_speech_buffer_size();
bool audio_speech_ended();

void encode_init();
void encode_restart();
void encode_push(const int16_t *buf, size_t end);
size_t encode_pull();
void encode_wait_end();
const uint8_t *encode_buffer();

void led_init();
enum led_state_t {
  LED_STATE_IDLE,
  LED_STATE_STARTUP,
  LED_STATE_CONN_CHECK,
  LED_STATE_SPEECH,
  LED_STATE_WAIT_RESPONSE,
  LED_STATE_RUN,
  LED_STATE_ERROR,
  LED_STATE_SIGNAL,
  LED_STATE_CALIBRATE,
};
void led_set_state(enum led_state_t state, int transition);
bool led_set_program(const char *program_source);
uint32_t led_get_program_duration();

bool gauge_init();
struct gauge_state {
  uint32_t voltage;   // in uV
  uint32_t fuel;      // in 1e-6
  uint32_t discharge; // in 1e-6/h
};
struct gauge_state gauge_query();
void gauge_sleep();
void gauge_wake();
