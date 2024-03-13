#include "dungeonUtils.h"
#include "raylib.h"

Position dungeon::find_walkable_tile(flecs::world &ecs)
{
  static auto dungeonDataQuery = ecs.query<const DungeonData>();

  Position res{0, 0};
  /*dungeonDataQuery*/ecs.each([&](const DungeonData &dd)
  {
    // prebuild all walkable and get one of them
    std::vector<Position> posList;
    for (size_t y = 0; y < dd.height; ++y)
      for (size_t x = 0; x < dd.width; ++x)
        if (dd.tiles[y * dd.width + x] == dungeon::floor)
          posList.push_back(Position{(x + 0.01f) * dungeon::tile_size, (y + 0.01f) * dungeon::tile_size});
    size_t rndIdx = size_t(GetRandomValue(0, int(posList.size()) - 1));
    res = posList[rndIdx];
  });
  return res;
}

bool dungeon::is_tile_walkable(flecs::world &ecs, Position pos)
{
  static auto dungeonDataQuery = ecs.query<const DungeonData>();

  bool res = false;
  int x = pos.x / dungeon::tile_size;
  int y = pos.y / dungeon::tile_size;
  /*dungeonDataQuery*/ecs.each([&](const DungeonData &dd)
  {
    if (x < 0 || x >= int(dd.width) ||
        y < 0 || y >= int(dd.height))
      return;
    res = dd.tiles[size_t(y) * dd.width + size_t(x)] == dungeon::floor;
  });
  return res;
}

bool dungeon::is_tile_walkable(flecs::world &ecs, IntPos pos)
{
  return dungeon::is_tile_walkable(ecs, Position{pos.x * dungeon::tile_size, pos.y * dungeon::tile_size});
}
