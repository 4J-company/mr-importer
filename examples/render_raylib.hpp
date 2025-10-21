#include "raylib.h"
#include "raymath.h"
#include <memory>
#include <filesystem>
#include <cmath>

// FPS Camera class
class FPSCamera {
  private:
    Camera3D camera;
    Vector3 position;
    Vector3 front;
    Vector3 up;
    Vector3 right;
    Vector3 worldUp;

    float yaw;
    float pitch;

    float movementSpeed;
    float mouseSensitivity;
    float zoom;

  public:
    FPSCamera(Vector3 startPosition = {0.0f, 0.0f, 3.0f}, 
        Vector3 startWorldUp = {0.0f, 1.0f, 0.0f}, 
        float startYaw = -90.0f, float startPitch = 0.0f)
      : position(startPosition), worldUp(startWorldUp), yaw(startYaw), pitch(startPitch),
      movementSpeed(5.0f), mouseSensitivity(0.1f), zoom(45.0f) {

        front = {0.0f, 0.0f, -1.0f};
        updateCameraVectors();

        camera.position = position;
        camera.target = {position.x + front.x, position.y + front.y, position.z + front.z};
        camera.up = up;
        camera.fovy = zoom;
        camera.projection = CAMERA_PERSPECTIVE;
      }

    void Update(float deltaTime) {
      // Keyboard input for movement
      if (IsKeyDown(KEY_W)) {
        position.x += front.x * movementSpeed * deltaTime;
        position.y += front.y * movementSpeed * deltaTime;
        position.z += front.z * movementSpeed * deltaTime;
      }
      if (IsKeyDown(KEY_S)) {
        position.x -= front.x * movementSpeed * deltaTime;
        position.y -= front.y * movementSpeed * deltaTime;
        position.z -= front.z * movementSpeed * deltaTime;
      }
      if (IsKeyDown(KEY_A)) {
        position.x -= right.x * movementSpeed * deltaTime;
        position.y -= right.y * movementSpeed * deltaTime;
        position.z -= right.z * movementSpeed * deltaTime;
      }
      if (IsKeyDown(KEY_D)) {
        position.x += right.x * movementSpeed * deltaTime;
        position.y += right.y * movementSpeed * deltaTime;
        position.z += right.z * movementSpeed * deltaTime;
      }

      // Mouse input for looking around
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 mouseDelta = GetMouseDelta();
        mouseDelta.x *= mouseSensitivity;
        mouseDelta.y *= mouseSensitivity;

        yaw += mouseDelta.x;
        pitch -= mouseDelta.y;

        // Constrain pitch to avoid flipping
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        updateCameraVectors();
      }

      // Update camera position and target
      camera.position = position;
      camera.target = {position.x + front.x, position.y + front.y, position.z + front.z};
    }

    Camera3D GetCamera() const { return camera; }
    Vector3 GetPosition() const { return position; }
    Vector3 GetFront() const { return front; }

    void SetMovementSpeed(float speed) { movementSpeed = speed; }
    void SetMouseSensitivity(float sensitivity) { mouseSensitivity = sensitivity; }
    void SetPosition(Vector3 newPosition) { position = newPosition; }

  private:
    void updateCameraVectors() {
      // Calculate new front vector
      Vector3 newFront;
      newFront.x = cosf(yaw * DEG2RAD) * cosf(pitch * DEG2RAD);
      newFront.y = sinf(pitch * DEG2RAD);
      newFront.z = sinf(yaw * DEG2RAD) * cosf(pitch * DEG2RAD);
      front = Vector3Normalize(newFront);

      // Re-calculate right and up vectors using raylib's Vector3CrossProduct
      right = Vector3Normalize(Vector3CrossProduct(front, worldUp));
      up = Vector3Normalize(Vector3CrossProduct(right, front));
    }
};

class ModelRenderer {
  private:
    mr::importer::Model model;
    std::vector<Model> raylibModels;

  public:
    bool LoadModel(const std::filesystem::path& filepath) {
      // Import the model using your library
      auto result = mr::importer::import(filepath);
      if (!result) {
        TraceLog(LOG_ERROR, "Failed to load model: %s", filepath.c_str());
        return false;
      }

      model = std::move(*result);
      return ConvertToRaylib();
    }

  private:
    bool ConvertToRaylib() {
      raylibModels.clear();

      // For each mesh in your model, create a raylib model
      for (const auto& mesh : model.meshes) {
        if (mesh.positions.empty() || mesh.lods.empty()) {
          continue;
        }

        // Create raylib mesh
        Mesh raylibMesh = {0};

        // Convert vertex positions
        raylibMesh.vertexCount = mesh.positions.size();
        raylibMesh.vertices = (float*)MemAlloc(raylibMesh.vertexCount * 3 * sizeof(float));

        for (size_t i = 0; i < mesh.positions.size(); ++i) {
          raylibMesh.vertices[i * 3] = mesh.positions[i][0];
          raylibMesh.vertices[i * 3 + 1] = mesh.positions[i][1];
          raylibMesh.vertices[i * 3 + 2] = mesh.positions[i][2];
        }

        // Convert vertex attributes
        if (!mesh.attributes.empty()) {
          raylibMesh.normals = (float*)MemAlloc(raylibMesh.vertexCount * 3 * sizeof(float));
          raylibMesh.texcoords = (float*)MemAlloc(raylibMesh.vertexCount * 2 * sizeof(float));

          for (size_t i = 0; i < mesh.attributes.size() && i < mesh.positions.size(); ++i) {
            // Normals
            if (!mesh.attributes[i].normal.empty()) {
              raylibMesh.normals[i * 3] = mesh.attributes[i].normal[0];
              raylibMesh.normals[i * 3 + 1] = mesh.attributes[i].normal[1];
              raylibMesh.normals[i * 3 + 2] = mesh.attributes[i].normal[2];
            }

            // Texture coordinates
            raylibMesh.texcoords[i * 2] = mesh.attributes[i].texcoord[0];
            raylibMesh.texcoords[i * 2 + 1] = mesh.attributes[i].texcoord[1];
          }
        }

        // Convert indices (use first LOD)
        const auto& lod = mesh.lods[0];
        if (!lod.indices.empty()) {
          raylibMesh.triangleCount = lod.indices.size() / 3;
          raylibMesh.indices = (unsigned short*)MemAlloc(lod.indices.size() * sizeof(unsigned short));

          for (size_t i = 0; i < lod.indices.size(); ++i) {
            raylibMesh.indices[i] = lod.indices[i];
          }
        }

        // Upload mesh to GPU
        UploadMesh(&raylibMesh, true);

        // Create raylib model from mesh
        Model raylibModel = LoadModelFromMesh(raylibMesh);

        // Apply material if available
        if (mesh.material < model.materials.size()) {
          const auto& mat = model.materials[mesh.material];
          raylibModel.materials[0] = CreateMaterialFromImported(mat);
        }

        raylibModels.push_back(raylibModel);
      }

      return !raylibModels.empty();
    }

    Material CreateMaterialFromImported(const mr::importer::MaterialData& importedMat) {
      Material mat = LoadMaterialDefault();

      // Set base color
      mat.maps[MATERIAL_MAP_DIFFUSE].color = {
        (unsigned char)(importedMat.constants.base_color_factor.r() * 255),
        (unsigned char)(importedMat.constants.base_color_factor.g() * 255),
        (unsigned char)(importedMat.constants.base_color_factor.b() * 255),
        (unsigned char)(importedMat.constants.base_color_factor.a() * 255)
      };

      // Load textures
      for (const auto& texture : importedMat.textures) {
        if (texture.type == mr::importer::TextureType::BaseColor) {
          Image image = ConvertImageData(texture.image);
          Texture2D tex = LoadTextureFromImage(image);
          UnloadImage(image);
          mat.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
        }
        // Add more texture type mappings as needed
      }

      return mat;
    }

    Image ConvertImageData(const mr::importer::ImageData& imageData) {
      Image image = {0};
      image.width = imageData.width;
      image.height = imageData.height;
      image.mipmaps = 1;
      image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

      // Allocate memory for image data
      image.data = MemAlloc(imageData.width * imageData.height * 4);

      // Convert from float RGBA to byte RGBA
      // This assumes imageData.pixels contains float RGBA data
      const float* sourcePixels = reinterpret_cast<const float*>(imageData.pixels.get());
      unsigned char* destPixels = static_cast<unsigned char*>(image.data);

      for (int i = 0; i < imageData.width * imageData.height; ++i) {
        destPixels[i * 4] = (unsigned char)(sourcePixels[i * 4] * 255.0f);
        destPixels[i * 4 + 1] = (unsigned char)(sourcePixels[i * 4 + 1] * 255.0f);
        destPixels[i * 4 + 2] = (unsigned char)(sourcePixels[i * 4 + 2] * 255.0f);
        destPixels[i * 4 + 3] = (unsigned char)(sourcePixels[i * 4 + 3] * 255.0f);
      }

      return image;
    }

  public:
    void Draw() {
      for (size_t i = 0; i < raylibModels.size(); ++i) {
        // Apply transform if available
        if (!model.meshes[i].transforms.empty()) {
          const auto& transform = model.meshes[i].transforms[0];

          // Convert to raylib Matrix
          Matrix matrix = {
            transform[0][0], transform[0][1], transform[0][2], transform[0][3],
            transform[1][0], transform[1][1], transform[1][2], transform[1][3],
            transform[2][0], transform[2][1], transform[2][2], transform[2][3],
            transform[3][0], transform[3][1], transform[3][2], transform[3][3]
          };

          DrawModel(raylibModels[i], (Vector3){0, 0, 0}, 1.0f, WHITE);
        } else {
          DrawModel(raylibModels[i], (Vector3){0, 0, 0}, 1.0f, WHITE);
        }
      }
    }

    void DrawWithTransform(Vector3 position, float scale, Color tint) {
      for (auto& model : raylibModels) {
        DrawModel(model, position, scale, tint);
      }
    }

    void DrawEx(Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale, Color tint) {
      for (auto& model : raylibModels) {
        DrawModelEx(model, position, rotationAxis, rotationAngle, scale, tint);
      }
    }

    ~ModelRenderer() {
      for (auto& model : raylibModels) {
        UnloadModel(model);
      }
    }
};
