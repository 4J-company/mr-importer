# Opinionated asset importer

## Features
- GLTF import and mesh optimization
- Automatic LOD generation

## Build
```bash
git clone https://github.com/4j-company/mr-importer
cd mr-importer
conan build . -b missing
```

## TODO
- Features:
    - Hot reloading (using efsw)
    - Performance statistics (?)
    - Hierarchical LOD (Batched Multi-Triangulation)
- Performance:
    - Try to give out `std::span` instead of `std::vector` for positions, indices, etc.
    - Async loading (using coroutines)
