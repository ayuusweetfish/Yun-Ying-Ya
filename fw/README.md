To build Opus (libopus):
```
tar xf opus-1.5.2.tar.gz
mkdir opus_build
cd opus_build
export PATH=~/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin:$PATH
../opus-1.5.2/configure --host=xtensa-esp32s3-elf --disable-extra-programs --disable-doc --enable-fixed-point --disable-float-api CFLAGS='-mlongcalls'
make
```
