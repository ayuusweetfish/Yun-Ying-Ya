To build Opus (libopus):
```
tar xf opus-1.5.2.tar.gz
cd opus-1.5.2
../configure --host=xtensa-esp32s3-elf --disable-extra-programs --disable-doc
make
```
