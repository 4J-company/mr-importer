#include "mr-importer/importer.hpp"

#include "pch.hpp"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/serialization/vector.hpp>
#include <fstream>
#include <memory>

namespace mr {
inline namespace importer {
namespace {
// Helper structure for serializing IndexSpan as offset+count
struct IndexSpanSerializable {
  std::size_t offset;
  std::size_t count;

  IndexSpanSerializable() : offset(0), count(0) {}
  IndexSpanSerializable(const IndexSpan &span, const IndexArray &parent_array)
  {
    if (span.empty() || parent_array.empty()) {
      offset = 0;
      count = 0;
      return;
    }

    const Index *span_start = span.data();
    const Index *array_start = parent_array.data();
    ASSERT(span_start >= array_start, "Span must point into parent array");
    ASSERT(span_start + span.size() <= array_start + parent_array.size(),
        "Span must be within parent array bounds");

    offset = static_cast<std::size_t>(span_start - array_start);
    count = span.size();
  }

  IndexSpan to_span(IndexArray &parent_array) const
  {
    if (count == 0) {
      return IndexSpan();
    }
    ASSERT(offset + count <= parent_array.size(), "Span out of bounds");
    return IndexSpan(parent_array.data() + offset, count);
  }
};
} // namespace
} // namespace importer
} // namespace mr

namespace boost {
namespace serialization {

// Basic type serialization
template <class Archive>
void serialize(
    Archive &ar, mr::importer::PackedVec3f &vec, const unsigned int version)
{
  ar &vec[0] & vec[1] & vec[2];
}

// Basic type serialization
template <class Archive>
void serialize(Archive &ar, mr::math::Color &vec, const unsigned int version)
{
  float x = vec[0], y = vec[1], z = vec[2], w = vec[3];
  ar & x & y & z & w;
}

// Basic type serialization
template <class Archive>
void serialize(Archive &ar, mr::math::Vec2f &vec, const unsigned int version)
{
  float x = vec[0], y = vec[1];
  ar & x & y;
}

// Basic type serialization
template <class Archive>
void serialize(Archive &ar, mr::math::Vec3f &vec, const unsigned int version)
{
  float x = vec[0], y = vec[1], z = vec[2];
  ar & x & y & z;
}

// Basic type serialization
template <class Archive>
void serialize(Archive &ar, mr::math::Vec4f &vec, const unsigned int version)
{
  float x = vec[0], y = vec[1], z = vec[2], w = vec[3];
  ar & x & y & z & w;
}

// PositionArray (derived from std::vector<Position>)
template <class Archive>
void serialize(
    Archive &ar, mr::importer::PositionArray &arr, const unsigned int version)
{
  // Cast to base class for serialization
  std::vector<mr::importer::Position> &base = arr;
  ar & base;
}

// IndexArray (derived from std::vector<Index>)
template <class Archive>
void serialize(
    Archive &ar, mr::importer::IndexArray &arr, const unsigned int version)
{
  std::vector<mr::importer::Index> &base = arr;
  ar & base;
}

// VertexAttributesArray (derived from std::vector<VertexAttributes>)
template <class Archive>
void serialize(Archive &ar,
    mr::importer::VertexAttributesArray &arr,
    const unsigned int version)
{
  std::vector<mr::importer::VertexAttributes> &base = arr;
  ar & base;
}

template <class Archive>
void serialize(Archive &ar,
    mr::importer::VertexAttributes &attrs,
    const unsigned int version)
{
  ar & attrs.color;
  ar & attrs.normal;
  ar & attrs.tangent;
  ar & attrs.bitangent;
  ar & attrs.texcoord;
}

template <class Archive>
void serialize(
    Archive &ar, mr::importer::Meshlet &meshlet, const unsigned int version)
{
  ar & meshlet.vertex_offset;
  ar & meshlet.triangle_offset;
  ar & meshlet.vertex_count;
  ar & meshlet.triangle_count;
}

template <class Archive>
void serialize(Archive &ar,
    mr::importer::BoundingSphere &sphere,
    const unsigned int version)
{
  ar & sphere.data;
}

template <class Archive>
void serialize(
    Archive &ar, mr::importer::PackedCone &cone, const unsigned int version)
{
  ar & cone.axis[0] & cone.axis[1] & cone.axis[2];
  ar & cone.cutoff;
}

template <class Archive>
void serialize(
    Archive &ar, mr::importer::Cone &cone, const unsigned int version)
{
  ar & cone.apex;
  ar & cone.axis;
  ar & cone.cutoff;
}

template <class Archive>
void serialize(
    Archive &ar, mr::importer::AABB &aabb, const unsigned int version)
{
  ar & aabb.min & aabb.max;
}

// IndexSpanSerializable
template <class Archive>
void serialize(Archive &ar,
    mr::importer::IndexSpanSerializable &span,
    const unsigned int version)
{
  ar & span.offset & span.count;
}

// MeshletArray
template <class Archive>
void serialize(
    Archive &ar, mr::importer::MeshletArray &array, const unsigned int version)
{
  ar & array.meshlets;
  ar & array.meshlet_vertices;
  ar & array.meshlet_triangles;
}

// MeshletBoundsArray
template <class Archive>
void serialize(Archive &ar,
    mr::importer::MeshletBoundsArray &array,
    const unsigned int version)
{
  ar & array.bounding_spheres;
  ar & array.packed_cones;
  ar & array.cones;
}

// Mesh - with custom span handling
template <class Archive>
void save(
    Archive &ar, const mr::importer::Mesh &mesh, const unsigned int version)
{
  // Serialize basic data
  ar & mesh.positions;
  ar & mesh.indices;
  ar & mesh.attributes;

  // Serialize LODs with span handling
  std::size_t lod_count = mesh.lods.size();
  ar & lod_count;

  for (const auto &lod : mesh.lods) {
    // Convert spans to offset+count
    mr::importer::IndexSpanSerializable indices_serializable(
        lod.indices, mesh.indices);
    mr::importer::IndexSpanSerializable shadow_serializable(
        lod.shadow_indices, mesh.indices);
    ar & indices_serializable;
    ar & shadow_serializable;
    ar & lod.meshlet_array;
    ar & lod.meshlet_bounds;
  }

  ar & mesh.transforms;
  ar & mesh.name;
  ar & mesh.material;
  ar & mesh.aabb;
}

template <class Archive>
void load(Archive &ar, mr::importer::Mesh &mesh, const unsigned int version)
{
  // Load basic data
  ar & mesh.positions;
  ar & mesh.indices;
  ar & mesh.attributes;

  // Load LODs
  std::size_t lod_count;
  ar & lod_count;
  mesh.lods.resize(lod_count);

  // Temporary storage for span data
  std::vector<mr::importer::IndexSpanSerializable> indices_data(lod_count);
  std::vector<mr::importer::IndexSpanSerializable> shadow_data(lod_count);

  // Load LOD data
  for (std::size_t i = 0; i < lod_count; ++i) {
    ar &indices_data[i];
    ar &shadow_data[i];
    ar & mesh.lods[i].meshlet_array;
    ar & mesh.lods[i].meshlet_bounds;
  }

  // Restore spans (after mesh.indices is fully loaded)
  for (std::size_t i = 0; i < lod_count; ++i) {
    mesh.lods[i].indices = indices_data[i].to_span(mesh.indices);
    mesh.lods[i].shadow_indices = shadow_data[i].to_span(mesh.indices);
  }

  ar & mesh.transforms;
  ar & mesh.name;
  ar & mesh.material;
  ar & mesh.aabb;
}

template <class Archive>
void serialize(
    Archive &ar, mr::importer::Mesh &mesh, const unsigned int version)
{
  split_free(ar, mesh, version);
}

// Transform (Matr4f) - assuming it's an array of 16 floats
template <class Archive>
void serialize(
    Archive &ar, mr::importer::Transform &transform, const unsigned int version)
{
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      ar &(&((float *)&transform)[4 * i])[j];
    }
  }
}

// SizedUniqueArray
template <class Archive>
void save(Archive &ar,
    const mr::importer::SizedUniqueArray<std::byte> &array,
    const unsigned int version)
{
  std::size_t size = array.size();
  ar & size;
  if (size > 0) {
    ar.save_binary(array.get(), size * sizeof(std::byte));
  }
}

template <class Archive>
void load(Archive &ar,
    mr::importer::SizedUniqueArray<std::byte> &array,
    const unsigned int version)
{
  std::size_t size;
  ar & size;
  array = std::make_unique<std::byte[]>(size);
  array.size(size);
  if (size > 0) {
    ar.load_binary(array.get(), size * sizeof(std::byte));
  }
}

template <class Archive>
void serialize(Archive &ar,
    mr::importer::SizedUniqueArray<std::byte> &array,
    const unsigned int version)
{
  split_free(ar, array, version);
}

// ImageData
template <class Archive>
void save(Archive &ar,
    const mr::importer::ImageData &image,
    const unsigned int version)
{
  // Serialize pixels
  ar & image.pixels;

  // Serialize mips as offset+size pairs
  std::size_t mip_count = image.mips.size();
  ar & mip_count;

  const std::byte *base_ptr = image.pixels.get();
  for (const auto &mip : image.mips) {
    const std::byte *mip_start = mip.data();
    ASSERT(mip_start >= base_ptr, "Mip must point into pixels buffer");
    std::size_t offset = static_cast<std::size_t>(mip_start - base_ptr);
    std::size_t size = mip.size_bytes();
    ar & offset & size;
  }

  ar & image.width;
  ar & image.height;
  ar & image.depth;
  ar & image.bytes_per_pixel;

  // Serialize vk::Format
  uint32_t format_val = static_cast<uint32_t>(image.format);
  ar & format_val;
}

template <class Archive>
void load(
    Archive &ar, mr::importer::ImageData &image, const unsigned int version)
{
  // Load pixels
  ar & image.pixels;

  // Load mips
  std::size_t mip_count;
  ar & mip_count;
  image.mips.clear();

  const std::byte *base_ptr = image.pixels.get();
  for (std::size_t i = 0; i < mip_count; ++i) {
    std::size_t offset, size;
    ar & offset & size;
    // ASSERT(offset + size <= image.pixels.size() * sizeof(std::byte), "Mip out
    // of bounds", mip_count, i, offset, size, image.pixels.size());
    const std::byte *mip_start = base_ptr + offset;
    image.mips.emplace_back(mip_start, size);
  }

  ar & image.width;
  ar & image.height;
  ar & image.depth;
  ar & image.bytes_per_pixel;

  // Load vk::Format
  uint32_t format_val;
  ar & format_val;
  image.format = static_cast<vk::Format>(format_val);
}

template <class Archive>
void serialize(
    Archive &ar, mr::importer::ImageData &image, const unsigned int version)
{
  split_free(ar, image, version);
}

// SamplerData
template <class Archive>
void serialize(
    Archive &ar, mr::importer::SamplerData &sampler, const unsigned int version)
{
  uint32_t mag = static_cast<uint32_t>(sampler.mag);
  uint32_t min = static_cast<uint32_t>(sampler.min);
  ar & mag & min;
  if constexpr (Archive::is_loading::value) {
    sampler.mag = static_cast<vk::Filter>(mag);
    sampler.min = static_cast<vk::Filter>(min);
  }
}

// TextureType
template <class Archive>
void save(Archive &ar,
    const mr::importer::TextureType &type,
    const unsigned int version)
{
  uint32_t val = static_cast<uint32_t>(type);
  ar & val;
}

template <class Archive>
void load(
    Archive &ar, mr::importer::TextureType &type, const unsigned int version)
{
  uint32_t val;
  ar & val;
  if (val < static_cast<uint32_t>(mr::importer::TextureType::Max)) {
    type = static_cast<mr::importer::TextureType>(val);
  }
  else {
    MR_ERROR("Invalid texture type during deserialization");
    type = mr::importer::TextureType::BaseColor;
  }
}

template <class Archive>
void serialize(
    Archive &ar, mr::importer::TextureType &type, const unsigned int version)
{
  split_free(ar, type, version);
}

// TextureData
template <class Archive>
void serialize(
    Archive &ar, mr::importer::TextureData &texture, const unsigned int version)
{
  ar & texture.image;
  ar & texture.type;
  ar & texture.sampler;
  ar & texture.name;
}

// MaterialData::ConstantBlock
template <class Archive>
void serialize(Archive &ar,
    mr::importer::MaterialData::ConstantBlock &constants,
    const unsigned int version)
{
  ar & constants.base_color_factor;
  ar & constants.emissive_color;
  ar & constants.emissive_strength;
  ar & constants.normal_map_intensity;
  ar & constants.roughness_factor;
  ar & constants.metallic_factor;
}

// MaterialData
template <class Archive>
void serialize(Archive &ar,
    mr::importer::MaterialData &material,
    const unsigned int version)
{
  ar & material.constants;
  ar & material.textures;
}

// Light serialization
template <class Archive>
void serialize(
    Archive &ar, mr::importer::LightBase &light, const unsigned int version)
{
  ar & light.packed_color_and_intensity;
}

template <class Archive>
void serialize(Archive &ar,
    mr::importer::DirectionalLight &light,
    const unsigned int version)
{
  ar &static_cast<mr::importer::LightBase &>(light);
}

template <class Archive>
void serialize(
    Archive &ar, mr::importer::SpotLight &light, const unsigned int version)
{
  ar &static_cast<mr::importer::LightBase &>(light);
  ar & light.inner_cone_angle;
  ar & light.outer_cone_angle;
}

template <class Archive>
void serialize(
    Archive &ar, mr::importer::PointLight &light, const unsigned int version)
{
  ar &static_cast<mr::importer::LightBase &>(light);
}

// Model::Lights
template <class Archive>
void serialize(Archive &ar,
    mr::importer::Model::Lights &lights,
    const unsigned int version)
{
  ar & lights.directionals;
  ar & lights.points;
  ar & lights.spots;
}

// Model
template <class Archive>
void serialize(
    Archive &ar, mr::importer::Model &model, const unsigned int version)
{
  ar & model.meshes;
  ar & model.materials;
  ar & model.lights;
}

} // namespace serialization
} // namespace boost

namespace mr {
inline namespace importer {

// Public API implementations
bool serialize(const Model &model, const std::string &filepath)
{
  std::ofstream ofs(filepath, std::ios::binary);
  if (!ofs) {
    MR_ERROR("Failed to open file for writing: {}", filepath);
    return false;
  }

  try {
    boost::archive::binary_oarchive oa(ofs);
    oa << model;
  }
  catch (const std::exception &e) {
    MR_ERROR("Serialization failed: {}", e.what());
    return false;
  }
  catch (...) {
    MR_ERROR("Unknown serialization error");
    return false;
  }

  return true;
}

std::optional<Model> deserialize(const std::string &filepath)
{
  std::ifstream ifs(filepath, std::ios::binary);
  if (!ifs) {
    MR_ERROR("Failed to open file for reading: {}", filepath);
    return std::nullopt;
  }

  Model model;
  try {
    boost::archive::binary_iarchive ia(ifs);
    ia >> model;
  }
  catch (const std::exception &e) {
    MR_ERROR("Deserialization failed: {}", e.what());
    return std::nullopt;
  }
  catch (...) {
    MR_ERROR("Unknown deserialization error");
    return std::nullopt;
  }

  return model;
}

bool serialize(const Mesh &mesh, const std::string &filepath)
{
  std::ofstream ofs(filepath, std::ios::binary);
  if (!ofs) {
    MR_ERROR("Failed to open file for writing: {}", filepath);
    return false;
  }

  try {
    boost::archive::binary_oarchive oa(ofs);
    oa << mesh;
  }
  catch (const std::exception &e) {
    MR_ERROR("Mesh serialization failed: {}", e.what());
    return false;
  }
  catch (...) {
    MR_ERROR("Unknown mesh serialization error");
    return false;
  }

  return true;
}

std::optional<Mesh> deserialize_mesh(const std::string &filepath)
{
  std::ifstream ifs(filepath, std::ios::binary);
  if (!ifs) {
    MR_ERROR("Failed to open file for reading: {}", filepath);
    return std::nullopt;
  }

  Mesh mesh;
  try {
    boost::archive::binary_iarchive ia(ifs);
    ia >> mesh;
  }
  catch (const std::exception &e) {
    MR_ERROR("Mesh deserialization failed: {}", e.what());
    return std::nullopt;
  }
  catch (...) {
    MR_ERROR("Unknown mesh deserialization error");
    return std::nullopt;
  }

  return mesh;
}

bool serialize(const MaterialData &material, const std::string &filepath)
{
  std::ofstream ofs(filepath, std::ios::binary);
  if (!ofs) {
    MR_ERROR("Failed to open file for writing: {}", filepath);
    return false;
  }

  try {
    boost::archive::binary_oarchive oa(ofs);
    oa << material;
  }
  catch (const std::exception &e) {
    MR_ERROR("Material serialization failed: {}", e.what());
    return false;
  }
  catch (...) {
    MR_ERROR("Unknown material serialization error");
    return false;
  }

  return true;
}

std::optional<MaterialData> deserialize_material(const std::string &filepath)
{
  std::ifstream ifs(filepath, std::ios::binary);
  if (!ifs) {
    MR_ERROR("Failed to open file for reading: {}", filepath);
    return std::nullopt;
  }

  MaterialData material;
  try {
    boost::archive::binary_iarchive ia(ifs);
    ia >> material;
  }
  catch (const std::exception &e) {
    MR_ERROR("Material deserialization failed: {}", e.what());
    return std::nullopt;
  }
  catch (...) {
    MR_ERROR("Unknown material deserialization error");
    return std::nullopt;
  }

  return material;
}

} // namespace importer
} // namespace mr
