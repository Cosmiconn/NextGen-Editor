# stb (vendored)

Single-header libraries by Sean Barrett, public domain / MIT (see `LICENSE`).
Taken unchanged from https://github.com/nothings/stb at commit
`f58f558c120e9b32c217290b80bad1a0729fbb2c`:

- `stb_image.h` v2.30 – decoding PNG, JPG, BMP, TGA, GIF, PSD
- `stb_image_write.h` v1.16 – writing PNG and JPG
- `stb_image_resize2.h` v2.14 – high-quality resizing

Used only by `src/core/ImageIO.cpp` (implementation in `src/core/StbImpl.cpp`).
