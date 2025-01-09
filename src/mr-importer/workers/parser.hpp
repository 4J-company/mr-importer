#pragma once

#include "def.hpp"

namespace mr {
  struct Parser {
      static inline std::optional<fastgltf::Asset> addTask(std::fs::path path)
      {
        auto parse = [path]() -> std::optional<fastgltf::Asset> {
          fg::Parser parser;

          static constexpr auto gltfOptions =
            fg::Options::GenerateMeshIndices |
            fg::Options::DecomposeNodeMatrices |
            fg::Options::LoadExternalBuffers | fg::Options::LoadExternalImages |
            fg::Options::GenerateMeshIndices;

          auto file = fg::MappedGltfFile::FromPath(path);
          auto gltf =
            parser.loadGltf(file.get(), path.parent_path(), gltfOptions);

          if (gltf) {
            return std::move(gltf.get());
          }

          return std::nullopt;
        };

        return parse();
      }
  };
} // namespace mr
