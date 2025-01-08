#include "main.h"
#include "opus.h"

#include "esp_heap_caps.h"

static OpusEncoder *encoder = NULL;

static const int frame_size = 640; // 40 ms @ 16 kHz mono
static uint8_t out[1000];

void encode_init()
{
  encoder = heap_caps_malloc(opus_encoder_get_size(2), MALLOC_CAP_SPIRAM);
  int err = opus_encoder_init(encoder, 16000, 1, OPUS_APPLICATION_VOIP);
  if (err != OPUS_OK) {
    fprintf(stderr, "Failed to create encoder: %s\n", opus_strerror(err));
    return;
  }
}

void encode_restart()
{
  opus_encoder_ctl(encoder, OPUS_RESET_STATE);
}

void encode_push(const int16_t *buf, size_t size)
{
  int n = opus_encode(encoder, buf, frame_size, out, sizeof out);
}
