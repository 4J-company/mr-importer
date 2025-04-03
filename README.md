# Opinionated asset importer
## Dependencies:
- GLTF parser: [fastgltf](https://github.com/spnda/fastgltf)
- Asset Optimizer: [meshoptimizer](https://github.com/zeux/meshoptimizer)
- Job System: [mr-contractor](https://github.com/4j-company/mr-contractor)

Note that these are downloaded in the CMake script via [CPM](https://github.com/cpm-cmake/CPM.cmake)

## TODO: (feat)
- Load textures (with samplers)
- Load materials

## TODO: (perf)
- Dont copy data in the `Extractor` phase as there shouldn't be any shared data between different primitives
- Handle uniqueness of buffers (as in manager)
- Determine sequential/parallel execution based on asset size
- Determine whether `meshopt` routines are necessary based on mesh size

