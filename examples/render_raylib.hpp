#include "raylib.h"
#include "raymath.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <cmath> // For atan2f, sinf, cosf

inline void render(std::vector<glm::vec3> positions, std::vector<uint32_t> indices) {
  constexpr int screenWidth = 1800;
  constexpr int screenHeight = 1600;

  InitWindow(screenWidth, screenHeight, "Mesh Rendering");
  SetTargetFPS(60);

  // Setup camera
  Camera3D camera = { 0 };
  camera.position = Vector3{ 0.0f, 0.0f, 10.0f }; // Camera position
  camera.target = Vector3{ 0.0f, 0.0f, 0.0f };      // Camera looking at point
  camera.up = Vector3{ 0.0f, 1.0f, 0.0f };          // Camera up vector
  camera.fovy = 90.0f;                                // Camera field-of-view Y
  camera.projection = CAMERA_PERSPECTIVE;             // Camera projection type

  // Create mesh and copy data
  Mesh mesh = { 0 };
  mesh.vertexCount = positions.size();
  mesh.triangleCount = indices.size() / 3;

  mesh.vertices = (float*)positions.data();

  // Allocate and copy indices
  mesh.indices = (unsigned short*)MemAlloc(indices.size() * sizeof(unsigned short));
  for (size_t i = 0; i < indices.size(); ++i) {
    mesh.indices[i] = indices[i];
  }

  // Upload mesh data to GPU
  UploadMesh(&mesh, false);

  DisableCursor();
  SetMousePosition(screenWidth / 2, screenHeight / 2);

  // Load model from mesh
  Model model = LoadModelFromMesh(mesh);

  // Camera movement settings
  float moveSpeed = 0.02f;
  float mouseSensitivity = 0.001f;
  float cameraPitch = 0.0f; // Up/down rotation (radians)
  float cameraYaw = 0.0f;   // Left/right rotation (radians)

  // Main loop
  while (!WindowShouldClose()) {
    // Update
    // --- Mouse Look (Camera Rotation) ---
    Vector2 mouseDelta = GetMouseDelta();
    cameraYaw   -= mouseDelta.x * mouseSensitivity;
    cameraPitch -= mouseDelta.y * mouseSensitivity;

    // Clamp pitch to avoid flipping
    cameraPitch = Clamp(cameraPitch, -1.5f, 1.5f); // ~85 degrees up/down

    // Calculate camera target (forward direction)
    Vector3 forward = {
      cosf(cameraYaw) * cosf(cameraPitch),
      sinf(cameraPitch),
      sinf(cameraYaw) * cosf(cameraPitch)
    };
    camera.target = Vector3Add(camera.position, forward);

    // --- Keyboard Movement (WASD) ---
    Vector3 right = { cosf(cameraYaw + 1.57f), 0.0f, sinf(cameraYaw + 1.57f) };
    Vector3 up = { 0.0f, 1.0f, 0.0f };

    if (IsKeyDown(KEY_W)) camera.position = Vector3Add(camera.position, Vector3Scale(forward, moveSpeed));
    if (IsKeyDown(KEY_S)) camera.position = Vector3Subtract(camera.position, Vector3Scale(forward, moveSpeed));
    if (IsKeyDown(KEY_A)) camera.position = Vector3Subtract(camera.position, Vector3Scale(right, moveSpeed));
    if (IsKeyDown(KEY_D)) camera.position = Vector3Add(camera.position, Vector3Scale(right, moveSpeed));
    if (IsKeyDown(KEY_SPACE)) camera.position = Vector3Add(camera.position, Vector3Scale(up, moveSpeed));
    if (IsKeyDown(KEY_LEFT_SHIFT)) camera.position = Vector3Subtract(camera.position, Vector3Scale(up, moveSpeed));

    // Update camera target (to keep looking forward)
    camera.target = Vector3Add(camera.position, forward);

    // Draw
    BeginDrawing();
    ClearBackground(RAYWHITE);
    BeginMode3D(camera);
    DrawModel(model, {0, 0, 0}, 1.0f, RED);
    EndMode3D();
    DrawFPS(10, 10);
    EndDrawing();
  }

  // Cleanup
  UnloadMesh(mesh);
  UnloadModel(model);
  CloseWindow();
}
