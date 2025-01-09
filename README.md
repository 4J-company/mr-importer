# Opinionated asset importer
## Dependencies:
- fastgltf
- meshoptimizer
- (?) `taskflow`/`work_contracts`/`...`

## TODO: (feat)
- Load textures (with samplers)
- Load materials

## TODO: (perf)
- Dont copy data in the `Extractor` phase as there shouldn't be any shared data between different primitives
- Handle uniqueness of buffers (as in manager)
- try `taskflow`
- try `work_contracts`
- Determine sequential/parallel execution based on asset size
- Determine whether `meshopt` routines are necessary based on mesh size

