// emcc -O2 -o duck_opus_decoder duck_opus_decoder.c -I opus-1.5.2/include opus_build/.libs/libopus.a -s INITIAL_MEMORY=128KB -s TOTAL_STACK=60KB -s MAXIMUM_MEMORY=128KB -s ALLOW_MEMORY_GROWTH=0 -s STANDALONE_WASM --no-entry
#include <opus.h>
#include <stdint.h>
#include <emscripten.h>

static OpusDecoder *decoder;

EMSCRIPTEN_KEEPALIVE
int init()
{
  int err;
  decoder = opus_decoder_create(16000, 1, &err);
  return err;
}

static uint8_t _in_buffer[4096];
static int16_t _out_buffer[960];

EMSCRIPTEN_KEEPALIVE void *in_buffer() { return _in_buffer; }
EMSCRIPTEN_KEEPALIVE void *out_buffer() { return _out_buffer; }

// Decode a packet
// `in_size`: number of bytes
// `out_size`: number of samples (int16's)
EMSCRIPTEN_KEEPALIVE
int decode(int in_size)
{
  int n_decoded = opus_decode(
    decoder, _in_buffer, in_size,
    _out_buffer, sizeof _out_buffer / sizeof(int16_t),
    0 /* in-band forward error correction */);
  return n_decoded;
}
