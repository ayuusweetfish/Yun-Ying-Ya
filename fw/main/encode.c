#include "main.h"
#include "opus.h"

#include "esp_heap_caps.h"

static OpusEncoder *encoder = NULL;

#define OUT_BUF_SIZE 2048
static uint8_t *out_buf;

void encode_init()
{
  encoder = heap_caps_malloc(opus_encoder_get_size(1), MALLOC_CAP_SPIRAM);
  int err = opus_encoder_init(encoder, 16000, 1, OPUS_APPLICATION_VOIP);
  if (err != OPUS_OK) {
    fprintf(stderr, "Failed to create encoder: %s\n", opus_strerror(err));
    return;
  }
  out_buf = heap_caps_malloc(OUT_BUF_SIZE * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
}

static uint32_t buf_last;

void encode_restart()
{
  opus_encoder_ctl(encoder, OPUS_RESET_STATE);
  buf_last = 0;
}

void encode_push(const int16_t *buf, size_t end, void (*callback)(const uint8_t *, size_t))
{
  uint32_t out_buf_size = 0;
  while (buf_last + 160 < end) {
    uint32_t frame_size;
    if (end - buf_last >= 960) frame_size = 960;
    else if (end - buf_last >= 640) frame_size = 640;
    else if (end - buf_last >= 320) frame_size = 320;
    else frame_size = 160;
    printf("Encode %u (%p + %u)\n", (unsigned)frame_size, buf, (unsigned)buf_last);
  re_encode:
    int n = opus_encode(encoder, buf + buf_last, frame_size, out_buf, OUT_BUF_SIZE - out_buf_size);
    printf("Encoded %d\n", n);
    if (n == OPUS_BUFFER_TOO_SMALL) {
      // Send buffer to output
      (*callback)(out_buf, out_buf_size);
      out_buf_size = 0;
      goto re_encode;
      // We ensure that `out_buf` contains one packet of maximum duration,
      // so `OPUS_BUFFER_TOO_SMALL` cannot happen twice successively
    }
    buf_last += frame_size;
  }
  if (out_buf_size > 0) (*callback)(out_buf, out_buf_size);
}
