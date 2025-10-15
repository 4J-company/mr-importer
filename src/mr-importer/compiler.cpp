/**
 * \file compiler.cpp
 * \brief Slang-based shader compilation pipeline implementation.
 */

#include "mr-importer/importer.hpp"

#include "pch.hpp"

namespace mr {
inline namespace importer {
namespace {

  /**
   * Create or reuse a thread-local Slang session configured for SPIR-V output.
   *
   * Returns a cached session on subsequent calls. Configures target, options,
   * and search paths once per thread. Does not report errors; assumes creation
   * succeeds and returns an empty session only if Slang fails unexpectedly.
   */
  static Slang::ComPtr<slang::ISession> get_or_create_session() {
    static thread_local Slang::ComPtr<slang::ISession> session;
    if (session) {
      return session;
    }
  
    static thread_local Slang::ComPtr<slang::IGlobalSession> global_session;
    slang::createGlobalSession(global_session.writeRef());
  
    static const slang::TargetDesc target_desc {
      .format = SLANG_SPIRV,
      .profile = global_session->findProfile("spirv_1_5"),
    };
    static constexpr std::array options {
      slang::CompilerOptionEntry {
        slang::CompilerOptionName::EmitSpirvDirectly,
        {slang::CompilerOptionValueKind::Int, 1}
      },
      slang::CompilerOptionEntry {
        slang::CompilerOptionName::UseUpToDateBinaryModule,
        {slang::CompilerOptionValueKind::Int, 1}
      },
    };
    static constexpr std::array<const char *, 1> search_paths {
      "/home/michael/Development/Personal/mr-importer-rewrite/bin/shaders",
    };
    static constexpr slang::SessionDesc session_desc {
      .targets = &target_desc,
      .targetCount = 1,
      .searchPaths = search_paths.data(),
      .searchPathCount = search_paths.size(),
      .compilerOptionEntries = const_cast<slang::CompilerOptionEntry*>(options.data()), // If this shoots me in the foot - life is shit
      .compilerOptionEntryCount = options.size(),
    };
  
    global_session->createSession(session_desc, session.writeRef());
  
    return session;
  }
  
  /**
   * Load/compile a Slang module from the given path.
   *
   * On success returns a module; on failure returns an error blob containing
   * compiler diagnostics via std::unexpected.
   */
  static std::expected<Slang::ComPtr<slang::IModule>, Slang::ComPtr<slang::IBlob>> compile_module(slang::ISession* session, const std::filesystem::path &path) {
    Slang::ComPtr<slang::IModule> module;
    Slang::ComPtr<slang::IBlob> blob;
  
    const char *module_path = path.c_str();
    const char *module_name = path.stem().c_str();
  
    module = session->loadModule(module_path, blob.writeRef());
  
    if (blob) {
      return std::unexpected(blob);
    }
  
    return module;
  }
  
  /**
   * Find the entry point named "main" in the given module.
   *
   * Returns the entry point if it exists, otherwise std::nullopt.
   */
  static std::optional<Slang::ComPtr<slang::IEntryPoint>> locate_entry_point(slang::IModule *module) {
    Slang::ComPtr<slang::IEntryPoint> res;
    module->findEntryPointByName("main", res.writeRef());
    if (res) {
      return res;
    }
    return std::nullopt;
  }
  
  /**
   * Compose the module and its entry point into a component for linking.
   *
   * On success returns the composed component; otherwise returns diagnostics
   * blob via std::unexpected.
   */
  static std::expected<Slang::ComPtr<slang::IComponentType>, Slang::ComPtr<slang::IBlob>> compose_components(slang::ISession *session, slang::IModule *module, slang::IEntryPoint *entry) {
    Slang::ComPtr<slang::IBlob> blob;
    Slang::ComPtr<slang::IComponentType> composed;
    std::array components {
      (slang::IComponentType*)module,
      (slang::IComponentType*)entry
    };
  
    [[maybe_unused]] SlangResult res = session->createCompositeComponentType(
      components.data(),
      components.size(),
      composed.writeRef(),
      blob.writeRef()
    );
  
    if (blob) {
      return std::unexpected(blob);
    }
  
    return composed;
  }
  
  /**
   * Link the composed component into a final program.
   *
   * On success returns the linked component; otherwise returns diagnostics
   * blob via std::unexpected.
   */
  static std::expected<Slang::ComPtr<slang::IComponentType>, Slang::ComPtr<slang::IBlob>> link_program(slang::IComponentType *composed) {
    Slang::ComPtr<slang::IBlob> blob;
    Slang::ComPtr<slang::IComponentType> linked;
  
    [[maybe_unused]] SlangResult res = composed->link(
      linked.writeRef(),
      blob.writeRef()
    );
  
    if (blob) {
      return std::unexpected(blob);
    }
  
    return linked;
  }
  
  /**
   * Extract target code (SPIR-V) for entry point 0 from the linked program.
   *
   * On success returns a blob with compiled code; otherwise returns diagnostics
   * blob via std::unexpected.
   */
  static std::expected<Slang::ComPtr<slang::IBlob>, Slang::ComPtr<slang::IBlob>> get_target_code(slang::IComponentType *linked) {
    Slang::ComPtr<slang::IBlob> blob;
    Slang::ComPtr<slang::IBlob> code;
  
    [[maybe_unused]] SlangResult res = linked->getEntryPointCode(0, 0, code.writeRef(), blob.writeRef());
  
    if (blob) {
      return std::unexpected(blob);
    }
    return code;
  }
}

  /**
   * Compile a shader module located at \p path into a \ref Shader.
   * On any error during compilation, composition or linking, logs diagnostics
   * and returns std::nullopt.
   */
  std::optional<Shader> compile(const std::filesystem::path &path) {
    Slang::ComPtr<slang::ISession> session = get_or_create_session();

    Slang::ComPtr<slang::IModule> module;

    if (auto res = compile_module(session.get(), path); res.has_value()) {
      module = std::move(res.value());
    }
    else {
      MR_ERROR(" Failed to compile {}", path.string());
      MR_ERROR("\t\t{}", (char*)res.error()->getBufferPointer());
      return std::nullopt;
    }

    Slang::ComPtr<slang::IEntryPoint> entry_point;
    if (auto res = locate_entry_point(module.get()); res.has_value()) {
      entry_point = std::move(res.value());
    }
    else {
      MR_ERROR(" Failed to locate entry point for shader {}", path.string());
      return std::nullopt;
    }

    Slang::ComPtr<slang::IComponentType> composed;
    if (auto res = compose_components(session.get(), module.get(), entry_point.get()); res.has_value()) {
      composed = std::move(res.value());
    }
    else {
      MR_ERROR(" Failed to compose a program {}", path.string());
      MR_ERROR("\t\t{}", (char*)res.error()->getBufferPointer());
      return std::nullopt;
    }

    Slang::ComPtr<slang::IComponentType> linked;
    if (auto res = link_program(composed); res.has_value()) {
      linked = std::move(res.value());
    }
    else {
      MR_ERROR(" Failed to link a program {}", path.string());
      MR_ERROR("\t\t{}", (char*)res.error()->getBufferPointer());
      return std::nullopt;
    }

    Shader shader;
    if (auto res = get_target_code(linked); res.has_value()) {
      auto comptr = std::move(res.value());
      auto ptr = comptr.detach();
      
      shader.spirv.reset((std::byte*)ptr->getBufferPointer());
      shader.spirv.size(ptr->getBufferSize());
    }
    else {
      MR_ERROR(" Failed to get target code from a program {}", path.string());
      MR_ERROR("\t\t{}", (char*)res.error()->getBufferPointer());
      return std::nullopt;
    }

    return shader;
  }
}
}
