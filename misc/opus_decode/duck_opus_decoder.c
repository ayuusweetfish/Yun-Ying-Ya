// emcc -O2 -o duck_opus_decoder duck_opus_decoder.c -I opus-1.5.2/include opus_build/.libs/libopus.a -s INITIAL_MEMORY=256KB -s TOTAL_STACK=120KB -s MAXIMUM_MEMORY=256KB -s ALLOW_MEMORY_GROWTH=0 -s STANDALONE_WASM --no-entry
#include <opus.h>
#include <stdint.h>
#include <emscripten.h>

EMSCRIPTEN_KEEPALIVE
void *create()
{
  int err;
  OpusDecoder *decoder = opus_decoder_create(16000, 1, &err);
  if (err != OPUS_OK) return (void *)err;
  return decoder;
}

EMSCRIPTEN_KEEPALIVE
void destroy(void *_d)
{
  OpusDecoder *decoder = _d;
  opus_decoder_destroy(decoder);
}

// Decode a packet
// `in_size`: number of bytes
// `out_size`: number of samples (int16's)
EMSCRIPTEN_KEEPALIVE
int decode(void *_d, const uint8_t *in, int in_size, int16_t *out, int out_size)
{
  OpusDecoder *decoder = _d;
  int n_decoded = opus_decode(
    decoder, in, in_size,
    out, out_size,
    0 /* in-band forward error correction */);
  return n_decoded;
}
