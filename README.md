# Farfocel Game Engine

A C++23 game engine. Minimal, data-oriented, funky, farfocel!

Authors: [Kiju](https://github.com/kijudev), [Tfoedy](https://github.com/Tfoedy), [Stanisław Dera](https://github.com/stanislawdera)

---

## Modules

- **[engine](engine/README.md)** - the engine.
- **[asscooker](asscooker/README.md)** - asset pipeline.
- **[examples](examples/README.md)** - demo programs covering core, data, renderer and more
- **[tests](tests/README.md)** - unit tests using doctest
- **[benchmarks](benchmarks/README.md)** - microbenchmarks using nanobench
- **[docs](docs/README.md)** - Doxygen HTML documentation.

---

## Third-party libraries

See [3rdparty/README.md](3rdparty/README.md) for details.

- SDL3
- glad
- glm
- cgltf
- stb
- bc7enc
- mikktspace
- imgui
- imguizmo
- doctest
- nanobench
- wyhash
- yyjson

---

## Build

**Requirements:** C++23 compiler (GCC or Clang), CMake 3.21+, Ninja.

On Linux you also need the SDL3 system dependencies (Wayland, X11, Vulkan, ALSA/PipeWire, etc.). On NixOS use the provided flake - `nix develop` drops you into a ready shell.

### Presets

| Preset    | Description                              |
|-----------|------------------------------------------|
| `debug`   | Debug + ASan + UBSan                     |
| `release` | Optimized release                        |
| `msan`    | Debug + MSan + UBSan (Clang only)        |
| `tsan`    | Debug + TSan                             |

```sh
# configure + build
cmake --preset debug
cmake --build --preset debug -j

# or without a preset (plain build dir)
cmake -B build
cmake --build build -j

# docs (requires Doxygen)
cmake --build build --target docs
```

### Sanitizer flags (manual)

```sh
cmake --preset debug   # ASan + UBSan on by default
cmake --preset msan    # MSan + UBSan (Clang only)
cmake --preset tsan    # TSan
```

