#ifndef __manager_hpp_
#define __manager_hpp_

#include <memory>
#include <atomic>
#include <string>
#include <optional>
#include <unordered_map>
#include <map>

#include <efsw/efsw.hpp>

#include "def.hpp"
#include "resource.hpp"
#include "pipe/pipe.hpp"

namespace mr {

  struct UnnamedTag {};

  inline constexpr UnnamedTag unnamed;

  template <typename ResourceT> class ResourceManager {
    public:
      static_assert(Resource<ResourceT>,
                    "ResourceT does not satisfy Resource concept");

      struct HandleT : AtomicSharedHandle<ResourceT> {
        using AtomicSharedHandle<ResourceT>::AtomicSharedHandle;
        using AtomicSharedHandle<ResourceT>::operator=;
        operator bool() {
          return (bool)this->load();
        }
        auto operator->() {
          return this->load();
        }
      };

      struct Entry : AtomicWeakHandle<ResourceT> {
        PipeHandle<ResourceT> pipe;

        Entry(AtomicWeakHandle<ResourceT> &&other)
          : AtomicWeakHandle<ResourceT>(std::move(other)) {}

        using AtomicWeakHandle<ResourceT>::AtomicWeakHandle;
        using AtomicWeakHandle<ResourceT>::operator=;

        template <typename ...Ts>
          HandleT init(Ts &&...args) {
            if constexpr (sizeof...(Ts) > 1) {
              pipe = make_pipe_prototype<ResourceT, Ts...>().on(
                std::tuple<Ts...>(std::move(args)...),
                [this]() {
                  this->exchange(std::make_shared<ResourceT>(pipe->result()));
                }
              );
            } else {
              pipe = make_pipe_prototype<ResourceT, Ts...>().on(
                std::move(args)...,
                [this]() {
                  this->exchange(std::make_shared<ResourceT>(pipe->result()));
                }
              );
            }

            pipe->execute();
            return this->load().lock();
          }

        void update() {
          pipe->schedule();
        }
      };

      using ResourceMapT = std::map<std::string, Entry>;

      class UpdateListener;
      friend class UpdateListener;

      class UpdateListener : public efsw::FileWatchListener {
        public:
          void handleFileAction( efsw::WatchID watchid, const std::string& dir,
              const std::string& filename, efsw::Action action,
              std::string oldFilename ) override {
            auto &mgr = ResourceManager<ResourceT>::get();
            switch ( action ) {
              case efsw::Actions::Add:
                MR_INFO("DIR ({}) FILE ({}) has event Added", dir, filename);
                break;
              case efsw::Actions::Delete:
                {
                  auto entry_opt = mgr.find_entry(dir + filename);
                  if (entry_opt) {
                    ((Entry&)entry_opt.value()).update();
                    MR_ERROR("Loaded asset file was deleted: {}", filename);
                  }
                }
                break;
              case efsw::Actions::Modified:
                {
                  auto entry_opt = mgr.find_entry(dir + filename);
                  if (entry_opt) {
                    ((Entry&)entry_opt.value()).update();
                  }
                }
                break;
              case efsw::Actions::Moved:
                {
                  auto entry_opt = mgr.find_entry(dir + oldFilename);
                  if (entry_opt) {
                    ((Entry&)entry_opt.value()).update();
                  }
                }
                break;
              default:
                MR_INFO("Should never happen!");
            }
          }
      };

      // tmp singleton
      static ResourceManager &get() noexcept
      {
        static ResourceManager manager;
        return manager;
      }

      template <typename... Args>
      [[nodiscard]] HandleT create(std::fs::path absolute_path, Args &&...args)
      {
        watcher.addWatch(absolute_path, &listener, true);
        return _resources[absolute_path.string()].init(absolute_path, std::forward<Args>(args)...);
      }

      template <typename... Args>
      [[nodiscard]] HandleT create(std::string name, Args &&...args)
      {
        return _resources[std::move(name)].init(std::forward<Args>(args)...);
      }

      template <typename... Args> [[nodiscard]] HandleT create(UnnamedTag, Args &&...args)
      {
        static std::atomic<size_t> unnamed_id = 0;
        return create(std::to_string(unnamed_id++),
                      std::forward<Args>(args)...);
      }

      [[nodiscard]] HandleT find(const std::string &name)
      {
        if (auto it = _resources.find(name); it != _resources.end()) {
          return it->second.load().lock();
        }

        return nullptr;
      }

    private:
      ResourceManager() noexcept {
        _resources.reserve(64);
        watcher.watch();
      }

      ResourceMapT _resources;
      UpdateListener listener;
      efsw::FileWatcher watcher;

      [[nodiscard]] std::optional<std::reference_wrapper<Entry>> find_entry(const std::string &name)
      {
        if (auto it = _resources.find(name); it != _resources.end()) {
          return it->second;
        }
        return std::nullopt;
      }
  };

} // namespace mr

#endif // __manager_hpp_
