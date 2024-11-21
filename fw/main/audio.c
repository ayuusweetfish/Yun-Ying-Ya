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
  char *nsnet_name = NULL;
  nsnet_name = esp_srmodel_filter(models, ESP_NSNET_PREFIX, NULL);
  printf("nsnet_name: %s\n", nsnet_name ? nsnet_name : "(null)");

  const esp_afe_sr_iface_t *afe_handle = &ESP_AFE_SR_HANDLE;
  afe_config_t afe_config = AFE_CONFIG_DEFAULT();
  afe_config.aec_init = false;
  afe_config.vad_init = false;
  afe_config.wakenet_init = false;
  afe_config.afe_ns_mode = NS_MODE_SSP;
  afe_config.afe_ns_model_name = nsnet_name;
  afe_config.pcm_config.total_ch_num = 1;
  afe_config.pcm_config.mic_num = 1;
  afe_config.pcm_config.ref_num = 0;

  afe_data = afe_handle->create_from_config(&afe_config);
  if (!afe_data) {
    printf("afe_data is null!\n");
    return;
  }

  xTaskCreate(audio_task, "audio_task", 8192, NULL, 5, NULL);
}

void audio_task(void *_unused)
{
  const esp_afe_sr_iface_t *afe_handle = &ESP_AFE_SR_HANDLE;

  int sample_per_ms = 16;
  int feed_chunksize = afe_handle->get_feed_chunksize(afe_data);
  int fetch_chunksize = afe_handle->get_fetch_chunksize(afe_data);
  int total_nch = afe_handle->get_total_channel_num(afe_data);
  int16_t *i2s_buff = (int16_t *) malloc(feed_chunksize * sizeof(int16_t) * total_nch * 8);
  assert(i2s_buff);
  ESP_LOGI(TAG, "feed task start, feed_chunksize = %d, total_nch = %d, fetch_chunksize = %d\n", feed_chunksize, total_nch, fetch_chunksize);

  ESP_ERROR_CHECK(i2s_enable());

  size_t buf_size = 16000;
  size_t n = 0;
  static int32_t r_buf[16000];
  while (1) {
    ESP_ERROR_CHECK(i2s_read(r_buf, &n, buf_size));
    if (n == buf_size) {
      ESP_LOGI(TAG, "Second!");
      n = 0;
    }

    afe_handle->feed(afe_data, i2s_buff);
    vTaskDelay((feed_chunksize / sample_per_ms) / portTICK_PERIOD_MS);
    // afe_fetch_result_t *fetch_result = afe_handle->fetch(afe_data);
  }
}
