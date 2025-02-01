#pragma once

#include <execution>

#include "def.hpp"

#include "extractor/extractor.hpp"
#include "optimizer/optimizer.hpp"
#include "parser.hpp"
#include "uploader.hpp"

namespace mr {
  template <auto policy> struct Composer {
      static inline bool addTask(std::fs::path path)
      {
        auto parsed = Parser::addTask(path);
        if (!parsed) {
          return false;
        }

        const auto &scene = parsed->scenes[*parsed->defaultScene];
        std::for_each(policy,
                      scene.nodeIndices.begin(),
                      scene.nodeIndices.end(),
                      [&](const auto &node_idx) {
                        const auto &node = parsed->nodes[node_idx];

                        if (node.meshIndex) {
                          const auto &mesh = parsed->meshes[*node.meshIndex];
                          std::for_each(
                            policy,
                            mesh.primitives.begin(),
                            mesh.primitives.end(),
                            [&](const auto &prim) {
                              auto extracted_data =
                                Extractor::addTask(*parsed, prim);

                              MR_INFO("Number of positions: {}",
                                      extracted_data.positions.size());
                              MR_INFO("Number of normals: {}",
                                      extracted_data.normals.size());
                              MR_INFO("Number of texcoords: {}",
                                      extracted_data.texcoords.size());
                              MR_INFO("Number of indices: {}\n",
                                      extracted_data.indices.size());

                              auto optimized_data =
                                Optimizer::addTask(std::move(extracted_data));
                              Uploader::addTask(std::move(optimized_data));
                            });
                        }
                      });

        return true;
      }
  };
} // namespace mr
