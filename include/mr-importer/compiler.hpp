#pragma once

/**
 * \file compiler.hpp
 * \brief Public API for shader compilation.
 */

#include "def.hpp"
#include "assets.hpp"

namespace mr {
inline namespace importer {
  /**
   * \brief Compile a shader source to backend code.
   * \param path Filesystem path to the shader module (e.g. Slang file).
   * \return Compiled \ref Shader on success, or std::nullopt on failure.
   */
  std::optional<Shader> compile(const std::filesystem::path& path);
}
} // namespace mr
