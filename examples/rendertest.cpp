#include <filesystem>

#include "mr-importer/importer.hpp"

#include "render_raylib.hpp"

// Example usage with FPS camera
int main(int argc, char** argv) {
  const int screenWidth = 1920;
  const int screenHeight = 1080;

  InitWindow(screenWidth, screenHeight, "Model Viewer with FPS Camera");
  SetTargetFPS(60);

  // Create FPS camera
  FPSCamera fpsCamera({5.0f, 2.0f, 10.0f});

  // Load model
  ModelRenderer modelRenderer;
  bool modelLoaded = modelRenderer.LoadModel(argv[1]);

  if (!modelLoaded) {
    TraceLog(LOG_WARNING, "Could not load model");
  }

  while (!WindowShouldClose()) {
    float deltaTime = GetFrameTime();

    // Update FPS camera
    fpsCamera.Update(deltaTime);

    // Draw
    BeginDrawing();
    ClearBackground(SKYBLUE);

    BeginMode3D(fpsCamera.GetCamera());

    if (modelLoaded) {
      modelRenderer.Draw();
    } else {
      // Fallback cube
      DrawCube((Vector3){0, 1, 0}, 2.0f, 2.0f, 2.0f, RED);
      DrawCubeWires((Vector3){0, 1, 0}, 2.0f, 2.0f, 2.0f, MAROON);
    }

    // Draw ground grid
    DrawGrid(20, 1.0f);

    EndMode3D();

    // Draw UI
    DrawFPS(10, 10);
    DrawText("FPS Camera Controls:", 10, 40, 20, DARKGRAY);
    DrawText("WASD - Move, Left Mouse Button - Look around", 10, 70, 20, DARKGRAY);
    DrawText(TextFormat("Position: (%.1f, %.1f, %.1f)", 
          fpsCamera.GetPosition().x, 
          fpsCamera.GetPosition().y, 
          fpsCamera.GetPosition().z), 
        10, 100, 20, DARKGRAY);

    EndDrawing();
  }

  CloseWindow();
  return 0;
}
