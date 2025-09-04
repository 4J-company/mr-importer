#include "mr-importer/importer.hpp"

#include "pch.hpp"

namespace mr {
inline namespace importer {
    Asset::Asset(const std::filesystem::path &path) {
      auto imported = import(path);
      if (!imported) {
        MR_ERROR("Asset import failed: {}", path.c_str());
        return;
      }

      *this = std::move(imported.value());
    }

    Shader::Shader(const std::filesystem::path &path) {
      auto compiled = compile(path);
      if (!compiled) {
        MR_ERROR("Shader compilation failed: {}", path.c_str());
        return;
      }

      *this = std::move(compiled.value());
    }
}
}
