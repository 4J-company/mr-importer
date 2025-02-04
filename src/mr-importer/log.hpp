#ifndef __trace_hpp_
#define __trace_hpp_

#include <print>

namespace mr::detail::term_modifier {
  inline constexpr const char *identity = "";
  inline constexpr const char *reset = "\033[0m";
  inline constexpr const char *red = "\033[31m";
  inline constexpr const char *yellow = "\033[33m";
  inline constexpr const char *magenta = "\033[35m";
} // namespace mr::detail::term_modifier

#ifndef NDEBUG
#define MR_LOG(category, modifier, ...)                 \
  do {                                                  \
    std::print("{}{}: ", modifier, category);           \
    std::println(__VA_ARGS__);                          \
    std::print("{}", mr::detail::term_modifier::reset); \
  } while (false)
#else
#define MR_LOG(...) static_cast<void>(0)
#endif

#define MR_INFO(...) \
  MR_LOG("INFO", mr::detail::term_modifier::reset, __VA_ARGS__)
#define MR_DEBUG(...) \
  MR_LOG("DEBUG", mr::detail::term_modifier::magenta, __VA_ARGS__)
#define MR_WARNING(...) \
  MR_LOG("WARNING", mr::detail::term_modifier::yellow, __VA_ARGS__)
#define MR_ERROR(...) \
  MR_LOG("ERROR", mr::detail::term_modifier::red, __VA_ARGS__)
#define MR_FATAL(...) \
  MR_LOG("FATAL", mr::detail::term_modifier::red, __VA_ARGS__)

template <typename T>
constexpr auto type_name() {
  std::string_view name, prefix, suffix;

#ifdef __clang__
  name = __PRETTY_FUNCTION__;
  prefix = "auto mr::type_name() [T = ";
  suffix = "]";
#elif defined(__GNUC__)
  name = __PRETTY_FUNCTION__;
  prefix = "constexpr auto mr::type_name() [with T = ";
  suffix = "]";
#elif defined(_MSC_VER)
  name = __FUNCSIG__;
  prefix = "auto __cdecl mr::type_name<";
  suffix = ">(void)";
#endif

  name.remove_prefix(prefix.size());
  name.remove_suffix(suffix.size());
  return name;
}

#endif // __trace_hpp_
