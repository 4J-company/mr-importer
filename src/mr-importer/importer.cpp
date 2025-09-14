#include "mr-importer/importer.hpp"

#include "pch.hpp"

namespace mr {
inline namespace importer {
  /**
   * \brief High-level import entry point.
   *
   * Loads an asset from disk, optionally optimizes meshes, and returns the result.
   * \param path Path to a source asset (e.g. glTF file).
   * \param options Import behavior flags, see \ref Options.
   * \return Imported \ref Model or std::nullopt if loading failed.
   */
  std::optional<Model> import(const std::filesystem::path& path, uint32_t options)
  {
    std::optional<Model> asset = load(path);

    if (!asset) {
      return std::nullopt;
    }

    if (options & Options::OptimizeMeshes) {
      tbb::parallel_for_each(asset.value().meshes, [](Mesh &mesh) {
          mesh = mr::optimize(std::move(mesh));
        }
      );
    }

    return asset;
  }
} // namespace importer
} // namespace mr
