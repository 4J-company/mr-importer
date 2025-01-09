#ifndef __manager_hpp_
#define __manager_hpp_

#include <memory>
#include <atomic>
#include <string>
#include <unordered_map>

#include "resource.hpp"

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
      };

      struct Entry : AtomicWeakHandle<ResourceT> {
        ResourceT *res; // default-constructible wrapper

        template <typename ...Ts>
          requires std::is_constructible_v<ResourceT, Ts...>
          HandleT reinit(Ts &&...args) {
            // *this->load().lock() = import_pipeline.execute();

            this->exchange(std::make_shared<ResourceT>(std::forward<Ts>(args)...));

            return this->load().lock();
            /*
            if (not this->load()) {
              auto shared = this->load().lock();
              shared = std::make_shared<ResourceT>(std::forward<Ts>(args)...);
              return shared;
            }
            else {
              auto shared = this->load().lock();
              *shared = ResourceT(std::forward<Ts>(args)...);
              return shared;
            }
            */
          }

        using AtomicWeakHandle<ResourceT>::AtomicWeakHandle;
        using AtomicWeakHandle<ResourceT>::operator=;
      };

      using ResourceMapT =
        std::unordered_map<std::string, Entry>;

      // tmp singleton
      static ResourceManager &get() noexcept
      {
        static ResourceManager manager;
        return manager;
      }

      template <typename... Args>
      [[nodiscard]] HandleT create(std::string name, Args &&...args)
      {
        /*
           HandleT resource = std::make_shared<ResourceT>(std::forward<Args>(args)...);
           _resources[std::move(name)] = resource;
           return resource;
        */
        return _resources[std::move(name)].reinit(std::forward<Args>(args)...);
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
      ResourceManager() noexcept { _resources.reserve(64); }

      ResourceMapT _resources;
  };

} // namespace mr

#endif // __manager_hpp_
