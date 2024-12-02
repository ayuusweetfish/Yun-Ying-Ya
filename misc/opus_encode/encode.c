// cc encode.c -I opus-1.5.2/include opus-1.5.2/.libs/libopus.a
#include <stdio.h>
#include <opus.h>

int main() {
  int err;
  OpusEncoder *encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, &err);
  if (err != OPUS_OK) {
    fprintf(stderr, "Failed to create encoder: %s\n", opus_strerror(err));
    return 1;
  }

  const int frame_size = 640; // 40 ms @ 16 kHz mono
  static int16_t pcm[frame_size];
  static uint8_t out[4000];

  int n_frames = 0;
  int n_bytes = 0;

  while (1) {
    int r = fread(pcm, sizeof(uint16_t), frame_size, stdin);
    int n = opus_encode(encoder, pcm, frame_size, out, sizeof(out));
    if (n < 0) { fprintf(stderr, "Encoder error: %s\n", opus_strerror(n)); break; }
    fwrite((uint8_t []){ n & 0xff, (n >> 8) & 0xff }, sizeof(uint8_t), 2, stdout);
    fwrite(out, sizeof(uint8_t), n, stdout);
    n_frames += 1; n_bytes += n;
    if (feof(stdin) || ferror(stdin) || ferror(stdout)) break;
  }
  fprintf(stderr, "Frames: %d\nTotal size: %d (%d incl. packet overhead)\n",
    n_frames, n_bytes, n_bytes + n_frames * 2);

  opus_encoder_destroy(encoder);
  return 0;
}
