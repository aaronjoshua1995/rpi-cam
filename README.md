# rpi-cam


Prerequisites (already installed by user):
- `libgstreamer1.0-dev` and `gstreamer1.0-tools`
- `pkg-config`, `cmake`, and a C++ toolchain (`build-essential` / `gcc`/`clang`)

Build:

```bash
cmake -S . -B build
cmake --build build
```

Run:

```bash
./build/app
```

Notes:
- This project uses `pkg-config` via CMake to find `gstreamer-1.0`.
- If configuration fails, ensure `pkg-config --modversion gstreamer-1.0` returns a version.