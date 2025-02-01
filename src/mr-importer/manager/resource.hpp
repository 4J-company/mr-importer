#ifndef __resource_hpp_
#define __resource_hpp_

#include <memory>

namespace mr {

  template <typename ResourceT> class ResourceBase;

  template <typename T>
  concept Resource = std::derived_from<T, ResourceBase<T>>;

  template <typename T>
  struct AtomicHandle : std::atomic<T> {
    AtomicHandle() = default;
    AtomicHandle(const AtomicHandle &other) noexcept {
      this->store(other.load());
    }
    AtomicHandle& operator=(const AtomicHandle &other) noexcept {
      this->store(other.load());
    }
    AtomicHandle(AtomicHandle &&other) noexcept {
      this->store(std::move(other.load()));
    }
    AtomicHandle& operator=(AtomicHandle &&other) noexcept {
      this->store(std::move(other.load()));
      return *this;
    }
    using std::atomic<T>::atomic;
    using std::atomic<T>::operator=;
  };

  template <Resource T> using AtomicSharedHandle = AtomicHandle<std::shared_ptr<T>>;
  template <Resource T> using AtomicWeakHandle = AtomicHandle<std::weak_ptr<T>>;

  template <typename ResourceT> class ResourceManager;

  template <typename ResourceT>
  class ResourceBase : public std::enable_shared_from_this<ResourceT> {
    protected:
      ResourceBase() = default;

      friend class ResourceManager<ResourceT>;
  };

} // namespace mr

#define MR_DECLARE_HANDLE(ResourceT) \
  using ResourceT##Handle = AtomicSharedHandle<ResourceT>;

#endif // __resource_hpp_
