# mr-importer — Opinionated Asset Importer

High-level importer focused on pragmatic, real-time graphics needs. It loads glTF assets, optimizes meshes for GPU rendering, and generates multi-LOD geometry out-of-the-box.

## Features
- glTF 2.0 import (via fastgltf)
- Mesh optimization (meshoptimizer):
  - Vertex cache optimization
  - Overdraw reduction
  - Vertex fetch remapping
- Automatic multi-LOD generation (with shadow index buffers)
- Minimal PBR material data and texture loading (via google/wuffs)
- Simple shader compilation pipeline (via Slang → SPIR-V)

## Public API Overview
Core public headers live under `include/mr-importer`:

- `def.hpp`: Common forward declarations and external dependencies used by the API
- `assets.hpp`: Core data structures (`Model`, `Mesh`, `MaterialData`, `Shader`, etc.)
- `loader.hpp`: `std::optional<Model> load(std::filesystem::path)` — parse and convert source assets
- `optimizer.hpp`: `Mesh optimize(Mesh)` — GPU-friendly topology and LOD generation
- `compiler.hpp`: `std::optional<Shader> compile(const std::filesystem::path&)` — shader compilation
- `options.hpp`: Import behavior flags
- `importer.hpp`: `import(path, options)` — high-level one-call import that can run optimization

See inline Doxygen-style comments in headers and source for details.

## Prerequisites
- A modern C++20 compiler and standard library
- CMake (>= 3.20 recommended)
- Conan 2.x (for dependency setup) or system-installed dependencies

Third-party dependencies (managed via Conan in this repo):
- spnda/fastgltf (glTF parsing)
- zeux/meshoptimizer
- google/suffs
- glm
- Slang (shader compiler)
- mr-utils, mr-manager, mr-math (internal libraries)

## Build with Conan (recommended)
```bash
git clone https://github.com/4j-company/mr-importer
cd mr-importer

# Configure and build, fetching any missing deps
conan build . -b missing

# Or do an explicit configure + build with CMake if preferred
conan install . --output-folder=build --build=missing
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake
cmake --build build --config Release
```

## Build with plain CMake (advanced)
If you prefer not to use Conan, make sure the dependencies listed above are discoverable by CMake (via your system package manager, custom toolchains, or manual `find_package` setup), then:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Examples
Example sources live in `examples/`:
- `rendertest.cpp` — basic asset import and rendering harness (backends in `render_*.hpp`)
- `shadercompiletest.cpp` — minimal shader compilation example

Typical workflow:
```bash
# After configuring the project (see build sections above)
cmake --build build --config Release

# Run produced example binaries (paths depend on your generator/toolchain)
./build/examples/rendertest            # or the corresponding output dir
./build/examples/shadercompiletest
```

## Documentation
- API reference is inline (Doxygen-style) within public headers in `include/mr-importer` and implementations in `src/mr-importer`.
- Quick entry points:
  - `include/mr-importer/importer.hpp` — high-level `import` function
  - `include/mr-importer/loader.hpp` — `load` for low-level parsing
  - `include/mr-importer/optimizer.hpp` — `optimize` to build LODs
  - `include/mr-importer/compiler.hpp` — `compile` for shaders

If you maintain a Doxygen setup, generate docs with:
```bash
doxygen Doxyfile
```
(Adjust to your environment. A `Doxyfile` may need to be added to the repository.)

## Roadmap / Ideas
- Hot reloading (efsw)
- Performance statistics
- Hierarchical LOD (Batched Multi-Triangulation)
- Async loading (C++ coroutines)

## License
See `LICENSE` for details.
