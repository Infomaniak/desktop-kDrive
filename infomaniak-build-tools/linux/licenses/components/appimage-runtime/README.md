# AppImage runtime

The AppImage file starts with the AppImage type 2 runtime, commit
[fedfad51acc2860b2916a1eef32ba4a500ff31ef](https://github.com/AppImage/type2-runtime/commit/fedfad51acc2860b2916a1eef32ba4a500ff31ef),
licensed under the MIT license (`LICENSE`). It statically links the libraries below.

| Library | Version | License | Notice |
|---|---|---|---|
| musl | 1.2.5 | MIT | `musl/COPYRIGHT` |
| libfuse | 3.15.0 | LGPL-2.1-only | `libfuse/LICENSE`, `libfuse/AUTHORS`, `../licenses/LGPL-2.1-only.txt` |
| squashfuse | 0.5.2 | BSD-2-Clause | `squashfuse/LICENSE` |
| zstd | 1.5.6 | BSD-3-Clause | `zstd/LICENSE` |
| zlib | 1.3.2 | Zlib | `zlib/LICENSE` |
| mimalloc | 2.1.7 | MIT | `mimalloc/LICENSE` |
