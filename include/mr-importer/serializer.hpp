#pragma once

/**
 * \file serializator.hpp
 * \brief Public API for mesh/model serialization and deserialization.
 */

#include "assets.hpp"
#include <optional>
#include <string>

namespace mr {
inline namespace importer {
/**
 * \brief Serialize a Model to binary file.
 *
 * Saves the entire Model structure (meshes, materials, lights) to a binary
 * file.
 * \param model The model to serialize.
 * \param filepath Path to save the serialized data.
 * \return true if serialization succeeded, false otherwise.
 */
bool serialize(const Model &model, const std::string &filepath);

/**
 * \brief Deserialize a Model from binary file.
 *
 * Loads a previously serialized Model from a binary file.
 * \param filepath Path to the serialized data.
 * \return Deserialized model, or std::nullopt on failure.
 */
std::optional<Model> deserialize(const std::string &filepath);

/**
 * \brief Serialize a Mesh to binary file.
 *
 * Saves a single Mesh structure to a binary file.
 * \param mesh The mesh to serialize.
 * \param filepath Path to save the serialized data.
 * \return true if serialization succeeded, false otherwise.
 */
bool serialize(const Mesh &mesh, const std::string &filepath);

/**
 * \brief Deserialize a Mesh from binary file.
 *
 * Loads a previously serialized Mesh from a binary file.
 * \param filepath Path to the serialized data.
 * \return Deserialized mesh, or std::nullopt on failure.
 */
std::optional<Mesh> deserialize_mesh(const std::string &filepath);

/**
 * \brief Serialize a MaterialData to binary file.
 *
 * Saves a single MaterialData structure to a binary file.
 * \param material The material to serialize.
 * \param filepath Path to save the serialized data.
 * \return true if serialization succeeded, false otherwise.
 */
bool serialize(const MaterialData &material, const std::string &filepath);

/**
 * \brief Deserialize a MaterialData from binary file.
 *
 * Loads a previously serialized MaterialData from a binary file.
 * \param filepath Path to the serialized data.
 * \return Deserialized material, or std::nullopt on failure.
 */
std::optional<MaterialData> deserialize_material(const std::string &filepath);
} // namespace importer
} // namespace mr
