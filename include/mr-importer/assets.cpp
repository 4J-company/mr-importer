#include "importer.hpp"
#include "assets.hpp"

namespace mr {
  inline namespace importer {
    Asset::Asset(const std::filesystem::path &path) {
      *this = import(path);
    }

    Shader::Shader(const std::filesystem::path &path) {
      *this = compile(path).value();
    }
  }
}
