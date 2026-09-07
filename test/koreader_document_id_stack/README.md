# Document ID scratch-buffer tests

This suite compiles the real `KOReaderDocumentId.cpp` and `Memory.h`. The test
overrides only nothrow array allocation to inject a failed 1024-byte scratch
allocation. A memory-backed HAL fixture supplies file content, while a host
`MD5Builder` adapter uses OpenSSL to compute an independent digest.

Install the OpenSSL development package before configuring the host tests
(`libssl-dev` on Debian/Ubuntu). Linux and CI require it; other hosts skip this
suite with a status message if OpenSSL cannot be found. The firmware gains no
dependency.

```sh
cmake -S test -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/test --target KOReaderDocumentIdStackTest
ctest --test-dir build/test -R '^DocumentIdStack\.' --output-on-failure
```

Fixture bytes follow `(position * 37 + position / 251) % 256`. Expected digests
were calculated with Python `hashlib.md5` over the concatenated official samples:
offset 0, followed by `1024 << (2 * i)` for `i = 0..10`, up to 1024 bytes per
offset. The last sample is limited by the advertised file size.

This refactor changes scratch-buffer ownership only. Existing short-read,
negative-read and failed-seek behavior is outside its scope and requires a
separate correction; these tests do not endorse incomplete hashes or unsafe
length conversions. The host adapter refuses oversized hash input without
reading outside the fixture buffer.

Host tests verify the checked allocation and unchanged hashes. Firmware stack
measurements and physical-device testing remain separate checks; moving scratch
storage to the heap does not reduce total RAM by 1024 bytes or prove an existing
stack overflow.
