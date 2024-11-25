#include "main.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "model_path.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_models.h"
#include "dl_lib_convq_queue.h"

#if (CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4)
#include "esp_nsn_models.h"
#include "esp_nsn_iface.h"
#endif

static const char *TAG = "Audio";

static esp_afe_sr_data_t *afe_data = NULL;

static void audio_task(void *_unused);

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

  xTaskCreate(audio_task, "audio_task", 16384, NULL, 5, NULL);
}

void audio_task(void *_unused)
{
  const esp_afe_sr_iface_t *afe_handle = &ESP_AFE_SR_HANDLE;

  int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
  int fetch_chunksize = afe_handle->get_fetch_chunksize(afe_data);
  int total_nch = afe_handle->get_total_channel_num(afe_data);
  assert(total_nch == 1);
  ESP_LOGI(TAG, "feed task start, feed_chunksize = %d, total_nch = %d, fetch_chunksize = %d", feed_chunksize, total_nch, fetch_chunksize);

  ESP_ERROR_CHECK(i2s_enable());

  size_t buf_count = feed_chunksize * total_nch;
  size_t n = 0;
  int32_t *buf32 = malloc(sizeof(int32_t) * buf_count);
  int16_t *buf16 = malloc(sizeof(int16_t) * buf_count);
  int feed_count = 0;
  while (1) {
    ESP_ERROR_CHECK(i2s_read(buf32, &n, buf_count));
    if (n == buf_count) {
      // ESP-SR calls for 16-bit samples, convert here
      for (int i = 0; i < buf_count; i++) buf16[i] = buf32[i] >> 16;
      afe_handle->feed(afe_data, buf16);
      n = 0;
      feed_count += buf_count;
    }

    if (feed_count >= fetch_chunksize) {
      afe_fetch_result_t *fetch_result = afe_handle->fetch(afe_data);
      static int n = 0;
      if ((n = (n + 1) % 16) == 15)
        ESP_LOGI(TAG, "fetch data size = %d, volume = %.5f, wakeup_state = %d", fetch_result->data_size, fetch_result->data_volume, (int)fetch_result->wakeup_state);
      if (fetch_result->wakeup_state == WAKENET_DETECTED)
        ESP_LOGI(TAG, "Wake up word detected");
      assert(fetch_result->data_size == fetch_chunksize * sizeof(int16_t));
      feed_count -= fetch_chunksize;
    }
  }
}
