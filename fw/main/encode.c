#include "main.h"
#include "opus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>

#include "esp_heap_caps.h"

static const char *TAG = "Encoder";

static OpusEncoder *encoder = NULL;

#define OUT_BUF_SIZE 65536
static uint8_t *out_buf;

static void encode_task(void *_unused);

static TaskHandle_t encode_task_handle;
static SemaphoreHandle_t buffer_mutex;

void encode_init()
{
  encoder = heap_caps_malloc(opus_encoder_get_size(1), MALLOC_CAP_SPIRAM);
  int err = opus_encoder_init(encoder, 16000, 1, OPUS_APPLICATION_VOIP);
  if (err != OPUS_OK) {
    ESP_LOGE(TAG, "Failed to create encoder: %s\n", opus_strerror(err));
    return;
  }
  out_buf = heap_caps_malloc(OUT_BUF_SIZE * sizeof(uint8_t), MALLOC_CAP_SPIRAM);

  buffer_mutex = xSemaphoreCreateMutex(); assert(buffer_mutex != NULL);

  BaseType_t result = xTaskCreate(encode_task, "encode_task", 40 * 1024, NULL, 0, &encode_task_handle);
  if (result == errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY)
    ESP_LOGE(TAG, "errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY\n");
}

static uint32_t out_buf_ptr;
static const int16_t *in_buf;
static uint32_t in_buf_last, in_buf_end;

void encode_restart()
{
  xSemaphoreTake(buffer_mutex, portMAX_DELAY);
  opus_encoder_ctl(encoder, OPUS_RESET_STATE);
  in_buf = NULL;
  in_buf_last = in_buf_end = 0;
  out_buf_ptr = 0;
  xSemaphoreGive(buffer_mutex);
  vTaskResume(encode_task_handle);
}

void encode_push(const int16_t *buf, size_t end)
{
  xSemaphoreTake(buffer_mutex, portMAX_DELAY);
  in_buf = buf;
  in_buf_end = end;
  xSemaphoreGive(buffer_mutex);
  vTaskResume(encode_task_handle);
}

size_t encode_pull()
{
  xSemaphoreTake(buffer_mutex, portMAX_DELAY);
  uint32_t ret = out_buf_ptr;
  xSemaphoreGive(buffer_mutex);
  return ret;
}

void encode_wait_end()
{
  xSemaphoreTake(buffer_mutex, portMAX_DELAY);
  xSemaphoreGive(buffer_mutex);
}

const uint8_t *encode_buffer()
{
  return out_buf;
}

static void encode_task(void *_unused)
{
  while (1) {
    vTaskSuspend(NULL); // Suspend self
    xSemaphoreTake(buffer_mutex, portMAX_DELAY);
    while (in_buf_last + 160 < in_buf_end) {
      uint32_t frame_size;
      if (in_buf_end - in_buf_last >= 960) frame_size = 960;
      else if (in_buf_end - in_buf_last >= 640) frame_size = 640;
      else if (in_buf_end - in_buf_last >= 320) frame_size = 320;
      else frame_size = 160;
      // printf("Encode %u (from in buffer %u)", (unsigned)frame_size, (unsigned)in_buf_last);
      int n = opus_encode(encoder,
        in_buf + in_buf_last, frame_size,
        out_buf + out_buf_ptr + 1, OUT_BUF_SIZE - out_buf_ptr);
      // printf(" - %d bytes\n", n);
      if (n < 0) {
        ESP_LOGE(TAG, "Encoder error: %d\n", n);
      } else {
        if (n <= 240) {
          out_buf[out_buf_ptr + 0] = n;
          out_buf_ptr += n + 1;
        } else {
          memmove(out_buf + out_buf_ptr + 2, out_buf + out_buf_ptr + 1, n);
          out_buf[out_buf_ptr + 0] = 240 + (n >> 8);
          out_buf[out_buf_ptr + 1] = n & 0xff;
          out_buf_ptr += n + 2;
        }
      }
      in_buf_last += frame_size;
    }
    xSemaphoreGive(buffer_mutex);
  }
}
