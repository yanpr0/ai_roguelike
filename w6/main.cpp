#include "raylib.h"
#include <flecs.h>
#include <algorithm>

#include "ecsTypes.h"
#include "shootEmUp.h"
#include "dungeonGen.h"


static void update_camera(flecs::world &ecs)
{
  static auto cameraQuery = ecs.query<Camera2D>();
  static auto playerQuery = ecs.query<const Position, const IsPlayer>();

  /*cameraQuery*/ecs.each([&](Camera2D &cam)
  {
    /*playerQuery*/ecs.each([&](const Position &pos, const IsPlayer &)
    {
      cam.target.x += (pos.x - cam.target.x) * 0.1f;
      cam.target.y += (pos.y - cam.target.y) * 0.1f;
    });
  });
}


int main(int /*argc*/, const char ** /*argv*/)
{
  int width = 1920;
  int height = 1080;
  InitWindow(width, height, "w6 AI MIPT");

  const int scrWidth = GetMonitorWidth(0);
  const int scrHeight = GetMonitorHeight(0);
  if (scrWidth < width || scrHeight < height)
  {
    width = std::min(scrWidth, width);
    height = std::min(scrHeight - 150, height);
    SetWindowSize(width, height);
  }

  flecs::world ecs;

  size_t dungWidth = 50;
  size_t dungHeight = 50;
  size_t spawn_cnt = 3;

  auto level_up = [&](){
    dungWidth *= 1.2;
    dungHeight *= 1.2;
    spawn_cnt = spawn_cnt * 1.2 + 1;
  };

  init_shoot_em_up(ecs, dungWidth, dungHeight, spawn_cnt);

  //Texture2D bgTex = LoadTexture("assets/background.png"); // TODO: move to ecs

  Camera2D camera = { {0, 0}, {0, 0}, 0.f, 1.f };
  camera.target = Vector2{ 0.f, 0.f };
  camera.offset = Vector2{ width * 0.5f, height * 0.5f };
  camera.rotation = 0.f;
  camera.zoom = 1;
  ecs.entity("camera")
    .set(Camera2D{camera});

  static auto cameraQuery = ecs.query<Camera2D>();

  SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
  while (!WindowShouldClose())
  {
    process_game(ecs);
    update_camera(ecs);

    BeginDrawing();
      ClearBackground(BLACK);
      /*cameraQuery*/ecs.each([&](Camera2D &cam) { BeginMode2D(cam); });
        //DrawTextureTiled(bgTex, {0, 0, 512, 512}, {0, 0, 10240, 10240}, {0, 0}, 0.f, 1.f, WHITE);
        //constexpr int tiles = 20;
        //DrawTextureQuad(bgTex, {tiles, tiles}, {0, 0},
        //    {-512 * tiles / 2, -512 * tiles / 2, 512 * tiles, 512 * tiles}, GRAY);
        if (!ecs.progress())
        {
          ecs.reset();
          level_up();
          ecs.entity("camera").set(Camera2D{camera});
          init_shoot_em_up(ecs, dungWidth, dungHeight, spawn_cnt);
        }
      EndMode2D();
      // Advance to next frame. Process submitted rendering primitives.
    EndDrawing();
  }

  CloseWindow();

  return 0;
}
