#include "main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <stdatomic.h>
#include <string.h>

#include "model_path.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "dl_lib_convq_queue.h"

#if (CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4)
#include "esp_nsn_models.h"
#include "esp_nsn_iface.h"
#endif

#define min(_a, _b) ((_a) < (_b) ? (_a) : (_b))

static const char *TAG = "Audio";

static esp_afe_sr_data_t *afe_data = NULL;

static void audio_task(void *_unused);

static TaskHandle_t audio_task_handle;

void audio_init()
{
  srmodel_list_t *models = esp_srmodel_init("model");
  char *wakenet_name = NULL;
  wakenet_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
  ESP_LOGI(TAG, "wakenet_name: %s", wakenet_name ? wakenet_name : "(null)");

  const esp_afe_sr_iface_t *afe_handle = &ESP_AFE_SR_HANDLE;
  afe_config_t afe_config = AFE_CONFIG_DEFAULT();
  afe_config.aec_init = false;
  afe_config.vad_init = false;
  afe_config.wakenet_init = true;
  afe_config.wakenet_model_name = wakenet_name;
  afe_config.afe_ns_mode = NS_MODE_SSP;
  afe_config.afe_ns_model_name = NULL;
  afe_config.pcm_config.total_ch_num = 1;
  afe_config.pcm_config.mic_num = 1;
  afe_config.pcm_config.ref_num = 0;

  afe_data = afe_handle->create_from_config(&afe_config);
  if (!afe_data) {
    ESP_LOGE(TAG, "afe_data is null!");
    return;
  }

  // Enable I2S first, before any read may happen in the task
  ESP_ERROR_CHECK(i2s_enable());

  xTaskCreate(audio_task, "audio_task", 16384, NULL, 5, &audio_task_handle);
}

static int below_sleep_threshold_count = 0;
static bool can_sleep = true;

static int wake_state = 0;

static SemaphoreHandle_t buffer_mutex;

static int16_t *speech_buffer;
#define SPEECH_BUFFER_SIZE (16000 * 10)
static int speech_buffer_ptr = 0;

static int below_speech_threshold_count = 0;  // TODO: What a messy name
static bool speech_ended_by_threshold = false;

// External signals

static bool i2s_channel_paused = false;

static atomic_flag request_to_disable = ATOMIC_FLAG_INIT; // Active-low
static const uint32_t *_Atomic external_push_buf = NULL;
static size_t external_push_size, external_push_start, external_push_count;

void audio_task(void *_unused)
{
  buffer_mutex = xSemaphoreCreateMutex();
  assert(buffer_mutex != NULL);

  speech_buffer = heap_caps_malloc(sizeof(int16_t) * SPEECH_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
  assert(speech_buffer != NULL);

  const esp_afe_sr_iface_t *afe_handle = &ESP_AFE_SR_HANDLE;

  int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
  int fetch_chunksize = afe_handle->get_fetch_chunksize(afe_data);
  int total_nch = afe_handle->get_total_channel_num(afe_data);
  assert(total_nch == 1);
  ESP_LOGI(TAG, "feed task start, feed_chunksize = %d, total_nch = %d, fetch_chunksize = %d", feed_chunksize, total_nch, fetch_chunksize);

  size_t buf_count = feed_chunksize * total_nch;
  size_t n = 0;
  int32_t *buf32 = malloc(sizeof(int32_t) * buf_count);
  int16_t *buf16 = malloc(sizeof(int16_t) * buf_count);
  int feed_count = 0;

  // Feed a block of `buf_count` samples into the AFE, and
  // fetch & process filtered samples when available
  void process_audio_block()
  {
    // ESP-SR calls for 16-bit samples, convert here
    for (int i = 0; i < buf_count; i++) buf16[i] = buf32[i] >> 16;
    afe_handle->feed(afe_data, buf16);
    feed_count += buf_count;

    if (feed_count >= fetch_chunksize) {
      afe_fetch_result_t *fetch_result = afe_handle->fetch(afe_data);
      static int n = 0;
      if ((n = (n + 1) % 16) == 15) {
        uint32_t rms = 0;
        for (int i = 0; i < buf_count; i++) {
          int32_t sample = buf16[i];
          rms += sample * sample;
        }
        ESP_LOGI(TAG, "fetch data size = %d, volume = %.5f, sample = %08x, RMS = %u", fetch_result->data_size, fetch_result->data_volume, (int)buf32[0], (unsigned)(rms / buf_count));
      }
      if (fetch_result->wakeup_state == WAKENET_DETECTED) {
        ESP_LOGI(TAG, "Wake word detected");
        wake_state = 1;
        below_speech_threshold_count = 0;
        speech_ended_by_threshold = false;
      }
      // XXX: Debug use
      // static int m = 0; if (wake_state == 0 && (m = (m + 1) % 64) == 63) wake_state = 1;
      if (fgetc(stdin) == 'h') wake_state = 1;
      assert(fetch_result->data_size == fetch_chunksize * sizeof(int16_t));
      feed_count -= fetch_chunksize;

      if (wake_state == 0 && !can_sleep) {
        // TODO: Unify this with VAD? (See below)
        if (fetch_result->data_volume < -40) {
          below_sleep_threshold_count++;
          if (below_sleep_threshold_count >= 1 * 16000 / fetch_chunksize)
            can_sleep = true;
        } else {
          below_sleep_threshold_count = 0;
        }
      }

      if (wake_state != 0 && !audio_speech_ended()) {
        // Save to buffer
        int copy_count = min(
          SPEECH_BUFFER_SIZE - speech_buffer_ptr,
          fetch_result->data_size / sizeof(int16_t)
        );
        memcpy(speech_buffer + speech_buffer_ptr, fetch_result->data, copy_count * sizeof(int16_t));
        speech_buffer_ptr += copy_count;
        // TODO: Unify this with VAD? (See above)
        if (speech_buffer_ptr >= 3 * 16000 && fetch_result->data_volume < -40) {
          if (++below_speech_threshold_count >= 16000 / fetch_chunksize) {
            speech_ended_by_threshold = true;
            ESP_LOGI(TAG, "No more speech, wrapping up!");
          }
        } else {
          below_speech_threshold_count = 0;
        }
        if (audio_speech_ended()) {
          led_set_state(LED_STATE_WAIT_RESPONSE, 1500);
          ESP_LOGI(TAG, "Pausing audio processing, as we wait for a run");
          // Directly raise the flag, since `audio_pause()` results in a deadlock
          xTaskNotifyIndexed(audio_task_handle, 0, 0, eNoAction);
        }
      }
    }
  }

  while (1) {
    if (xTaskNotifyWaitIndexed(/* index */ 0, 0, 0, NULL, 0)) {
      if (!atomic_flag_test_and_set(&request_to_disable)) {
        ESP_ERROR_CHECK(i2s_disable());
        vTaskSuspend(audio_task_handle);
      }
      if (external_push_buf != 0) {
        ESP_LOGI(TAG, "External push %p %zu %zu", external_push_buf, external_push_start, external_push_size);
        external_push_buf = 0;
      }
    }
    if (!i2s_channel_paused) {
      if (i2s_read(buf32, &n, buf_count) == ESP_OK) {
        if (n == buf_count) {
          n = 0;
          process_audio_block();
        }
      } else {
        vTaskDelay(1);
        continue;
      }
    }
  }
}

// We use a unified notification for both external data push and pause.
// Performance-wise, this results in a very negligible gain.

void audio_push(const uint32_t *buf, size_t size, size_t start, size_t count)
{
  external_push_buf = buf;
  external_push_size = size;
  external_push_start = start;
  external_push_count = count;
  xTaskNotifyIndexed(audio_task_handle, /* index */ 0, 0, eNoAction);
  while (external_push_buf != NULL) taskYIELD();
}

void audio_pause()
{
  // `vTaskSuspend()` followed by `i2s_disable()` hangs,
  // supposedly due to unresolved blocks
  // Here we raise a notification flag and wait for the task to suspend itself
  i2s_channel_paused = true;
  atomic_flag_clear(&request_to_disable);
  xTaskNotifyIndexed(audio_task_handle, /* index */ 0, 0, eNoAction);
  while (eTaskGetState(audio_task_handle) != eSuspended) taskYIELD();
}

void audio_resume()
{
  i2s_channel_paused = false;
  ESP_ERROR_CHECK(i2s_enable());
  vTaskResume(audio_task_handle);
}

bool audio_can_sleep()
{
  return can_sleep;
}

void audio_clear_can_sleep()
{
  below_sleep_threshold_count = 0;
  can_sleep = false;
}

int audio_wake_state()
{
  return wake_state;
}

void audio_clear_wake_state()
{
  xSemaphoreTake(buffer_mutex, portMAX_DELAY);
  wake_state = 0;
  speech_buffer_ptr = 0;
  xSemaphoreGive(buffer_mutex);
}

const int16_t *audio_speech_buffer()
{
  return speech_buffer;
}

int audio_speech_buffer_size()
{
  xSemaphoreTake(buffer_mutex, portMAX_DELAY);
  int ret = speech_buffer_ptr;
  xSemaphoreGive(buffer_mutex);
  return ret;
}

bool audio_speech_ended()
{
  return (speech_ended_by_threshold || speech_buffer_ptr >= SPEECH_BUFFER_SIZE);
}
