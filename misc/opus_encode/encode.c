// mkdir cmake_build_host; cd cmake_build_host
// ../opus-1.5.2/configure --disable-extra-programs --disable-doc --disable-rtcd --disable-intrinsics --disable-stack-protector CFLAGS=-O2
// make

// cc encode.c -I opus-1.5.2/include opus_build_host/.libs/libopus.a -lm
// ffmpeg -i $NAME.mp3 -ar 16000 -f s16le -acodec pcm_s16le - | ./a.out >/dev/null
#include <stdio.h>
#include <opus.h>

int main() {
  int err;
  OpusEncoder *encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, &err);
  if (err != OPUS_OK) {
    fprintf(stderr, "Failed to create encoder: %s\n", opus_strerror(err));
    return 1;
  }

  enum { frame_size = 640 };  // 40 ms @ 16 kHz mono
  static int16_t pcm[frame_size];
  static uint8_t out[4000];

  int n_frames = 0;
  int n_bytes = 0;
  int max_packet = 0;

  while (1) {
    int r = fread(pcm, sizeof(uint16_t), frame_size, stdin);
    int n = opus_encode(encoder, pcm, frame_size, out, sizeof(out));
    if (n < 0) { fprintf(stderr, "Encoder error: %s\n", opus_strerror(n)); break; }
    if (n <= 240) {
      fwrite((uint8_t []){ n }, sizeof(uint8_t), 1, stdout);
      n_bytes += 1;
    } else {
      // assert(((n + 15) >> 8) <= 15);
      fwrite((uint8_t []){ 240 + ((n + 15) >> 8), (n + 15) & 0xff }, sizeof(uint8_t), 2, stdout);
      n_bytes += 2;
    }
    fwrite(out, sizeof(uint8_t), n, stdout);
    n_frames += 1; n_bytes += n; if (max_packet < n) max_packet = n;
    if (feof(stdin) || ferror(stdin) || ferror(stdout)) break;
  }
  fprintf(stderr, "Frames: %d\nMax. packet: %d\nTotal size: %d (incl. packet overhead)\n",
    n_frames, max_packet, n_bytes);

  opus_encoder_destroy(encoder);
  return 0;
}
