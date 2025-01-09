#pragma once

#include "def.hpp"

namespace mr {
  struct Optimizer {
      /*
     * Indexing
     * (OPT) Simplification
     * Cache
     * Overdraw
     * Fetch
     * Quantization
     * Shadow Indexing
     */
      static inline VertexAttribsMap addTask(VertexAttribsMap m)
      {
        std::vector<meshopt_Stream> streams = {
          {   .data = m.positions.data(),
           .size = sizeof(float) * 3,
           .stride = sizeof(float) * 3},
          {     .data = m.normals.data(),
           .size = sizeof(float) * 3,
           .stride = sizeof(float) * 3},
          {.data = m.texcoords[0].data(),
           .size = sizeof(float) * 2,
           .stride = sizeof(float) * 2},
        };

        std::vector<unsigned int> remap(m.indices.size());
        size_t vertex_count =
          meshopt_generateVertexRemapMulti(remap.data(),
                                           m.indices.data(),
                                           m.indices.size(),
                                           m.positions.size(),
                                           streams.data(),
                                           streams.size());

        std::vector poscopy = m.positions;
        meshopt_remapVertexBuffer(poscopy.data(),
                                  m.positions.data(),
                                  m.positions.size(),
                                  sizeof(m.positions[0]),
                                  remap.data());
        m.positions = std::move(poscopy);

        std::vector indcopy = m.indices;
        meshopt_remapIndexBuffer(
          indcopy.data(), m.indices.data(), m.indices.size(), remap.data());
        m.indices = std::move(indcopy);

        for (auto &[name, buf] : m) {
          if (buf) {
            std::vector copy = buf.value();
            meshopt_remapVertexBuffer(copy.data(),
                                      buf->data(),
                                      buf->size(),
                                      sizeof(buf->operator[](0)),
                                      remap.data());
            buf = std::move(copy);
          }
        }

        meshopt_optimizeVertexCache(m.indices.data(),
                                    m.indices.data(),
                                    m.indices.size(),
                                    m.positions.size());
        meshopt_optimizeOverdraw(m.indices.data(),
                                 m.indices.data(),
                                 m.indices.size(),
                                 &m.positions[0][0],
                                 vertex_count,
                                 12,
                                 1.05f);
        // meshopt_optimizeVertexFetchRemap(m.indices.data(), m.indices.data(), m.indices.size(), m.positions.size());

        return m;
      }
  };
} // namespace mr
