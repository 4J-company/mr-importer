/**
 * \file loader.cpp
 * \brief glTF loading and conversion into runtime asset structures.
 */

#define WUFFS_IMPLEMENTATION
#include "wuffs-v0.4.c"

#include <dds.hpp>

#include <KHR/khr_df.h>
#include <ktx.h>
#include <ktxvulkan.h>

#include <draco/compression/decode.h>

#include "mr-importer/importer.hpp"

#include "pch.hpp"

#include "flowgraph.hpp"

namespace mr {
inline namespace importer {
uint32_t ImageData::pixel_byte_size() const noexcept
{
  return bytes_per_pixel == -1 ? format_byte_size(format) : bytes_per_pixel;
}

namespace {
/**
 * Parse a glTF file into a fastgltf::Asset.
 *
 * On IO or parse error, logs an error with the fastgltf code and returns
 * std::nullopt. Uses LoadExternalBuffers/Images to resolve external data.
 */
static std::optional<fastgltf::Asset> get_asset_from_path(
    const std::filesystem::path &path)
{
  using namespace fastgltf;

  ZoneScoped;
  auto [err, data] = GltfDataBuffer::FromPath(path);
  if (err != Error::None) {
    MR_ERROR("Failed to parse GLTF file\n"
             "\t\t{}: {}",
        getErrorName(err),
        getErrorMessage(err));
    return std::nullopt; // Failed to load GLTF data
  }

  auto extensions =
      fastgltf::Extensions::KHR_lights_punctual |
      fastgltf::Extensions::KHR_materials_pbrSpecularGlossiness |
      fastgltf::Extensions::KHR_draco_mesh_compression |
      fastgltf::Extensions::EXT_mesh_gpu_instancing |
      fastgltf::Extensions::EXT_texture_webp |
      fastgltf::Extensions::MSFT_texture_dds |
      fastgltf::Extensions::KHR_texture_basisu |
      fastgltf::Extensions::MSFT_packing_occlusionRoughnessMetallic;
  Parser parser(extensions);
  auto options = fastgltf::Options::LoadExternalBuffers |
                 fastgltf::Options::DontRequireValidAssetMember;

  auto dir = path.parent_path();

  MR_DEBUG("Loading from directory {}", dir.string());

  auto [error, asset] = parser.loadGltf(data, dir, options);
  if (error != Error::None) {
    MR_ERROR("Failed to parse GLTF file\n"
             "\t\t{}: {}",
        getErrorName(error),
        getErrorMessage(error));
    return std::nullopt; // Failed to load GLTF data
  }

  return std::move(asset);
}

static const fastgltf::Accessor &get_accessor_from_attribute(
    const fastgltf::Asset &asset, const fastgltf::Attribute &attribute)
{
  ASSERT(attribute.accessorIndex < asset.accessors.size(),
      "Invalid GLTF file. "
      "Attribute didn't contain valid accessor index. "
      "Checkout attribute->accessor indices.",
      attribute.name);

  const fastgltf::Accessor &res = asset.accessors[attribute.accessorIndex];

  return res;
}

struct AccessorDescription {
  enum struct Type { Draco, Plane };

  const fastgltf::Accessor &accessor;
  Type type;
};

/**
 * Locate an accessor by attribute name on a primitive and validate its type.
 *
 * Expects Vec3 Float for POSITION/NORMAL. Logs warnings when missing or of
 * unexpected type, and returns std::nullopt in that case.
 */
static std::optional<AccessorDescription> get_accessor_by_name(Options options,
    const fastgltf::Asset &asset,
    const fastgltf::Primitive &primitive,
    std::string_view name)
{
  ZoneScoped;

  using namespace fastgltf;

  const fastgltf::Attribute *attr = nullptr;
  AccessorDescription::Type type = AccessorDescription::Type::Plane;

  if (const auto *tmp = primitive.findAttribute(name);
      tmp != primitive.attributes.cend()) {
    attr = tmp;
  }

  if (primitive.dracoCompression.get() != nullptr &&
      (attr == nullptr || !(options & Options::PreferUncompressed))) {
    if (const auto *tmp = primitive.dracoCompression->findAttribute(name);
        tmp != primitive.dracoCompression->attributes.cend()) {
      MR_INFO("Decided to load DRACO {}", name);
      attr = tmp;
      type = AccessorDescription::Type::Draco;
    }
  }

  if (attr == nullptr) {
    return std::nullopt;
  }

  return AccessorDescription(get_accessor_from_attribute(asset, *attr), type);
}

static void decode_draco_index_buffer(
    draco::Mesh *mesh, size_t component_size, std::vector<uint8_t> &out_buffer)
{
  if (component_size == 4) {
    ASSERT(sizeof(mesh->face(draco::FaceIndex(0))[0]) == component_size);
    memcpy(out_buffer.data(),
        &mesh->face(draco::FaceIndex(0))[0],
        out_buffer.size());
  }
  else {
    size_t face_stride = component_size * 3;
    for (draco::FaceIndex f(0); f < mesh->num_faces(); ++f) {
      const draco::Mesh::Face &face = mesh->face(f);
      if (component_size == 2) {
        uint16_t indices[3] = {static_cast<uint16_t>(face[0].value()),
            static_cast<uint16_t>(face[1].value()),
            static_cast<uint16_t>(face[2].value())};
        memcpy(out_buffer.data() + f.value() * face_stride,
            &indices[0],
            face_stride);
      }
      else {
        uint8_t indices[3] = {static_cast<uint8_t>(face[0].value()),
            static_cast<uint8_t>(face[1].value()),
            static_cast<uint8_t>(face[2].value())};
        memcpy(out_buffer.data() + f.value() * face_stride,
            &indices[0],
            face_stride);
      }
    }
  }
}

template <typename T>
static bool get_attribute_for_all_points(draco::Mesh *mesh,
    const draco::PointAttribute *p_attribute,
    std::vector<uint8_t> &out_buffer)
{
  size_t byte_offset = 0;
  uint8_t values[64] = {};
  for (draco::PointIndex i(0); i < mesh->num_points(); ++i) {
    const draco::AttributeValueIndex val_index = p_attribute->mapped_index(i);
    if (!p_attribute->ConvertValue<T>(
            val_index, p_attribute->num_components(), (T *)values)) {
      return false;
    }

    ASSERT(byte_offset + sizeof(T) * p_attribute->num_components() <=
           out_buffer.size());
    memcpy(out_buffer.data() + byte_offset,
        &values[0],
        sizeof(T) * p_attribute->num_components());
    byte_offset += sizeof(T) * p_attribute->num_components();
  }
  return true;
}

static bool get_attribute_for_all_points(fastgltf::ComponentType component_type,
    draco::Mesh *mesh,
    const draco::PointAttribute *p_attribute,
    std::vector<uint8_t> &out_buffer)
{
  bool decode_result = false;
#if 0
  switch (component_type) {
  case fastgltf::ComponentType::UnsignedByte:
    decode_result =
        get_attribute_for_all_points<uint8_t>(mesh, p_attribute, out_buffer);
    break;
  case fastgltf::ComponentType::Byte:
    decode_result =
        get_attribute_for_all_points<int8_t>(mesh, p_attribute, out_buffer);
    break;
  case fastgltf::ComponentType::UnsignedShort:
    decode_result =
        get_attribute_for_all_points<uint16_t>(mesh, p_attribute, out_buffer);
    break;
  case fastgltf::ComponentType::Short:
    decode_result =
        get_attribute_for_all_points<int16_t>(mesh, p_attribute, out_buffer);
    break;
  case fastgltf::ComponentType::Int:
    decode_result =
        get_attribute_for_all_points<int32_t>(mesh, p_attribute, out_buffer);
    break;
  case fastgltf::ComponentType::UnsignedInt:
    decode_result =
        get_attribute_for_all_points<uint32_t>(mesh, p_attribute, out_buffer);
    break;
  case fastgltf::ComponentType::Float:
    decode_result =
        get_attribute_for_all_points<float>(mesh, p_attribute, out_buffer);
    break;
  case fastgltf::ComponentType::Double:
    decode_result =
        get_attribute_for_all_points<double>(mesh, p_attribute, out_buffer);
    break;
  default:
    decode_result = false;
    break;
  }

  if (decode_result) {
    return decode_result;
  }
#endif

  switch (p_attribute->data_type()) {
  case draco::DataType::DT_UINT8:
    decode_result =
        get_attribute_for_all_points<uint8_t>(mesh, p_attribute, out_buffer);
    break;
  case draco::DataType::DT_INT8:
    decode_result =
        get_attribute_for_all_points<int8_t>(mesh, p_attribute, out_buffer);
    break;
  case draco::DataType::DT_UINT16:
    decode_result =
        get_attribute_for_all_points<uint16_t>(mesh, p_attribute, out_buffer);
    break;
  case draco::DataType::DT_INT16:
    decode_result =
        get_attribute_for_all_points<int16_t>(mesh, p_attribute, out_buffer);
    break;
  case draco::DataType::DT_INT32:
    decode_result =
        get_attribute_for_all_points<int32_t>(mesh, p_attribute, out_buffer);
    break;
  case draco::DataType::DT_UINT32:
    decode_result =
        get_attribute_for_all_points<uint32_t>(mesh, p_attribute, out_buffer);
    break;
  case draco::DataType::DT_INT64:
    decode_result =
        get_attribute_for_all_points<int64_t>(mesh, p_attribute, out_buffer);
    break;
  case draco::DataType::DT_UINT64:
    decode_result =
        get_attribute_for_all_points<uint64_t>(mesh, p_attribute, out_buffer);
    break;
  case draco::DataType::DT_FLOAT32:
    decode_result =
        get_attribute_for_all_points<float>(mesh, p_attribute, out_buffer);
    break;
  case draco::DataType::DT_FLOAT64:
    decode_result =
        get_attribute_for_all_points<double>(mesh, p_attribute, out_buffer);
    break;
  default:
    decode_result = false;
    break;
  }

  ASSERT(decode_result, p_attribute->data_type(), component_type);
  return decode_result;
}

static size_t byte_size(fastgltf::ComponentType component_type)
{
  switch (component_type) {
  case fastgltf::ComponentType::Byte:
  case fastgltf::ComponentType::UnsignedByte:
    return 1;
  case fastgltf::ComponentType::Short:
  case fastgltf::ComponentType::UnsignedShort:
    return 2;
  case fastgltf::ComponentType::Int:
  case fastgltf::ComponentType::UnsignedInt:
  case fastgltf::ComponentType::Float:
    return 4;
  case fastgltf::ComponentType::Double:
    return 8;
  default:
    return 0;
  }
}

static size_t byte_size(draco::DataType component_type)
{
  switch (component_type) {
  case draco::DataType::DT_UINT8:
  case draco::DataType::DT_INT8:
    return 1;
  case draco::DataType::DT_UINT16:
  case draco::DataType::DT_INT16:
    return 2;
  case draco::DataType::DT_UINT32:
  case draco::DataType::DT_INT32:
  case draco::DataType::DT_FLOAT32:
    return 4;
  case draco::DataType::DT_UINT64:
  case draco::DataType::DT_INT64:
  case draco::DataType::DT_FLOAT64:
    return 8;
  default:
    return 0;
  }
}

// Main Draco decoding function
static bool decode_draco_primitive(const fastgltf::Asset &asset,
    const fastgltf::Primitive &primitive,
    PositionArray &out_positions,
    VertexAttributesArray &out_attributes,
    IndexArray &out_indices)
{
  if (!primitive.dracoCompression) {
    return false;
  }

  // Get the buffer view containing Draco data
  auto &buffer_view = asset.bufferViews[primitive.dracoCompression->bufferView];
  auto &buffer = asset.buffers[buffer_view.bufferIndex];

  // Extract Draco compressed data
  std::span<const char> draco_data;
  std::visit(
      fastgltf::visitor{[&](const fastgltf::sources::Array &array) {
                          draco_data = {(const char *)array.bytes.data() +
                                            buffer_view.byteOffset,
                              buffer_view.byteLength};
                        },
          [&](const fastgltf::sources::Vector &vector) {
            draco_data = {
                (const char *)vector.bytes.data() + buffer_view.byteOffset,
                buffer_view.byteLength};
          },
          [](auto &) {
            MR_ERROR("Unsupported buffer source for Draco compression");
          }},
      buffer.data);

  // Decode Draco mesh
  draco::Decoder decoder;
  draco::DecoderBuffer decoder_buffer;
  decoder_buffer.Init(draco_data.data(), draco_data.size());
  auto decode_result = decoder.DecodeMeshFromBuffer(&decoder_buffer);
  if (!decode_result.ok()) {
    MR_ERROR("Failed to decode Draco mesh:",
        decode_result.status().error_msg_string());
    return false;
  }

  const std::unique_ptr<draco::Mesh> mesh = std::move(decode_result).value();

  // Process indices
  if (primitive.indicesAccessor.has_value()) {
    auto &indices_accessor = asset.accessors[primitive.indicesAccessor.value()];
    size_t component_size =
        std::max<size_t>(byte_size(indices_accessor.componentType),
            1 + (mesh->num_points() > UINT8_MAX) +
                ((mesh->num_points() > UINT16_MAX) << 1));

    std::vector<uint8_t> index_buffer(mesh->num_faces() * 3 * component_size);
    decode_draco_index_buffer(mesh.get(), component_size, index_buffer);

    // Convert to uint32_t indices for our mesh format
    out_indices.resize(mesh->num_faces() * 3);
    if (component_size == 1) {
      const uint8_t *src =
          reinterpret_cast<const uint8_t *>(index_buffer.data());
      for (size_t i = 0; i < out_indices.size(); ++i) {
        out_indices[i] = src[i];
      }
    }
    else if (component_size == 2) {
      const uint16_t *src =
          reinterpret_cast<const uint16_t *>(index_buffer.data());
      for (size_t i = 0; i < out_indices.size(); ++i) {
        out_indices[i] = src[i];
      }
    }
    else {
      const uint32_t *src =
          reinterpret_cast<const uint32_t *>(index_buffer.data());
      for (size_t i = 0; i < out_indices.size(); ++i) {
        out_indices[i] = src[i];
      }
    }
  }

  // Process attributes
  out_positions.resize(mesh->num_points());
  out_attributes.resize(mesh->num_points());

  for (const auto &[attribute_name, draco_attribute_id] :
      primitive.dracoCompression->attributes) {
    if (attribute_name.empty()) {
      continue;
    }

    const draco::PointAttribute *draco_attr =
        mesh->GetAttributeByUniqueId(draco_attribute_id);
    if (!draco_attr)
      continue;

    // Get the original accessor to determine component type
    fastgltf::ComponentType component_type =
        fastgltf::ComponentType::Float; // default
    auto attr_accessor =
        primitive.dracoCompression->findAttribute(attribute_name);
    if (attr_accessor == nullptr)
      continue;
    auto &accessor = asset.accessors[attr_accessor->accessorIndex];
    component_type = accessor.componentType;

    std::vector<uint8_t> attr_buffer;
    attr_buffer.resize(mesh->num_points() * draco_attr->num_components() *
                       std::max(byte_size(component_type),
                           byte_size(draco_attr->data_type())));

    if (!get_attribute_for_all_points(
            component_type, mesh.get(), draco_attr, attr_buffer)) {
      MR_ERROR("Failed to decode Draco attribute:", attribute_name);
      continue;
    }

    // Convert to our internal format
    if (attribute_name == "POSITION") {
      const float *src = reinterpret_cast<const float *>(attr_buffer.data());
      for (size_t i = 0; i < mesh->num_points(); ++i) {
        out_positions[i] = {src[i * 3], src[i * 3 + 1], src[i * 3 + 2]};
      }
    }
    else if (attribute_name == "NORMAL") {
      const float *src = reinterpret_cast<const float *>(attr_buffer.data());
      for (size_t i = 0; i < mesh->num_points(); ++i) {
        out_attributes[i].normal = {src[i * 3], src[i * 3 + 1], src[i * 3 + 2]};
      }
    }
    else if (attribute_name == "TEXCOORD_0") {
      const float *src = reinterpret_cast<const float *>(attr_buffer.data());
      for (size_t i = 0; i < mesh->num_points(); ++i) {
        out_attributes[i].texcoord = {src[i * 2], src[i * 2 + 1]};
      }
    }
    // Add more attribute types as needed
  }

  return true;
}
/**
 * Convert a glTF primitive into an internal Mesh.
 *
 * Populates positions, normals (if present), primary LOD indices, and
 * leaves attributes partially defaulted if normals are missing.
 * Asserts that indices accessor exists.
 */
static std::optional<Mesh> get_mesh_from_primitive(Options options,
    const fastgltf::Asset &asset,
    const fastgltf::Primitive &primitive)
{
  ZoneScoped;

  using namespace fastgltf;

  Mesh mesh;

  if (decode_draco_primitive(
          asset, primitive, mesh.positions, mesh.attributes, mesh.indices)) {
    ASSERT(mesh.lods.size() == 0);
    mesh.lods.emplace_back(IndexSpan(mesh.indices.data(), mesh.indices.size()),
        IndexSpan() // empty shadow indices
    );

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
      mesh.aabb.min = {min_pos[0], min_pos[1], max_pos[2]};
      mesh.aabb.max = {max_pos[0], max_pos[1], max_pos[2]};
    }

    // Set material
    if (primitive.materialIndex) {
      mesh.material = primitive.materialIndex.value();
    }
    else {
      MR_ERROR("Mesh has no material specified");
    }

    ASSERT(mesh.positions.size() == mesh.attributes.size());
    return mesh;
  }
  else {
    std::atomic_bool error = false;
    std::mutex attributes_resize_mutex;
    tbb::parallel_invoke(
        [&]() {
          std::optional<AccessorDescription> positions =
              get_accessor_by_name(options, asset, primitive, "POSITION");
          if (!positions.has_value() ||
              positions.value().accessor.type != fastgltf::AccessorType::Vec3) {
            MR_ERROR("Positions are not in vec3 format ({})",
                getAccessorTypeName(positions.value().accessor.type));
            error = true;
            return;
          }
          mesh.positions.reserve(positions.value().accessor.count);
          fastgltf::iterateAccessor<glm::vec3>(asset,
              positions.value().accessor,
              [&](glm::vec3 v) { mesh.positions.push_back({v.x, v.y, v.z}); });
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
        },
        [&]() {
          std::optional<AccessorDescription> normals =
              get_accessor_by_name(options, asset, primitive, "NORMAL");
          if (normals.has_value()) {
            int count = normals.value().accessor.count;
            {
              std::lock_guard lock(attributes_resize_mutex);
              mesh.attributes.resize(count);
            }
            ASSERT(
                normals.value().accessor.type == fastgltf::AccessorType::Vec3,
                "Normals are not in vec3 format",
                getAccessorTypeName(normals.value().accessor.type));
            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset, normals.value().accessor, [&](glm::vec3 v, int index) {
                  mesh.attributes[index].normal = {v.x, v.y, v.z};
                });
          }
        },
        [&]() {
          std::optional<AccessorDescription> texcoords =
              get_accessor_by_name(options, asset, primitive, "TEXCOORD_0");
          if (texcoords.has_value()) {
            ASSERT(texcoords.value().accessor.type ==
                   fastgltf::AccessorType::Vec2);
            {
              std::lock_guard lock(attributes_resize_mutex);
              mesh.attributes.resize(texcoords.value().accessor.count);
            }
            fastgltf::iterateAccessorWithIndex<glm::vec2>(
                asset, texcoords.value().accessor, [&](glm::vec2 v, int index) {
                  mesh.attributes[index].texcoord = {v.x, v.y};
                });
          }
        },
        [&]() {
          if (!primitive.indicesAccessor.has_value()) {
            MR_ERROR(
                "Primitive didn't contain indices - we don't support that");
            error = true;
            return;
          }

          auto &idxAccessor =
              asset.accessors[primitive.indicesAccessor.value()];
          mesh.indices.resize(idxAccessor.count);
          fastgltf::copyFromAccessor<std::uint32_t>(
              asset, idxAccessor, mesh.indices.data());

          ASSERT(mesh.lods.size() == 0);
          mesh.lods.emplace_back(
              IndexSpan(mesh.indices.data(), mesh.indices.size()),
              IndexSpan() // empty shadow indices
          );
        },
        [&primitive, &mesh]() {
          if (primitive.materialIndex) {
            mesh.material = primitive.materialIndex.value();
          }
          else {
            MR_ERROR("Mesh has no material specified");
          }
        });
    if (error) {
      return std::nullopt;
    }
  }

  if (mesh.attributes.size() != 0 &&
      mesh.positions.size() != mesh.attributes.size()) {
    return std::nullopt;
  }

  return mesh;
}

/**
 * Extract meshes from the fastgltf asset and attach per-mesh transforms.
 *
 * Iterates scene nodes to gather transforms, then converts all primitives
 * into Mesh objects, preserving names.
 */
static std::vector<Mesh> get_meshes_from_asset(
    Options options, fastgltf::Asset *asset)
{
  ZoneScoped;

  ASSERT(asset);

  using namespace fastgltf;

  std::vector<std::vector<Transform>> transforms;
  transforms.resize(asset->meshes.size());
  {
    ZoneScoped;

    fastgltf::iterateSceneNodes(*asset,
        0,
        fastgltf::math::fmat4x4(),
        [&](fastgltf::Node &node, fastgltf::math::fmat4x4 matrix) {
          if (node.meshIndex.has_value()) {
            if (node.instancingAttributes.size() > 0) {
              std::vector<fastgltf::math::fvec3> translations;
              const auto &translation_iterator =
                  node.findInstancingAttribute("TRANSLATION");
              ASSERT(translation_iterator != node.instancingAttributes.cend());
              const auto &translation_accessor =
                  get_accessor_from_attribute(*asset, *translation_iterator);
              translations.reserve(translation_accessor.count);
              fastgltf::iterateAccessor<fastgltf::math::fvec3>(
                  *asset, translation_accessor, [&](fastgltf::math::fvec3 v) {
                    translations.push_back({v.x(), v.y(), v.z()});
                  });

              std::vector<fastgltf::math::fquat> rotations;
              const auto &rotation_iterator =
                  node.findInstancingAttribute("ROTATION");
              ASSERT(rotation_iterator != node.instancingAttributes.cend());
              const auto &rotation_accessor =
                  get_accessor_from_attribute(*asset, *rotation_iterator);
              rotations.reserve(translation_accessor.count);
              fastgltf::iterateAccessor<fastgltf::math::fquat>(*asset,
                  rotation_accessor,
                  [&](fastgltf::math::fquat q) { rotations.push_back(q); });

              std::vector<fastgltf::math::fvec3> scales;
              const auto &scale_iterator =
                  node.findInstancingAttribute("SCALE");
              ASSERT(scale_iterator != node.instancingAttributes.cend());
              const auto &scale_accessor =
                  get_accessor_from_attribute(*asset, *scale_iterator);
              scales.reserve(scale_accessor.count);
              fastgltf::iterateAccessor<fastgltf::math::fvec3>(
                  *asset, scale_accessor, [&](fastgltf::math::fvec3 v) {
                    scales.push_back({v.x(), v.y(), v.z()});
                  });

              transforms[*node.meshIndex].reserve(scales.size());
              for (const auto &[t, r, s] :
                  std::views::zip(translations, rotations, scales)) {
                fastgltf::math::fmat4x4 res =
                    scale(rotate(translate(matrix, t), r), s);
                transforms[*node.meshIndex].push_back({
                    res[0][0], res[1][0], res[2][0], res[3][0],
                    res[0][1], res[1][1], res[2][1], res[3][1],
                    res[0][2], res[1][2], res[2][2], res[3][2],
                    res[0][3], res[1][3], res[2][3], res[3][3],
                });
              }
            }

            glm::mat4 t = glm::make_mat4(matrix.data());
            transforms[*node.meshIndex].push_back({
                t[0][0], t[1][0], t[2][0], t[3][0],
                t[0][1], t[1][1], t[2][1], t[3][1],
                t[0][2], t[1][2], t[2][2], t[3][2],
                t[0][3], t[1][3], t[2][3], t[3][3],
            });
          }
        });
  }

  tbb::concurrent_vector<Mesh> meshes;
  meshes.reserve(asset->meshes.size() * 2);

  {
    ZoneScoped;

    tbb::parallel_for<int>(0, asset->meshes.size(), [&](int i) {
      const fastgltf::Mesh &gltfMesh = asset->meshes[i];
      tbb::parallel_for<int>(0, gltfMesh.primitives.size(), [&](int j) {
        const auto &primitive = gltfMesh.primitives[j];
        std::optional<Mesh> mesh_opt =
            get_mesh_from_primitive(options, *asset, primitive);
        if (mesh_opt.has_value()) {
          mesh_opt->transforms = transforms[i];
          mesh_opt->name = std::move(gltfMesh.name);
          meshes.emplace_back(std::move(mesh_opt.value()));
        }
      });
    });
  }

  std::vector<Mesh> result;
  result.resize(meshes.size());
  tbb::parallel_for<int>(
      0, meshes.size(), [&](int i) { result[i] = std::move(meshes[i]); });

  return result;
}

static void resize_image(ImageData &image,
    size_t component_number,
    size_t component_size,
    size_t desired_component_number)
{
  ZoneScoped;

  if (desired_component_number == component_number) {
    return;
  }

  size_t pixel_size = image.pixel_byte_size();
  size_t pixel_count = image.num_of_pixels();

  size_t desired_pixel_size = desired_component_number * component_size;

  size_t desired_byte_size =
      pixel_size * pixel_count / component_number * desired_component_number;
  std::unique_ptr<std::byte[]> new_ptr =
      std::make_unique<std::byte[]>(desired_byte_size);

  for (int i = 0; i < pixel_count; i++) {
    size_t pixel_byte_offset = i * pixel_size;
    size_t desired_pixel_byte_offset = i * desired_pixel_size;
    for (int j = 0; j < std::min(component_number, desired_component_number);
        j++) {
      for (int k = 0; k < component_size; k++) {
        new_ptr[desired_pixel_byte_offset + j * component_size + k] =
            image.pixels[pixel_byte_offset + j * component_size + k];
      }
    }
  }

  image.pixels = std::move(new_ptr);
  image.pixels.size(desired_byte_size);

  int offset = 0;
  for (int i = 0; i < image.mips.size(); i++) {
    size_t desired_mip_size =
        image.mips[i].size() / component_number * desired_component_number;
    image.mips[i] = {image.pixels.get() + offset, desired_mip_size};
    offset += desired_mip_size;
  }
}

/**
 * Decode a glTF image.
 *
 * Supports URI, embedded vector, and buffer view sources. Returns an
 * ImageData with owned memory; logs warnings for unexpected sources.
 */
static std::optional<ImageData> get_image_from_gltf(
    const std::filesystem::path &directory,
    Options options,
    const fastgltf::Asset &asset,
    const fastgltf::Image &image)
{
  ZoneScoped;

  ImageData new_image{};

  auto load_dds_from_file = [](const std::string &path,
                                ImageData &new_image) -> bool {
    ZoneScopedN("DDS import from file");

    dds::Image dds_image;
    dds::ReadResult res = dds::readFile(path, &dds_image);
    if (res != dds::ReadResult::Success)
      return false;
    if (dds_image.data.get() == nullptr)
      return false;
    if (dds::getBitsPerPixel(dds_image.format) % 8 != 0)
      return false;

    new_image.width = dds_image.width;
    new_image.height = dds_image.height;
    new_image.depth = dds_image.arraySize;
    new_image.format = (vk::Format)dds::getVulkanFormat(
        dds_image.format, dds_image.supportsAlpha);
    new_image.bytes_per_pixel = dds::getBitsPerPixel(dds_image.format) / 8;

    new_image.pixels.reset((std::byte *)dds_image.data.release());
    new_image.pixels.size(new_image.bytes_per_pixel * new_image.width * new_image.height);
    int size = 0;
    for (auto &mip : dds_image.mipmaps) {
      auto tmp = std::as_bytes(mip);
      size += mip.size_bytes();
      new_image.mips.emplace_back(tmp);
    }
    new_image.pixels.size(size);

    return new_image.width > 0 && new_image.height > 0 &&
           new_image.bytes_per_pixel > 0;
  };

  auto load_dds_from_memory = [](const std::byte *data,
                                  size_t size,
                                  ImageData &new_image,
                                  const std::string &context) -> bool {
    ZoneScopedN("DDS import from memory");

    dds::Image dds_image;
    dds::ReadResult res = dds::readImage((uint8_t *)data, size, &dds_image);
    if (res != dds::ReadResult::Success)
      return false;
    if (dds_image.data.get() == nullptr)
      return false;
    if (dds::getBitsPerPixel(dds_image.format) % 8 != 0)
      return false;

    new_image.width = dds_image.width;
    new_image.height = dds_image.height;
    new_image.depth = dds_image.arraySize;
    new_image.format = (vk::Format)dds::getVulkanFormat(
        dds_image.format, dds_image.supportsAlpha);
    new_image.bytes_per_pixel = dds::getBitsPerPixel(dds_image.format) / 8;

    new_image.pixels.reset((std::byte *)dds_image.data.release());
    for (auto &mip : dds_image.mipmaps) {
      auto tmp = std::as_bytes(mip);
      size += mip.size_bytes();
      new_image.mips.emplace_back(std::as_bytes(mip));
    }
    new_image.pixels.size(size);

    return new_image.width > 0 && new_image.height > 0 &&
           new_image.bytes_per_pixel > 0;
  };

  auto load_ktx2_from_file = [](const std::string &path,
                                 ImageData &new_image) -> bool {
    ZoneScopedN("KTX import from file");

    ktxTexture2 *tex;
    KTX_error_code result = ktxTexture_CreateFromNamedFile(path.c_str(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        (ktxTexture **)&tex);

    std::unique_ptr<ktxTexture2, void (*)(ktxTexture2*)> ktx_texture {
      tex,
      +[](ktxTexture2 *ptr) { ktxTexture_Destroy((ktxTexture *)ptr); }
    };
    if (result != KTX_SUCCESS)
      return false;

    if (ktxTexture2_NeedsTranscoding(ktx_texture.get())) {
      result = ktxTexture2_TranscodeBasis(ktx_texture.get(), KTX_TTF_BC7_RGBA, 0);
      if (result != KTX_SUCCESS) {
        return false;
      }
    }

    new_image.height = ktx_texture->baseHeight;
    new_image.width = ktx_texture->baseWidth;
    new_image.depth = ktx_texture->baseDepth;
    new_image.format = (vk::Format)ktxTexture2_GetVkFormat(ktx_texture.get());
    new_image.bytes_per_pixel = format_byte_size(new_image.format);

    new_image.pixels = std::make_unique_for_overwrite<std::byte[]>(ktx_texture->dataSize);
    new_image.pixels.size(ktx_texture->dataSize);
    std::memcpy(new_image.pixels.get(), ktx_texture->pData, ktx_texture->dataSize);

    for (uint32_t mip_index = 0; mip_index < ktx_texture->numLevels;
        mip_index++) {
      uint32_t copy_width = new_image.width >> mip_index;
      uint32_t copy_height = new_image.height >> mip_index;
      ktx_size_t copy_buffer_offset = 0;
      result = ktxTexture_GetImageOffset(
          (ktxTexture *)ktx_texture.get(), mip_index, 0, 0, &copy_buffer_offset);
      if (result != KTX_SUCCESS) {
        continue;
      }

      new_image.mips.emplace_back(new_image.pixels.get() + copy_buffer_offset,
          copy_width * copy_height * new_image.bytes_per_pixel);
    }

    return true;
  };

  auto load_ktx2_from_memory = [](const std::byte *data,
                                   size_t size,
                                   ImageData &new_image,
                                   const std::string &context) -> bool {
    ZoneScopedN("KTX import from memory");

    ktxTexture2 *tex;
    KTX_error_code result =
        ktxTexture2_CreateFromMemory((const ktx_uint8_t *)data,
            size,
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &tex);

    std::unique_ptr<ktxTexture2, void (*)(ktxTexture2*)> ktx_texture {
      tex,
      +[](ktxTexture2 *ptr) { ktxTexture_Destroy((ktxTexture *)ptr); }
    };

    if (result != KTX_SUCCESS)
      return false;

    if (ktxTexture2_NeedsTranscoding(ktx_texture.get())) {
      result = ktxTexture2_TranscodeBasis(ktx_texture.get(), KTX_TTF_BC7_RGBA, 0);
      if (result != KTX_SUCCESS) {
        return false;
      }
    }

    new_image.height = ktx_texture->baseHeight;
    new_image.width = ktx_texture->baseWidth;
    new_image.depth = ktx_texture->baseDepth;
    new_image.format = (vk::Format)ktxTexture2_GetVkFormat(ktx_texture.get());
    new_image.bytes_per_pixel = format_byte_size(new_image.format);

    new_image.pixels = std::make_unique_for_overwrite<std::byte[]>(ktx_texture->dataSize);
    new_image.pixels.size(ktx_texture->dataSize);
    std::memcpy(new_image.pixels.get(), ktx_texture->pData, ktx_texture->dataSize);

    for (uint32_t mip_index = 0; mip_index < ktx_texture->numLevels;
        mip_index++) {
      uint32_t copy_width = new_image.width >> mip_index;
      uint32_t copy_height = new_image.height >> mip_index;
      ktx_size_t copy_buffer_offset = 0;
      result = ktxTexture_GetImageOffset(
          (ktxTexture *)ktx_texture.get(), mip_index, 0, 0, &copy_buffer_offset);
      if (result != KTX_SUCCESS) {
        continue;
      }

      new_image.mips.emplace_back(new_image.pixels.get() + copy_buffer_offset,
          copy_width * copy_height * new_image.bytes_per_pixel);
    }

    return true;
  };

  auto try_load_wuffs_from_file = [](const std::string &path,
                                      ImageData &new_image) -> bool {
    ZoneScopedN("WUFFS import from file");

    std::unique_ptr<FILE, int (*)(FILE *)> file(
        fopen(path.c_str(), "rb"), fclose);
    if (file.get() == nullptr) {
      MR_INFO("Failed to open image file {}", path.c_str());
      return false;
    }

    wuffs_aux::DecodeImageCallbacks callbacks;
    wuffs_aux::sync_io::FileInput input(file.get());
    wuffs_aux::DecodeImageResult img = wuffs_aux::DecodeImage(callbacks, input);

    if (!img.error_message.empty()) {
      MR_INFO("Failed to parse image: {}", img.error_message);
      return false;
    }
    if (!img.pixbuf.pixcfg.is_valid()) {
      MR_INFO("Failed to parse image for unknown reason");
      return false;
    }

    wuffs_base__table_u8 tab = img.pixbuf.plane(0);
    wuffs_base__pixel_format format = img.pixbuf.pixel_format();

    if (format.bits_per_pixel() % 8 != 0) {
      MR_INFO("Image format bits_per_pixel % 8 != 0. "
              "To handle such cases you'd need to change public API from "
              "byte_size to bit_size");
      return false;
    }

    new_image.bytes_per_pixel = format.bits_per_pixel() / 8;
    new_image.width = tab.width / new_image.bytes_per_pixel;
    new_image.height = tab.height;

    if (new_image.width <= 0 || new_image.height <= 0 ||
        new_image.bytes_per_pixel <= 0) {
      MR_INFO("Image format bits_per_pixel % 8 != 0. "
              "To handle such cases you'd need to change public API from "
              "byte_size to bit_size");
      return false;
    }

    new_image.pixels.reset((std::byte *)img.pixbuf_mem_owner.release());
    new_image.pixels.size(new_image.width * new_image.height * new_image.bytes_per_pixel);
    new_image.mips.emplace_back(new_image.pixels.get(), new_image.pixels.size());
    return true;
  };

  auto try_load_wuffs_from_memory = [](const std::byte *data,
                                        size_t size,
                                        ImageData &new_image,
                                        const std::string &context) -> bool {
    ZoneScopedN("WUFFS import from memory");

    wuffs_aux::DecodeImageCallbacks callbacks;
    wuffs_aux::sync_io::MemoryInput input((const char *)data, size);
    wuffs_aux::DecodeImageResult img = wuffs_aux::DecodeImage(callbacks, input);

    if (!img.error_message.empty()) {
      MR_INFO("Failed to parse image: {}", img.error_message);
      return false;
    }
    if (!img.pixbuf.pixcfg.is_valid()) {
      MR_INFO("Failed to parse image for unknown reason");
      return false;
    }

    wuffs_base__table_u8 tab = img.pixbuf.plane(0);
    wuffs_base__pixel_format format = img.pixbuf.pixel_format();

    if (format.bits_per_pixel() % 8 != 0) {
      MR_INFO("Image format bits_per_pixel % 8 != 0. "
              "To handle such cases you'd need to change public API from "
              "byte_size to bit_size");
      return false;
    }

    new_image.bytes_per_pixel = format.bits_per_pixel() / 8;
    new_image.width = tab.width / new_image.bytes_per_pixel;
    new_image.height = tab.height;

    if (new_image.width <= 0 || new_image.height <= 0 ||
        new_image.bytes_per_pixel <= 0) {
      return false;
    }

    new_image.pixels.reset((std::byte *)img.pixbuf_mem_owner.release());
    new_image.pixels.size(new_image.width * new_image.height * new_image.bytes_per_pixel);
    new_image.mips.emplace_back((std::byte *)tab.ptr, new_image.pixels.size());
    return true;
  };

  auto try_load_with_fallback = [&](const std::byte *data,
                                    size_t size,
                                    fastgltf::MimeType mimeType,
                                    const std::string &context_info =
                                        "") -> bool {
    if (mimeType == fastgltf::MimeType::DDS) {
      return load_dds_from_memory(data, size, new_image, context_info);
    }

    if (mimeType == fastgltf::MimeType::KTX2) {
      return load_ktx2_from_memory(data, size, new_image, context_info);
    }

    if (try_load_wuffs_from_memory(data, size, new_image, context_info)) {
      return true;
    }

    if (mimeType == fastgltf::MimeType::GltfBuffer ||
        mimeType == fastgltf::MimeType::OctetStream) {
      if (load_dds_from_memory(data, size, new_image, context_info)) {
        return true;
      }
      if (load_ktx2_from_memory(data, size, new_image, context_info)) {
        return true;
      }
    }

    return false;
  };

  auto try_load_file_with_fallback = [&](const std::string &path,
                                         fastgltf::MimeType mimeType) -> bool {
    if (mimeType == fastgltf::MimeType::DDS ||
        std::filesystem::path(path).extension() == ".dds") {
      return load_dds_from_file(path, new_image);
    }

    if (mimeType == fastgltf::MimeType::KTX2 ||
        std::filesystem::path(path).extension() == ".ktx2") {
      return load_ktx2_from_file(path, new_image);
    }

    if (try_load_wuffs_from_file(path, new_image)) {
      return true;
    }

    if (mimeType == fastgltf::MimeType::GltfBuffer ||
        mimeType == fastgltf::MimeType::OctetStream) {
      MR_INFO(
          "WUFFS failed for ambiguous mime type, trying DDS then KTX2", path);

      if (load_dds_from_file(path, new_image)) {
        return true;
      }
      if (load_ktx2_from_file(path, new_image)) {
        return true;
      }
    }

    return false;
  };

  std::visit(
      fastgltf::visitor{
          [](auto &arg) {
            ASSERT(false, "Unsupported image source in a GLTF file", arg);
          },
          [&](const fastgltf::sources::URI &filePath) {
            ASSERT(filePath.fileByteOffset == 0,
                "Offsets with files are not supported because plain wuffs' C++ "
                "bindings don't support them.",
                filePath.uri.c_str());
            ASSERT(filePath.uri.isLocalPath(),
                "Tried to load an image from absolute path"
                " - we don't support that (local files only)",
                filePath.uri.c_str());

            std::filesystem::path absolute_path =
                directory / filePath.uri.fspath();
            const std::string path = std::move(absolute_path).string();

            if (!try_load_file_with_fallback(path, filePath.mimeType)) {
              PANIC(
                  "Failed to load image file with all available methods", path);
            }
          },
          [&](const fastgltf::sources::Array &array) {
            ZoneScopedN("Import from memory array");
            if (!try_load_with_fallback((const std::byte *)array.bytes.data(),
                    array.bytes.size(),
                    fastgltf::MimeType::None,
                    "memory array")) {
              PANIC("Failed to load image from memory array with all available "
                    "methods");
            }
          },
          [&](const fastgltf::sources::Vector &vector) {
            ZoneScopedN("Import from memory vector");
            if (!try_load_with_fallback((const std::byte *)vector.bytes.data(),
                    vector.bytes.size(),
                    vector.mimeType,
                    "memory vector")) {
              PANIC("Failed to load image from memory vector with all "
                    "available methods");
            }
          },
          [&](const fastgltf::sources::BufferView &view) {
            auto &bufferView = asset.bufferViews[view.bufferViewIndex];
            auto &buffer = asset.buffers[bufferView.bufferIndex];

            std::visit(
                fastgltf::visitor{
                    [](auto &arg) {
                      ASSERT(false,
                          "Try to process image from buffer view but not from "
                          "RAM "
                          "(should be illegal because of LoadExternalBuffers)",
                          arg);
                    },
                    [&](const fastgltf::sources::Array &array) {
                      ZoneScopedN("Import from buffer view array");
                      const std::byte *data =
                          (const std::byte *)array.bytes.data() +
                          bufferView.byteOffset;
                      if (!try_load_with_fallback(data,
                              bufferView.byteLength,
                              array.mimeType,
                              "buffer view array")) {
                        PANIC("Failed to load image from buffer view array "
                              "with all available methods");
                      }
                    },
                    [&](const fastgltf::sources::Vector &vector) {
                      ZoneScopedN("Import from buffer view vector");
                      const std::byte *data =
                          (const std::byte *)vector.bytes.data() +
                          bufferView.byteOffset;
                      if (!try_load_with_fallback(data,
                              bufferView.byteLength,
                              vector.mimeType,
                              "buffer view vector")) {
                        PANIC("Failed to load image from buffer view vector "
                              "with all available methods");
                      }
                    }},
                buffer.data);
          },
      },
      image.data);

  // Format fallback logic (unchanged from your original code)
  if (new_image.format == vk::Format()) {
    switch (new_image.bytes_per_pixel) {
    case 1:
      if (!(options & Options::Allow1ComponentImages)) {
        MR_INFO("Resizing an image from 1-component to 2-component. Consider "
                "doing it offline");
        resize_image(new_image, 1, 1, 2);
      }
      else {
        new_image.format = vk::Format::eR8Srgb;
        break;
      }
    case 2:
      if (!(options & Options::Allow2ComponentImages)) {
        MR_INFO("Resizing an image from 2-component to 3-component. Consider "
                "doing it offline");
        resize_image(new_image, 2, 1, 3);
      }
      else {
        new_image.format = vk::Format::eR8G8Srgb;
        break;
      }
    case 3:
      if (!(options & Options::Allow3ComponentImages)) {
        MR_INFO("Resizing an image from 3-component to 4-component. Consider "
                "doing it offline");
        resize_image(new_image, 3, 1, 4);
      }
      else {
        new_image.format = vk::Format::eB8G8R8Srgb;
        break;
      }
    case 4:
      if (!(options & Options::Allow4ComponentImages)) {
        MR_ERROR(
            "Disallowing 4-component images makes lossless import impossible. "
            "Transfer your images to 3-components (or less) offline!");
      }
      else {
        new_image.format = vk::Format::eB8G8R8A8Srgb;
        break;
      }
    default:
      PANIC("Failed to determine number of image components",
          new_image.bytes_per_pixel,
          options);
      break;
    }
  }

  ASSERT(new_image.pixels.get() != nullptr,
      "Unexpected error reading image data. Needs investigation",
      image.name);

  return new_image;
}

/**
 * Create a TextureData from a glTF TextureInfo, decoding its image.
 *
 * Returns TextureData on success or an explanatory string_view on failure
 * (e.g., unsupported formats). Does not throw.
 */
static std::expected<TextureData, std::string_view> get_texture_from_gltf(
    const std::filesystem::path &directory,
    Options options,
    fastgltf::Asset &asset,
    TextureType type,
    const fastgltf::TextureInfo &texinfo)
{
  ZoneScoped;

  fastgltf::Texture &tex = asset.textures[texinfo.textureIndex];

  size_t img_idx = ~0z;

  if (tex.imageIndex.has_value()) {
    img_idx = tex.imageIndex.value();
  }
  if (tex.ddsImageIndex.has_value() &&
      (!(options & Options::PreferUncompressed) || img_idx == ~0z)) {
    img_idx = tex.ddsImageIndex.value();
  }
  if (tex.basisuImageIndex.has_value() &&
      (!(options & Options::PreferUncompressed) || img_idx == ~0z)) {
    img_idx = tex.basisuImageIndex.value();
  }
  if (tex.webpImageIndex.has_value() && img_idx == ~0z) {
    img_idx = tex.webpImageIndex.value();
  }
  if (img_idx == ~0z) {
    return std::unexpected("Texture is in unsupported format");
  }

  fastgltf::Image &img = asset.images[img_idx];
  std::optional<ImageData> img_data_opt =
      get_image_from_gltf(directory, options, asset, img);
  ASSERT(img_data_opt.has_value(), "Unable to load image");

  static auto convert_filter =
      [](fastgltf::Optional<fastgltf::Filter> filter) -> vk::Filter {
    if (!filter.has_value()) {
      return vk::Filter();
    }
    switch (filter.value()) {
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::NearestMipMapNearest:
      return vk::Filter::eNearest;
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapLinear:
    case fastgltf::Filter::LinearMipMapNearest:
      return vk::Filter::eLinear;
    }
    ASSERT(false,
        "Unhandled Sampler::Filter",
        filter.value(),
        (int)filter.value());
    return vk::Filter();
  };

  SamplerData sampler{};
  if (tex.samplerIndex.has_value()) {
    sampler = {
        convert_filter(asset.samplers[tex.samplerIndex.value()].minFilter),
        convert_filter(asset.samplers[tex.samplerIndex.value()].magFilter),
    };
  };

  return TextureData(std::move(img_data_opt.value()), type, sampler);
}

/** Convert normalized vec4 to Color. */
static Color color_from_nvec4(fastgltf::math::nvec4 v)
{
  return {v.x(), v.y(), v.z(), v.w()};
}
/** Convert normalized vec3 to Color with alpha = 1. */
static Color color_from_nvec3(fastgltf::math::nvec3 v)
{
  return {v.x(), v.y(), v.z(), 1.f};
}

/**
 * Build MaterialData array from glTF materials.
 *
 * Transfers PBR factors and attempts to load referenced textures; logs
 * errors/warnings for failed texture loads and continues gracefully.
 */
static std::vector<MaterialData> get_materials_from_asset(
    const std::filesystem::path &directory,
    fastgltf::Asset *asset,
    Options options)
{
  ZoneScoped;

  ASSERT(asset);

  std::vector<MaterialData> materials;
  materials.resize(asset->materials.size());

  tbb::parallel_for(0uz,
      asset->materials.size(),
      [&asset, &materials, &directory, &options](size_t i) {
        fastgltf::Material &src = asset->materials[i];
        MaterialData &dst = materials[i];

        dst.constants.base_color_factor =
            color_from_nvec4(src.pbrData.baseColorFactor);
        dst.constants.roughness_factor = src.pbrData.roughnessFactor;
        dst.constants.metallic_factor = src.pbrData.metallicFactor;
        dst.constants.emissive_color = color_from_nvec3(src.emissiveFactor);
        dst.constants.normal_map_intensity = 1;
        dst.constants.emissive_strength = src.emissiveStrength;

        tbb::concurrent_vector<TextureData> textures;

        tbb::parallel_invoke(
            [&] {
              if (src.pbrData.baseColorTexture.has_value()) {
                auto exp = get_texture_from_gltf(directory,
                    options,
                    *asset,
                    TextureType::BaseColor,
                    src.pbrData.baseColorTexture.value());
                if (exp.has_value()) {
                  textures.emplace_back(std::move(exp.value()));
                }
                else {
                  MR_ERROR("Loading Base Color texture - ", exp.error());
                }
              }
              else if (src.specularGlossiness.get() &&
                       src.specularGlossiness->diffuseTexture.has_value()) {
                auto exp = get_texture_from_gltf(directory,
                    options,
                    *asset,
                    TextureType::BaseColor,
                    src.specularGlossiness->diffuseTexture.value());
                if (exp.has_value()) {
                  textures.emplace_back(std::move(exp.value()));
                }
                else {
                  MR_ERROR("Loading Base Color texture - ", exp.error());
                }
              }
            },
            [&] {
              if (src.normalTexture.has_value()) {
                auto exp = get_texture_from_gltf(directory,
                    options,
                    *asset,
                    TextureType::NormalMap,
                    src.normalTexture.value());
                if (exp.has_value()) {
                  textures.emplace_back(std::move(exp.value()));
                }
                else {
                  MR_WARNING("Loading Normal Map texture - ", exp.error());
                }
              }
            },
            [&] {
              if (src.packedOcclusionRoughnessMetallicTextures &&
                  src.packedOcclusionRoughnessMetallicTextures
                      ->occlusionRoughnessMetallicTexture.has_value()) {
                auto exp = get_texture_from_gltf(directory,
                    options,
                    *asset,
                    TextureType::OcclusionRoughnessMetallic,
                    src.packedOcclusionRoughnessMetallicTextures
                        ->occlusionRoughnessMetallicTexture.value());
                if (exp.has_value()) {
                  textures.emplace_back(std::move(exp.value()));
                }
                else {
                  MR_ERROR(
                      "Loading packed Occlusion Roughness Metallic texture - ",
                      exp.error());
                }
              }
              else if (src.pbrData.metallicRoughnessTexture.has_value()) {
                auto exp = get_texture_from_gltf(directory,
                    options,
                    *asset,
                    TextureType::RoughnessMetallic,
                    src.pbrData.metallicRoughnessTexture.value());

                if (exp.has_value()) {
                  textures.emplace_back(std::move(exp.value()));
                }
                else {
                  MR_ERROR(
                      "Loading Metallic Roughness texture - ", exp.error());
                }

                if (src.occlusionTexture.has_value()) {
                  auto exp = get_texture_from_gltf(directory,
                      options,
                      *asset,
                      TextureType::OcclusionMap,
                      src.occlusionTexture.value());
                  if (exp.has_value()) {
                    textures.emplace_back(std::move(exp.value()));
                  }
                  else {
                    MR_ERROR("Loading Occlusion texture - ", exp.error());
                  }
                }
              }
              else if (src.specularGlossiness.get() &&
                       src.specularGlossiness->specularGlossinessTexture
                           .has_value()) {
                auto exp = get_texture_from_gltf(directory,
                    options,
                    *asset,
                    TextureType::SpecularGlossiness,
                    src.specularGlossiness->specularGlossinessTexture.value());
                if (exp.has_value()) {
                  textures.emplace_back(std::move(exp.value()));
                }
                else {
                  MR_ERROR(
                      "Loading Specular Glossiness texture - ", exp.error());
                }
              }
            },
            [&] {
              if (src.emissiveTexture.has_value()) {
                auto exp = get_texture_from_gltf(directory,
                    options,
                    *asset,
                    TextureType::EmissiveColor,
                    src.emissiveTexture.value());
                if (exp.has_value()) {
                  textures.emplace_back(std::move(exp.value()));
                }
                else {
                  MR_ERROR("Loading Emissive texture - ", exp.error());
                }
              }
            });

        dst.textures.resize(textures.size());
        tbb::parallel_for<int>(0, textures.size(), [&dst, &textures](int i) {
          dst.textures[i] = std::move(textures[i]);
        });
      });

  return materials;
}

static Model::Lights get_lights_from_asset(fastgltf::Asset *asset)
{
  ZoneScoped;

  Model::Lights lights;

  for (auto &light : asset->lights) {
    switch (light.type) {
    case fastgltf::LightType::Directional:
      lights.directionals.emplace_back(
          light.color.x(), light.color.y(), light.color.z(), light.intensity);
      break;
    case fastgltf::LightType::Point:
      lights.points.emplace_back(
          light.color.x(), light.color.y(), light.color.z(), light.intensity);
      break;
    case fastgltf::LightType::Spot:
      lights.spots.emplace_back(light.color.x(),
          light.color.y(),
          light.color.z(),
          light.intensity,
          light.innerConeAngle.value(),
          light.outerConeAngle.value());
      break;
    }
  }

  return lights;
}
} // namespace

void add_loader_nodes(FlowGraph &graph, const Options &options)
{
  ZoneScoped;

  graph.asset_loader =
      std::make_unique<tbb::flow::input_node<fastgltf::Asset *>>(graph.graph,
          [&graph](oneapi::tbb::flow_control &fc) -> fastgltf::Asset * {
            if (graph.model) {
              fc.stop();
              return nullptr;
            }

            ZoneScoped;
            graph.asset = get_asset_from_path(graph.path);
            if (!graph.asset) {
              MR_ERROR(
                  "Failed to load asset from path: {}", graph.path.string());
              fc.stop();
              return nullptr;
            }

            graph.model = std::make_unique<Model>();

            return &graph.asset.value();
          });

  graph.meshes_load = std::make_unique<
      tbb::flow::function_node<fastgltf::Asset *, fastgltf::Asset *>>(
      graph.graph,
      tbb::flow::unlimited,
      [&graph, &options](fastgltf::Asset *asset) -> fastgltf::Asset * {
        if (asset != nullptr) {
          ZoneScoped;
          graph.model->meshes = get_meshes_from_asset(options, asset);
        }
        return asset;
      });

  graph.materials_load =
      std::make_unique<tbb::flow::function_node<fastgltf::Asset *>>(graph.graph,
          tbb::flow::unlimited,
          [&graph, &options](fastgltf::Asset *asset) {
            if (asset != nullptr) {
              ZoneScoped;
              graph.model->materials = get_materials_from_asset(
                  graph.path.parent_path(), &graph.asset.value(), options);
            }
          });

  graph.lights_load =
      std::make_unique<tbb::flow::function_node<fastgltf::Asset *>>(
          graph.graph, tbb::flow::unlimited, [&graph](fastgltf::Asset *asset) {
            if (asset != nullptr) {
              ZoneScoped;
              graph.model->lights = get_lights_from_asset(asset);
            }
          });

  tbb::flow::make_edge(*graph.asset_loader, *graph.meshes_load);
  tbb::flow::make_edge(*graph.asset_loader, *graph.materials_load);
  tbb::flow::make_edge(*graph.asset_loader, *graph.lights_load);
}

/**
 * Load a source asset (currently glTF) and convert it into runtime \ref Model.
 * Returns std::nullopt on parse or IO errors; logs details via MR_ logging.
 */
std::optional<Model> load(std::filesystem::path path, Options options)
{
  FlowGraph graph;
  graph.path = std::move(path);

  add_loader_nodes(graph, options);

  graph.asset_loader->activate();
  graph.graph.wait_for_all();

  return graph.model.get() == nullptr ? std::nullopt
                                      : std::optional(std::move(*graph.model));
}
} // namespace importer
} // namespace mr
