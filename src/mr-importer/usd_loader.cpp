/**
 * \file usd_loader.cpp
 * \brief OpenUSD import into runtime asset structures.
 */

#include "mr-importer/importer.hpp"

#include "flowgraph.hpp"
#include "pch.hpp"

#include <pxr/base/plug/registry.h>
#include <pxr/base/gf/half.h>
#include <pxr/base/gf/math.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/type.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/common.h>
#include <pxr/usd/usd/primFlags.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/primvar.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdLux/cylinderLight.h>
#include <pxr/usd/usdLux/diskLight.h>
#include <pxr/usd/usdLux/distantLight.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdLux/rectLight.h>
#include <pxr/usd/usdLux/shapingAPI.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/tokens.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string_view>

namespace mr {
inline namespace importer {
namespace {

PXR_NAMESPACE_USING_DIRECTIVE

static void push_unique_path(std::vector<std::string> &paths, std::string const &p)
{
  if (p.empty()) {
    return;
  }
  for (std::string const &x : paths) {
    if (x == p) {
      return;
    }
  }
  paths.push_back(p);
}

static void append_usd_plugin_root(std::vector<std::string> &paths, std::filesystem::path const &root)
{
  if (root.empty()) {
    return;
  }
  auto const sdf_plug = root / "sdf" / "resources" / "plugInfo.json";
  if (std::filesystem::exists(sdf_plug)) {
    push_unique_path(paths, sdf_plug.string());
  }
  push_unique_path(paths, root.string());
}

static void ensure_usd_plugins_registered()
{
  static std::once_flag once;
  std::call_once(once, [] {
    std::vector<std::string> paths;
    // Runtime override (no rebuild): same layout as PXR install lib/usd.
    if (const char *env = std::getenv("MR_IMPORTER_USD_PLUGIN_ROOT")) {
      append_usd_plugin_root(paths, std::filesystem::path(env));
    }
#ifdef MR_IMPORTER_PXR_USD_PLUGIN_ROOT
    append_usd_plugin_root(paths, std::filesystem::path(MR_IMPORTER_PXR_USD_PLUGIN_ROOT));
#endif
    if (const char *env = std::getenv("PXR_PLUGINPATH")) {
#if defined(_WIN32)
      constexpr char kListSep = ';';
#else
      constexpr char kListSep = ':';
#endif
      std::string_view sv(env);
      while (!sv.empty()) {
        std::size_t const pos = sv.find(kListSep);
        std::string_view const part = (pos == std::string_view::npos) ? sv : sv.substr(0, pos);
        if (!part.empty()) {
          push_unique_path(paths, std::string(part.data(), part.size()));
        }
        sv = (pos == std::string_view::npos) ? std::string_view{} : sv.substr(pos + 1);
      }
    }
    if (!paths.empty()) {
      PlugRegistry::GetInstance().RegisterPlugins(paths);
    }
    else {
      MR_WARNING(
          "USD: no plugin search paths registered (Ar/Sdf resolvers e.g. Sdf_UsdzResolver may fail). "
          "Set MR_IMPORTER_USD_PLUGIN_ROOT or PXR_PLUGINPATH to your OpenUSD lib/usd directory, or "
          "configure CMake with MR_IMPORTER_PXR_USD_PLUGIN_ROOT.");
    }
  });
}

static Transform matr4f_from_gf_matrix(GfMatrix4d const &g)
{
  // GfMatrix4d uses row vectors (p_row * M); translation sits in the last row. glm and Polyscope use
  // column vectors (M * p_col). The same rigid transform requires the transpose when copying entries
  // into a glm::mat4 for column-major storage.
  glm::mat4 t{};
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      t[c][r] = static_cast<float>(g[c][r]);
    }
  }
  return Transform(t[0][0],
      t[1][0],
      t[2][0],
      t[3][0],
      t[0][1],
      t[1][1],
      t[2][1],
      t[3][1],
      t[0][2],
      t[1][2],
      t[2][2],
      t[3][2],
      t[0][3],
      t[1][3],
      t[2][3],
      t[3][3]);
}

static GfMatrix4d stage_up_axis_correction(UsdStageRefPtr const &stage)
{
  TfToken up = UsdGeomGetStageUpAxis(stage);
  if (up == UsdGeomTokens->z) {
    return GfMatrix4d().SetRotate(GfRotation(GfVec3d(1, 0, 0), 90.0));
  }
  return GfMatrix4d(1.0);
}

static void triangulate_faces_from_counts(int const *face_vertex_counts,
    size_t nfaces,
    int const *face_vertex_indices,
    std::vector<Index> &out)
{
  out.clear();
  int idx = 0;
  for (size_t fi = 0; fi < nfaces; ++fi) {
    int n = face_vertex_counts[fi];
    if (n < 3) {
      idx += n;
      continue;
    }
    int base = idx;
    if (n == 3) {
      out.push_back(static_cast<Index>(face_vertex_indices[idx]));
      out.push_back(static_cast<Index>(face_vertex_indices[idx + 1]));
      out.push_back(static_cast<Index>(face_vertex_indices[idx + 2]));
      idx += 3;
      continue;
    }
    if (n == 4) {
      Index i0 = static_cast<Index>(face_vertex_indices[base]);
      Index i1 = static_cast<Index>(face_vertex_indices[base + 1]);
      Index i2 = static_cast<Index>(face_vertex_indices[base + 2]);
      Index i3 = static_cast<Index>(face_vertex_indices[base + 3]);
      out.push_back(i0);
      out.push_back(i1);
      out.push_back(i2);
      out.push_back(i0);
      out.push_back(i2);
      out.push_back(i3);
      idx += 4;
      continue;
    }
    for (int t = 1; t < n - 1; ++t) {
      out.push_back(static_cast<Index>(face_vertex_indices[base]));
      out.push_back(static_cast<Index>(face_vertex_indices[base + t]));
      out.push_back(static_cast<Index>(face_vertex_indices[base + t + 1]));
    }
    idx += n;
  }
}

static Color gf_vec_to_color(GfVec3f const &v)
{
  return Color(v[0], v[1], v[2], 1.f);
}

static float lux_effective_intensity(float intensity, float exposure)
{
  return intensity * std::pow(2.0f, exposure);
}

static std::optional<TextureData> try_load_uv_texture(
    UsdShadeShader const &texShader, TextureType type, std::filesystem::path const &stage_dir,
    Options options)
{
  UsdShadeInput fileInput = texShader.GetInput(TfToken("file"));
  if (!fileInput) {
    return std::nullopt;
  }
  SdfAssetPath ap;
  if (!fileInput.GetAttr().Get(&ap)) {
    return std::nullopt;
  }
  std::string resolved = ap.GetResolvedPath();
  if (resolved.empty()) {
    resolved = ap.GetAssetPath();
  }
  if (resolved.empty()) {
    return std::nullopt;
  }
  std::filesystem::path img_path = resolved;
  if (img_path.is_relative()) {
    img_path = stage_dir / img_path;
  }
  auto image = load_image_from_file_path(img_path, options);
  if (!image.has_value()) {
    MR_WARNING("USD: failed to load texture {}", img_path.string());
    return std::nullopt;
  }
  return TextureData(std::move(image.value()), type, SamplerData{vk::Filter::eLinear, vk::Filter::eLinear},
      img_path.filename().string());
}

static void append_texture_from_input(UsdShadeShader const &preview,
    TfToken const &inputName,
    TextureType type,
    std::filesystem::path const &stage_dir,
    Options options,
    std::vector<TextureData> &out)
{
  UsdShadeInput input = preview.GetInput(inputName);
  if (!input) {
    return;
  }
  UsdShadeConnectableAPI source;
  TfToken sourceName;
  UsdShadeAttributeType attrType;
  if (!input.GetConnectedSource(&source, &sourceName, &attrType)) {
    return;
  }
  UsdPrim p = source.GetPrim();
  UsdShadeShader srcShader(p);
  TfToken id;
  if (!srcShader.GetIdAttr().Get(&id)) {
    return;
  }
  if (id == TfToken("UsdUVTexture")) {
    auto tex = try_load_uv_texture(srcShader, type, stage_dir, options);
    if (tex.has_value()) {
      out.emplace_back(std::move(tex.value()));
    }
  }
}

static MaterialData build_material_from_preview(
    UsdShadeShader const &preview, std::filesystem::path const &stage_dir, Options options)
{
  MaterialData m{};
  m.constants.base_color_factor = Color(1, 1, 1, 1);
  m.constants.emissive_color = Color(0, 0, 0, 1);
  m.constants.emissive_strength = 1.f;
  m.constants.normal_map_intensity = 1.f;
  m.constants.roughness_factor = 0.5f;
  m.constants.metallic_factor = 0.f;

  GfVec3f v3;
  GfVec4f v4;
  if (preview.GetInput(TfToken("diffuseColor")).GetAttr().Get(&v3)) {
    m.constants.base_color_factor = Color(v3[0], v3[1], v3[2], 1.f);
  }
  if (preview.GetInput(TfToken("emissiveColor")).GetAttr().Get(&v3)) {
    m.constants.emissive_color = Color(v3[0], v3[1], v3[2], 1.f);
  }
  float f = 0.f;
  if (preview.GetInput(TfToken("metallic")).GetAttr().Get(&f)) {
    m.constants.metallic_factor = f;
  }
  if (preview.GetInput(TfToken("roughness")).GetAttr().Get(&f)) {
    m.constants.roughness_factor = f;
  }

  if (is_enabled(options, Options::LoadMaterials)) {
    append_texture_from_input(
        preview, TfToken("diffuseColor"), TextureType::BaseColor, stage_dir, options, m.textures);
    append_texture_from_input(
        preview, TfToken("normal"), TextureType::NormalMap, stage_dir, options, m.textures);
    append_texture_from_input(preview,
        TfToken("metallic"),
        TextureType::RoughnessMetallic,
        stage_dir,
        options,
        m.textures);
    append_texture_from_input(preview,
        TfToken("roughness"),
        TextureType::RoughnessMetallic,
        stage_dir,
        options,
        m.textures);
    append_texture_from_input(
        preview, TfToken("emissiveColor"), TextureType::EmissiveColor, stage_dir, options, m.textures);
  }

  return m;
}

static std::optional<UsdShadeShader> find_preview_surface(UsdShadeMaterial const &mat)
{
  TfToken sourceName;
  UsdShadeAttributeType st = UsdShadeAttributeType::Invalid;
  UsdShadeShader surface = mat.ComputeSurfaceSource(
      UsdShadeTokens->universalRenderContext, &sourceName, &st);
  if (!surface.GetPrim().IsValid()) {
    return std::nullopt;
  }
  TfToken id;
  if (surface.GetIdAttr().Get(&id) && id == TfToken("UsdPreviewSurface")) {
    return surface;
  }
  for (UsdPrim const &child : mat.GetPrim().GetChildren()) {
    UsdShadeShader sh(child);
    if (!sh.GetPrim().IsValid()) {
      continue;
    }
    if (sh.GetIdAttr().Get(&id) && id == TfToken("UsdPreviewSurface")) {
      return sh;
    }
  }
  return std::nullopt;
}

struct MeshGeometryScratch {
  std::vector<Position> positions;
  std::vector<int> face_vertex_counts;
  std::vector<int> face_vertex_indices;
  std::vector<float> normals_xyz;
  std::vector<float> uv_xy;
};

struct MeshBuildItem {
  UsdGeomMesh mesh;
  GfMatrix4d world;
  std::string name;
  size_t material_index = 0;
  MeshGeometryScratch scratch;
  bool geom_ok = false;
};

/** True when UsdGeomImageable purpose is <tt>guide</tt> (collision / editor helpers). */
static bool usd_geom_mesh_is_guide_purpose(UsdGeomMesh const &gm)
{
  UsdGeomImageable const img(gm.GetPrim());
  if (!img) {
    return false;
  }
  TfToken purpose;
  if (!img.GetPurposeAttr().Get(&purpose, UsdTimeCode::Default())) {
    return false;
  }
  return purpose == UsdGeomTokens->guide;
}

static bool extract_usd_mesh_geometry(
    UsdGeomMesh const &geom, MeshGeometryScratch &scratch, Options options)
{
  ZoneScopedN("USD extract mesh");
  const UsdTimeCode time = UsdTimeCode::Default();
  VtVec3fArray points;
  bool const pts_ok = geom.GetPointsAttr().Get(&points, time);
  if (!pts_ok || points.empty()) {
    return false;
  }
  VtIntArray faceVertexCounts;
  VtIntArray faceVertexIndices;
  if (!geom.GetFaceVertexCountsAttr().Get(&faceVertexCounts, time) ||
      !geom.GetFaceVertexIndicesAttr().Get(&faceVertexIndices, time)) {
    return false;
  }

  scratch.positions.clear();
  scratch.positions.reserve(points.size());
  for (GfVec3f const &p : points) {
    scratch.positions.push_back(Position{p[0], p[1], p[2]});
  }
  scratch.face_vertex_counts.assign(
      faceVertexCounts.cdata(), faceVertexCounts.cdata() + faceVertexCounts.size());
  scratch.face_vertex_indices.assign(
      faceVertexIndices.cdata(), faceVertexIndices.cdata() + faceVertexIndices.size());

  if (scratch.face_vertex_counts.empty()) {
    return false;
  }
  long long corner_sum = 0;
  for (int c : scratch.face_vertex_counts) {
    if (c < 0) {
      return false;
    }
    corner_sum += c;
  }
  if (corner_sum != static_cast<long long>(scratch.face_vertex_indices.size())) {
    return false;
  }

  size_t const nv = scratch.positions.size();
  for (int vi : scratch.face_vertex_indices) {
    if (vi < 0 || static_cast<size_t>(vi) >= nv) {
      return false;
    }
  }

  scratch.normals_xyz.clear();
  scratch.uv_xy.clear();

  if (is_enabled(options, Options::LoadMeshAttributes)) {
    UsdGeomPrimvarsAPI pvapi(geom.GetPrim());
    UsdGeomPrimvar nvar = pvapi.GetPrimvar(TfToken("normals"));
    if (nvar.HasValue()) {
      VtVec3fArray normals;
      if (nvar.Get(&normals, time) && normals.size() == points.size()) {
        scratch.normals_xyz.resize(normals.size() * 3u);
        for (size_t i = 0; i < normals.size(); ++i) {
          scratch.normals_xyz[i * 3 + 0] = normals[i][0];
          scratch.normals_xyz[i * 3 + 1] = normals[i][1];
          scratch.normals_xyz[i * 3 + 2] = normals[i][2];
        }
      }
    }
    UsdGeomPrimvar uvvar = pvapi.GetPrimvar(TfToken("st"));
    if (!uvvar || !uvvar.HasValue()) {
      uvvar = pvapi.GetPrimvar(TfToken("uv"));
    }
    if (uvvar.HasValue()) {
      VtVec2fArray uvs;
      if (uvvar.Get(&uvs, time) && uvs.size() == points.size()) {
        scratch.uv_xy.resize(uvs.size() * 2u);
        for (size_t i = 0; i < uvs.size(); ++i) {
          scratch.uv_xy[i * 2 + 0] = uvs[i][0];
          scratch.uv_xy[i * 2 + 1] = uvs[i][1];
        }
      }
    }
  }
  return true;
}

static void finalize_mesh_from_scratch(
    MeshGeometryScratch const &scratch, Mesh &mesh, bool load_attributes, Options options)
{
  ZoneScopedN("USD finalize mesh");
  mesh.positions = scratch.positions;
  triangulate_faces_from_counts(scratch.face_vertex_counts.data(),
      scratch.face_vertex_counts.size(),
      scratch.face_vertex_indices.data(),
      mesh.indices);
  if (mesh.indices.empty()) {
    mesh = Mesh{};
    return;
  }

  mesh.lods.clear();
  // IndexArray owns triangle indices; LOD[0] only references the same storage.
  mesh.lods.emplace_back(IndexSpan(mesh.indices.data(), mesh.indices.size()), IndexSpan());

  if (load_attributes && is_enabled(options, Options::LoadMeshAttributes)) {
    size_t const n = mesh.positions.size();
    if (!scratch.normals_xyz.empty() && scratch.normals_xyz.size() == n * 3) {
      mesh.attributes.resize(n);
      mesh.attributes.is_normal_present = true;
      for (size_t i = 0; i < n; ++i) {
        mesh.attributes[i].normal = {scratch.normals_xyz[i * 3 + 0],
            scratch.normals_xyz[i * 3 + 1],
            scratch.normals_xyz[i * 3 + 2]};
      }
    }
    if (!scratch.uv_xy.empty() && scratch.uv_xy.size() == n * 2) {
      if (mesh.attributes.size() != n) {
        mesh.attributes.resize(n);
      }
      mesh.attributes.is_texcoord_present = true;
      for (size_t i = 0; i < n; ++i) {
        mesh.attributes[i].texcoord = {scratch.uv_xy[i * 2 + 0], scratch.uv_xy[i * 2 + 1]};
      }
    }
  }

  if (!mesh.positions.empty()) {
    auto min_pos = mesh.positions[0];
    auto max_pos = mesh.positions[0];
    for (const auto &pos : mesh.positions) {
      min_pos[0] = std::min(min_pos[0], pos[0]);
      min_pos[1] = std::min(min_pos[1], pos[1]);
      min_pos[2] = std::min(min_pos[2], pos[2]);
      max_pos[0] = std::max(max_pos[0], pos[0]);
      max_pos[1] = std::max(max_pos[1], pos[1]);
      max_pos[2] = std::max(max_pos[2], pos[2]);
    }
    mesh.aabb.min = {min_pos[0], min_pos[1], min_pos[2]};
    mesh.aabb.max = {max_pos[0], max_pos[1], max_pos[2]};
  }

  if (!mesh.positions.empty()) {
    mr::Vec3f c(0, 0, 0);
    for (auto const &p : mesh.positions) {
      c += mr::Vec3f(p[0], p[1], p[2]);
    }
    const float inv_n = 1.f / static_cast<float>(mesh.positions.size());
    c = mr::Vec3f(c.x() * inv_n, c.y() * inv_n, c.z() * inv_n);
    float r2 = 0.f;
    for (auto const &p : mesh.positions) {
      float dx = p[0] - c.x();
      float dy = p[1] - c.y();
      float dz = p[2] - c.z();
      r2 = std::max(r2, dx * dx + dy * dy + dz * dz);
    }
    mesh.bounding_sphere.center(c);
    mesh.bounding_sphere.radius(std::sqrt(r2));
  }
}

static std::filesystem::path resolve_usd_asset_path(std::filesystem::path const &path)
{
  std::error_code ec;
  std::filesystem::path const canonical = std::filesystem::weakly_canonical(path, ec);
  if (!ec) {
    return canonical;
  }
  ec.clear();
  std::filesystem::path const abs = std::filesystem::absolute(path, ec);
  if (!ec) {
    return abs;
  }
  return path;
}

static UsdStageRefPtr open_usd_stage_from_path(std::filesystem::path const &path)
{
  ZoneScopedN("open_usd_stage_from_path");
  ensure_usd_plugins_registered();
  // File-backed root + explicit resolver context so relative references,
  // sublayers, and nested payloads resolve against the asset directory.
  std::string const p = path.string();
  ArResolverContext const ctx = ArGetResolver().CreateDefaultContextForAsset(p);
  return UsdStage::Open(p, ctx);
}

static bool load_usd_into_model(Model &model, std::filesystem::path const &path, Options options)
{
  ZoneScopedN("load_usd_into_model");

  std::filesystem::path const asset_path = resolve_usd_asset_path(path);
  UsdStageRefPtr stage = open_usd_stage_from_path(asset_path);
  if (!stage) {
    MR_ERROR("Failed to open USD stage: {}", asset_path.string());
    return false;
  }

  // Nested payloads (e.g. prefab → .gdt.usd → variant → .geo.usd) must be in the
  // load set or composition stops at empty payload gates (0 meshes).
  stage->Load(SdfPath::AbsoluteRootPath(), UsdLoadWithDescendants);

  GfMatrix4d upCorr = stage_up_axis_correction(stage);
  std::filesystem::path stage_dir = asset_path.parent_path();

  std::unordered_map<std::string, size_t> material_index_by_path;
  std::mutex material_mutex;

  auto ensure_material = [&](UsdShadeMaterial const &mat) -> size_t {
    std::string key = mat.GetPath().GetString();
    std::lock_guard lock(material_mutex);
    auto it = material_index_by_path.find(key);
    if (it != material_index_by_path.end()) {
      return it->second;
    }
    size_t idx = model.materials.size();
    material_index_by_path[key] = idx;
    MaterialData md{};
    if (is_enabled(options, Options::LoadMaterials)) {
      auto preview = find_preview_surface(mat);
      if (preview.has_value()) {
        md = build_material_from_preview(preview.value(), stage_dir, options);
      }
      else {
        md.constants.base_color_factor = Color(0.8f, 0.8f, 0.8f, 1.f);
      }
    }
    else {
      md.constants.base_color_factor = Color(0.8f, 0.8f, 0.8f, 1.f);
    }
    model.materials.push_back(std::move(md));
    return idx;
  };

  if (model.materials.empty()) {
    MaterialData default_m{};
    default_m.constants.base_color_factor = Color(1, 1, 1, 1);
    model.materials.push_back(std::move(default_m));
  }

  {
    ZoneScopedN("USD traverse materials");
    for (UsdPrim const &prim : stage->Traverse(UsdTraverseInstanceProxies())) {
      if (prim.GetTypeName() != TfToken("Material")) {
        continue;
      }
      UsdShadeMaterial mat(prim);
      if (mat.GetPrim().IsValid()) {
        ensure_material(mat);
      }
    }
  }

  std::vector<MeshBuildItem> items;
  {
    ZoneScopedN("USD collect meshes");
    // Traverse the full composed stage (same as lights/cameras). Using only
    // UsdPrimRange(defaultPrim) omits meshes outside the default prim subtree.
    for (UsdPrim const &prim : stage->Traverse(UsdTraverseInstanceProxies())) {
      if (!prim.IsA<UsdGeomMesh>()) {
        continue;
      }
      UsdGeomMesh gm(prim);
      UsdGeomXformable xf(prim);
      GfMatrix4d world = xf.ComputeLocalToWorldTransform(UsdTimeCode::Default());
      // Row-vector USD: p_w = p_l * W, then stage up-axis p' = p_w * U → combined p' = p_l * (W * U).
      world = world * upCorr;

      MeshBuildItem item;
      item.mesh = gm;
      item.world = world;
      item.name = prim.GetName();

      UsdShadeMaterialBindingAPI bindingAPI(prim);
      UsdShadeMaterial mat = bindingAPI.ComputeBoundMaterial();
      if (mat.GetPrim().IsValid()) {
        item.material_index = ensure_material(mat);
      }
      items.push_back(std::move(item));
    }
  }

  {
    bool any_non_guide = false;
    for (auto const &it : items) {
      if (!usd_geom_mesh_is_guide_purpose(it.mesh)) {
        any_non_guide = true;
        break;
      }
    }
    if (any_non_guide) {
      items.erase(std::remove_if(items.begin(), items.end(),
                     [](MeshBuildItem const &it) { return usd_geom_mesh_is_guide_purpose(it.mesh); }),
          items.end());
    }
  }

  {
    ZoneScopedN("USD extract mesh geometry");
    for (size_t i = 0; i < items.size(); ++i) {
      items[i].geom_ok = extract_usd_mesh_geometry(items[i].mesh, items[i].scratch, options);
    }
  }

  model.meshes.resize(items.size());
  {
    ZoneScopedN("USD finalize mesh geometry parallel");
    tbb::parallel_for(size_t{0}, items.size(), [&](size_t i) {
      Mesh &mesh = model.meshes[i];
      MeshBuildItem const &item = items[i];
      if (!item.geom_ok) {
        mesh = Mesh{};
        return;
      }
      finalize_mesh_from_scratch(
          item.scratch, mesh, is_enabled(options, Options::LoadMeshAttributes), options);
      mesh.name = item.name;
      mesh.transforms = {matr4f_from_gf_matrix(item.world)};
      mesh.material = item.material_index;
    });
  }

  model.meshes.erase(std::remove_if(model.meshes.begin(),
                         model.meshes.end(),
                         [](Mesh const &m) { return m.indices.empty(); }),
      model.meshes.end());

  {
    ZoneScopedN("USD lights");
    for (UsdPrim const &prim : stage->Traverse(UsdTraverseInstanceProxies())) {
      if (prim.IsA<UsdLuxDistantLight>()) {
        UsdLuxDistantLight L(prim);
        GfVec3f c(1, 1, 1);
        L.GetColorAttr().Get(&c);
        float intensity = 1.f;
        L.GetIntensityAttr().Get(&intensity);
        float exposure = 0.f;
        L.GetExposureAttr().Get(&exposure);
        intensity = lux_effective_intensity(intensity, exposure);
        model.lights.directionals.emplace_back(c[0], c[1], c[2], intensity);
      }
      else if (prim.IsA<UsdLuxSphereLight>()) {
        UsdLuxSphereLight L(prim);
        GfVec3f c(1, 1, 1);
        L.GetColorAttr().Get(&c);
        float intensity = 1.f;
        L.GetIntensityAttr().Get(&intensity);
        float exposure = 0.f;
        L.GetExposureAttr().Get(&exposure);
        intensity = lux_effective_intensity(intensity, exposure);
        UsdLuxShapingAPI shaping(prim);
        float cone = 180.f;
        if (shaping.GetShapingConeAngleAttr().Get(&cone) && cone > 0.f && cone < 179.f) {
          float outer = GfDegreesToRadians(cone);
          model.lights.spots.emplace_back(c[0], c[1], c[2], intensity, outer * 0.9f, outer);
          continue;
        }
        model.lights.points.emplace_back(c[0], c[1], c[2], intensity);
      }
      else if (prim.IsA<UsdLuxDiskLight>() || prim.IsA<UsdLuxRectLight>() ||
               prim.IsA<UsdLuxCylinderLight>()) {
        UsdLuxLightAPI L(prim);
        GfVec3f c(1, 1, 1);
        L.GetColorAttr().Get(&c);
        float intensity = 1.f;
        L.GetIntensityAttr().Get(&intensity);
        float exposure = 0.f;
        L.GetExposureAttr().Get(&exposure);
        intensity = lux_effective_intensity(intensity, exposure);
        model.lights.points.emplace_back(c[0], c[1], c[2], intensity);
      }
      else if (prim.IsA<UsdLuxDomeLight>()) {
        // Environment domes do not map to punctual lights; skip.
      }
    }
  }

  {
    ZoneScopedN("USD cameras");
    for (UsdPrim const &prim : stage->Traverse(UsdTraverseInstanceProxies())) {
      if (!prim.IsA<UsdGeomCamera>()) {
        continue;
      }
      UsdGeomCamera cam(prim);
      CameraData cd{};
      cd.name = prim.GetName();
      UsdGeomXformable xf(prim);
      GfMatrix4d w = xf.ComputeLocalToWorldTransform(UsdTimeCode::Default());
      w = w * upCorr;
      cd.world_from_camera = matr4f_from_gf_matrix(w);
      TfToken proj;
      if (cam.GetProjectionAttr().Get(&proj)) {
        cd.perspective = (proj == UsdGeomTokens->perspective);
      }
      cam.GetFocalLengthAttr().Get(&cd.focal_length_mm);
      cam.GetHorizontalApertureAttr().Get(&cd.horizontal_aperture_mm);
      cam.GetVerticalApertureAttr().Get(&cd.vertical_aperture_mm);
      GfVec2f cr(0.01f, 1e6f);
      if (cam.GetClippingRangeAttr().Get(&cr)) {
        cd.clipping_range_near = cr[0];
        cd.clipping_range_far = cr[1];
      }
      model.cameras.push_back(std::move(cd));
    }
  }

  return true;
}

} // namespace

void add_usd_loader_nodes(FlowGraph &graph, const Options &options)
{
  ZoneScoped;

  graph.asset_loader = std::make_unique<tbb::flow::input_node<void *>>(
      graph.graph, [&graph, &options](oneapi::tbb::flow_control &fc) -> void * {
        if (graph.model) {
          fc.stop();
          return nullptr;
        }

        ZoneScoped;
        graph.model = std::make_unique<Model>();
        if (!load_usd_into_model(*graph.model, graph.path, options)) {
          graph.model.reset();
          fc.stop();
          return nullptr;
        }

        return usd_pipeline_token();
      });

  graph.meshes_load = std::make_unique<tbb::flow::function_node<void *, void *>>(
      graph.graph, tbb::flow::unlimited, [](void *token) -> void * { return token; });

  graph.materials_load = std::make_unique<tbb::flow::function_node<void *>>(
      graph.graph, tbb::flow::unlimited, [](void *) {});

  graph.lights_load =
      std::make_unique<tbb::flow::function_node<void *>>(graph.graph, tbb::flow::unlimited, [](void *) {});

  tbb::flow::make_edge(*graph.asset_loader, *graph.meshes_load);
  tbb::flow::make_edge(*graph.asset_loader, *graph.materials_load);
  tbb::flow::make_edge(*graph.asset_loader, *graph.lights_load);
}

} // namespace importer
} // namespace mr

// Register scalar half with TfType when missing. Otherwise VtValue::GetType() warns
// ("unregistered C++ type pxr_half::half") for half-valued attributes on real assets.
PXR_NAMESPACE_OPEN_SCOPE
TF_REGISTRY_FUNCTION(TfType)
{
  TfType const t = TfType::Find<GfHalf>();
  if (t.IsUnknown()) {
    TfType::Define<GfHalf>();
  }
}
PXR_NAMESPACE_CLOSE_SCOPE
