#pragma once

#include <efsw/efsw.hpp>

#include "def.hpp"
#include "log.hpp"
#include "manager/manager.hpp"

namespace mr {
  struct AssetSystem {
    private:
      enum struct EntryType {
        eImage,
        eTexCoord,
        eSampler,
        eMesh,
        eMaterial,
        eTexture,
        eModel,
        eDirectory,
        eUnknown
      };

      inline static AssetType path2enum(const std::fs::path &p) {
        if (0
            || p.extension() == ".gltf"
            || p.extension() == ".glb"
            ) {
          return EntryType::eModel;
        }
        else if (0
            || p.extension() == ".png"
            || p.extension() == ".jpeg"
            || p.extension() == ".jpg"
            ) {
          return EntryType::eImage;
        }
        else if (is_directory(p)) {
          return EntryType::eDirectory;
        }
        else {
          return EntryType::eUnknown;
        }
      }

      struct UpdateListener : efsw::FileWatchListener {
        void handleFileAction( efsw::WatchID watchid, const std::string& dir,
            const std::string& filename, efsw::Action action,
            std::string oldFilename ) override {
          static AssetSystem & asset_system = AssetSystem::get();
          switch (path2enum(dir + filename)) {
            default:
              break;
          }
          switch ( action ) {
            case efsw::Actions::Add:
              MR_INFO("DIR ({}) FILE ({}) has event Added", dir, filename);
              break;
            case efsw::Actions::Delete:
              break;
            case efsw::Actions::Modified:
              MR_INFO("DIR ({}) FILE ({}) has event Modified", dir, filename);
              break;
            case efsw::Actions::Moved:
              MR_INFO("DIR ({}) FILE ({}) has event Moved from ({})", dir, filename, oldFilename);
              break;
            default:
              MR_INFO("Should never happen!");
          }
        }
      };

      UpdateListener listener;
      efsw::FileWatcher watcher;

      AssetSystem() {
        watcher.watch();
      }

    public:
      ResourceManager<Image>    &image_manager    = ResourceManager<Image>::get();
      ResourceManager<TexCoord> &texcoord_manager = ResourceManager<TexCoord>::get();
      ResourceManager<Sampler>  &sampler_manager  = ResourceManager<Sampler>::get();

      ResourceManager<Mesh>     &mesh_manager     = ResourceManager<Mesh>::get();
      ResourceManager<Material> &material_manager = ResourceManager<Material>::get();

      ResourceManager<Texture>  &texture_manager = ResourceManager<Texture>::get();
      ResourceManager<Model>    &model_manager   = ResourceManager<Model>::get();

      inline static AssetSystem &get() noexcept {
        static AssetSystem system {};
        return system;
      }

      void add(std::fs::path p) noexcept {
        switch (path2enum(p)) {
          case EntryType::eModel:
            watcher.addWatch(p, &listener, true);
            MR_INFO("Added asset: {}", p.string());
            break;
          case EntryType::eImage:
            watcher.addWatch(p, &listener, true);
            MR_INFO("Added asset: {}", p.string());
            return create(p.string(), p);
          case EntryType::eDirectory:
            using std::filesystem::recursive_directory_iterator;
            for (const auto& entry : recursive_directory_iterator(p)) {
              if (std::fs::is_regular_file(entry)) {
                add(entry);
              }
            }
            MR_INFO("Added directory: {}", p.string());
            break;
          default:
            MR_ERROR("Unsupported asset format: {} (on {})", p.extension().string(), p.string());
            break;
        }
      }
  };
}
