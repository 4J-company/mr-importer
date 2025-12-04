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
static Slang::ComPtr<slang::ISession> get_or_create_session()
{
  static thread_local Slang::ComPtr<slang::ISession> session;
  if (session) {
    return session;
  }

  static thread_local Slang::ComPtr<slang::IGlobalSession> global_session;
  slang::createGlobalSession(global_session.writeRef());

  static const slang::TargetDesc target_desc{
      .format = SLANG_SPIRV,
      .profile = global_session->findProfile("spirv_1_5"),
  };
  static constexpr std::array options{
      slang::CompilerOptionEntry{slang::CompilerOptionName::EmitSpirvDirectly,
                                 {slang::CompilerOptionValueKind::Int, 1}},
      slang::CompilerOptionEntry{
                                 slang::CompilerOptionName::UseUpToDateBinaryModule,
                                 {slang::CompilerOptionValueKind::Int, 1}},
  };
  static constexpr slang::SessionDesc session_desc{
      .targets = &target_desc,
      .targetCount = 1,
      .searchPaths = nullptr,
      .searchPathCount = 0,
      .compilerOptionEntries = const_cast<slang::CompilerOptionEntry *>(
          options.data()), // If this shoots me in the foot - life is shit
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
static std::expected<Slang::ComPtr<slang::IModule>, Slang::ComPtr<slang::IBlob>>
compile_module(slang::ISession *session, const std::filesystem::path &path)
{
  Slang::ComPtr<slang::IModule> module;
  Slang::ComPtr<slang::IBlob> blob;

  auto module_path_str = path.string();
  auto module_name_str = path.stem().string();

  const char *module_path = module_path_str.c_str();
  const char *module_name = module_name_str.c_str();

  module = session->loadModule(module_path, blob.writeRef());

  if (blob) {
    return std::unexpected(blob);
  }

  return module;
}

mr::Shader::Stage slang2importer(SlangStage stage) {
  switch (stage) {
    case SLANG_STAGE_VERTEX: return mr::Shader::Stage::Vertex; break;
    case SLANG_STAGE_HULL: return mr::Shader::Stage::Hull; break;
    case SLANG_STAGE_DOMAIN: return mr::Shader::Stage::Domain; break;
    case SLANG_STAGE_GEOMETRY: return mr::Shader::Stage::Geometry; break;
    case SLANG_STAGE_FRAGMENT: return mr::Shader::Stage::Fragment; break;
    case SLANG_STAGE_COMPUTE: return mr::Shader::Stage::Compute; break;
    case SLANG_STAGE_RAY_GENERATION: return mr::Shader::Stage::RayGeneration; break;
    case SLANG_STAGE_INTERSECTION: return mr::Shader::Stage::Intersection; break;
    case SLANG_STAGE_ANY_HIT: return mr::Shader::Stage::AnyHit; break;
    case SLANG_STAGE_CLOSEST_HIT: return mr::Shader::Stage::ClosestHit; break;
    case SLANG_STAGE_MISS: return mr::Shader::Stage::Miss; break;
    case SLANG_STAGE_CALLABLE: return mr::Shader::Stage::Callable; break;
    case SLANG_STAGE_MESH: return mr::Shader::Stage::Mesh; break;
    case SLANG_STAGE_AMPLIFICATION: return mr::Shader::Stage::Amplification; break;
    case SLANG_STAGE_DISPATCH: return mr::Shader::Stage::Dispatch; break;
  }
  PANIC("Unhandled SlangStage");
  return {};
}

/**
 * Find the entry point named "main" in the given module.
 *
 * Returns the entry point if it exists, otherwise std::nullopt.
 */
static std::optional<std::vector<std::pair<Slang::ComPtr<slang::IEntryPoint>, mr::Shader::Stage>>>
locate_entry_point(slang::IModule *module)
{
  std::vector<std::pair<Slang::ComPtr<slang::IEntryPoint>, mr::Shader::Stage>> res_vec;

  Slang::ComPtr<slang::IEntryPoint> res;
  Slang::ComPtr<slang::IBlob> blob;
  for (auto [stage, name] : std::vector<std::pair<SlangStage, std::string_view>>{
           {        SLANG_STAGE_VERTEX,        "vertex_main"},
           {          SLANG_STAGE_HULL,          "hull_main"},
           {        SLANG_STAGE_DOMAIN,        "domain_main"},
           {      SLANG_STAGE_GEOMETRY,      "geometry_main"},
           {      SLANG_STAGE_FRAGMENT,      "fragment_main"},
           {       SLANG_STAGE_COMPUTE,       "compute_main"},
           {SLANG_STAGE_RAY_GENERATION,    "generation_main"},
           {  SLANG_STAGE_INTERSECTION,  "intersection_main"},
           {       SLANG_STAGE_ANY_HIT,       "any_hit_main"},
           {   SLANG_STAGE_CLOSEST_HIT,   "closest_hit_main"},
           {          SLANG_STAGE_MISS,          "miss_main"},
           {      SLANG_STAGE_CALLABLE,      "callable_main"},
           {          SLANG_STAGE_MESH,          "mesh_main"},
           { SLANG_STAGE_AMPLIFICATION, "amplification_main"},
           {      SLANG_STAGE_DISPATCH,      "dispatch_main"}
  }) {
    module->findAndCheckEntryPoint(name.data(), stage, res.writeRef(), blob.writeRef());
    if (res) {
      res_vec.emplace_back(std::move(res), slang2importer(stage));
    }
  }

  if (res_vec.empty()) {
    return std::nullopt;
  }

  return res_vec;
}

/**
 * Compose the module and its entry point into a component for linking.
 *
 * On success returns the composed component; otherwise returns diagnostics
 * blob via std::unexpected.
 */
static std::expected<Slang::ComPtr<slang::IComponentType>,
    Slang::ComPtr<slang::IBlob>>
compose_components(
    slang::ISession *session, slang::IModule *module, slang::IEntryPoint *entry)
{
  Slang::ComPtr<slang::IBlob> blob;
  Slang::ComPtr<slang::IComponentType> composed;
  std::array components{
      (slang::IComponentType *)module, (slang::IComponentType *)entry};

  [[maybe_unused]] SlangResult res =
      session->createCompositeComponentType(components.data(),
          components.size(),
          composed.writeRef(),
          blob.writeRef());

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
static std::expected<Slang::ComPtr<slang::IComponentType>,
    Slang::ComPtr<slang::IBlob>>
link_program(slang::IComponentType *composed)
{
  Slang::ComPtr<slang::IBlob> blob;
  Slang::ComPtr<slang::IComponentType> linked;

  [[maybe_unused]] SlangResult res =
      composed->link(linked.writeRef(), blob.writeRef());

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
static std::expected<Slang::ComPtr<slang::IBlob>, Slang::ComPtr<slang::IBlob>>
get_target_code(slang::IComponentType *linked)
{
  Slang::ComPtr<slang::IBlob> blob;
  Slang::ComPtr<slang::IBlob> code;

  [[maybe_unused]] SlangResult res =
      linked->getEntryPointCode(0, 0, code.writeRef(), blob.writeRef());

  if (blob) {
    return std::unexpected(blob);
  }
  return code;
}
} // namespace

/**
 * Compile a shader module located at \p path into a \ref Shader.
 * On any error during compilation, composition or linking, logs diagnostics
 * and returns std::nullopt.
 */
std::optional<std::vector<Shader>> compile(const std::filesystem::path &path)
{
  Slang::ComPtr<slang::ISession> session = get_or_create_session();

  Slang::ComPtr<slang::IModule> module;

  if (auto res = compile_module(session.get(), path); res.has_value()) {
    module = std::move(res.value());
  }
  else {
    MR_ERROR(" Failed to compile {}", path.string());
    MR_ERROR("\t\t{}", (char *)res.error()->getBufferPointer());
    return std::nullopt;
  }

  std::vector<std::pair<Slang::ComPtr<slang::IEntryPoint>, mr::Shader::Stage>> entry_points;
  if (auto res = locate_entry_point(module.get()); res.has_value()) {
    entry_points = std::move(res.value());
  }
  else {
    MR_ERROR(" Failed to locate entry point for shader {}", path.string());
    return std::nullopt;
  }

  std::vector<std::pair<Slang::ComPtr<slang::IComponentType>, mr::Shader::Stage>> composeds;
  for (const auto &entry_point : entry_points) {
    if (auto res =
            compose_components(session.get(), module.get(), entry_point.first.get());
        res.has_value()) {
      composeds.emplace_back(std::move(res.value()), entry_point.second);
    }
    else {
      MR_ERROR(" Failed to compose a program {}", path.string());
      MR_ERROR("\t\t{}", (char *)res.error()->getBufferPointer());
    }
  }

  std::vector<std::pair<Slang::ComPtr<slang::IComponentType>, mr::Shader::Stage>> linkeds;
  for (const auto &composed : composeds) {
    if (auto res = link_program(composed.first); res.has_value()) {
      linkeds.emplace_back(std::move(res.value()), composed.second);
    }
    else {
      MR_ERROR(" Failed to link a program {}", path.string());
      MR_ERROR("\t\t{}", (char *)res.error()->getBufferPointer());
    }
  }

  std::vector<Shader> shaders;
  for (const auto &linked : linkeds) {
    if (auto res = get_target_code(linked.first); res.has_value()) {
      auto comptr = std::move(res.value());
      auto ptr = comptr.detach();

      auto &last = shaders.emplace_back();
      last.stage = linked.second;
      last.spirv.reset((std::byte *)ptr->getBufferPointer());
      last.spirv.size(ptr->getBufferSize());
    }
    else {
      MR_ERROR(" Failed to get target code from a program {}", path.string());
      MR_ERROR("\t\t{}", (char *)res.error()->getBufferPointer());
    }
  }

  return shaders;
}
} // namespace importer
} // namespace mr
