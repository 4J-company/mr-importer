/**
 * \file assets.cpp
 * \brief Inline helpers and constructors for runtime asset structures.
 */

#include "mr-importer/importer.hpp"

#include "pch.hpp"

namespace mr {
inline namespace importer {
/**
 * Construct an \ref Model by importing from a file path.
 * On failure, logs an error and leaves the instance default-initialized.
 */
Model::Model(const std::filesystem::path &path)
{
  auto imported = import(path);
  if (!imported) {
    MR_ERROR("Model import failed: {}", path.string());
    return;
  }

  *this = std::move(imported.value());
}

/**
 * Construct a \ref Shader by compiling a shader file.
 * On failure, logs an error and leaves the instance default-initialized.
 */
Shader::Shader(const std::filesystem::path &path)
{
  auto compiled = compile(path);
  if (!compiled || compiled->size() == 0) {
    MR_ERROR("Shader compilation failed: {}", path.string());
    return;
  }

  if (compiled->size() > 1) {
    MR_ERROR("Shader compilation yielded multiple binaries.\n"
             "This usually happens when shader contains multiple entry points.\n"
             "If this is expected behaviour call `mr::compile` explicitly");
    return;
  }

  *this = std::move(compiled.value().front());
}
} // namespace importer
} // namespace mr
