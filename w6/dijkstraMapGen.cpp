#include "dijkstraMapGen.h"
#include "ecsTypes.h"
#include "dungeonUtils.h"

template<typename Callable>
static void query_dungeon_data(flecs::world &ecs, Callable c)
{
  static auto dungeonDataQuery = ecs.query<const DungeonData>();

  dungeonDataQuery.each(c);
}

template<typename Callable>
static void query_characters_positions(flecs::world &ecs, Callable c)
{
  static auto characterPositionQuery = ecs.query<const Position, const Team>();

  characterPositionQuery.each(c);
}

constexpr float invalid_tile_value = 1e5f;

static void init_tiles(std::vector<float> &map, const DungeonData &dd)
{
  map.resize(dd.width * dd.height);
  for (float &v : map)
    v = invalid_tile_value;
}

// scan version, could be implemented as Dijkstra version as well
static void process_dmap(std::vector<float> &map, const DungeonData &dd)
{
  bool done = false;
  auto getMapAt = [&](size_t x, size_t y, float def)
  {
    if (x < dd.width && y < dd.width && dd.tiles[y * dd.width + x] == dungeon::floor)
      return map[y * dd.width + x];
    return def;
  };
  auto getMinNei = [&](size_t x, size_t y)
  {
    float val = map[y * dd.width + x];
    val = std::min(val, getMapAt(x - 1, y + 0, val));
    val = std::min(val, getMapAt(x + 1, y + 0, val));
    val = std::min(val, getMapAt(x + 0, y - 1, val));
    val = std::min(val, getMapAt(x + 0, y + 1, val));
    return val;
  };
  while (!done)
  {
    done = true;
    for (size_t y = 0; y < dd.height; ++y)
      for (size_t x = 0; x < dd.width; ++x)
      {
        const size_t i = y * dd.width + x;
        if (dd.tiles[i] != dungeon::floor)
          continue;
        const float myVal = getMapAt(x, y, invalid_tile_value);
        const float minVal = getMinNei(x, y);
        if (minVal < myVal - 1.f)
        {
          map[i] = minVal + 1.f;
          done = false;
        }
      }
  }
}

static std::pair<int, int> get_pos(const Position pos)
{
  Position foot_pos = pos + Position{0.45f * dungeon::tile_size, 0.85f * dungeon::tile_size};
  return {foot_pos.x / dungeon::tile_size, foot_pos.y / dungeon::tile_size};
}

void dmaps::gen_multiobject_approach_map(flecs::world &ecs, const std::vector<Position>& obj_pos, std::vector<float> &map)
{
  ecs.each([&](const DungeonData &dd)
  {
    init_tiles(map, dd);
    for (auto pos : obj_pos)
    {
      auto [x, y] = get_pos(pos);
      map[y * dd.width + x] = 0.f;
    }
    process_dmap(map, dd);
  });
}

void dmaps::gen_player_approach_map(flecs::world &ecs, std::vector<float> &map)
{
  ecs.each([&](const DungeonData &dd)
  {
    init_tiles(map, dd);
    ecs.each([&](const Position &pos, const Team &t)
    {
      if (t.team == 0) // player team hardcode
      {
        auto [x, y] = get_pos(pos);
        map[y * dd.width + x] = 0.f;
      }
    });
    process_dmap(map, dd);
  });
}

void dmaps::gen_player_flee_map(flecs::world &ecs, std::vector<float> &map)
{
  gen_player_approach_map(ecs, map);
  for (float &v : map)
    if (v < invalid_tile_value)
      v *= -1.2f;
  ecs.each([&](const DungeonData &dd)
  {
    process_dmap(map, dd);
  });
}

void dmaps::gen_hive_pack_map(flecs::world &ecs, std::vector<float> &map)
{
  static auto hiveQuery = ecs.query<const Position, const Hive>();
  ecs.each([&](const DungeonData &dd)
  {
    init_tiles(map, dd);
    /*hiveQuery*/ecs.each([&](const Position &pos, const Hive &)
    {
      auto [x, y] = get_pos(pos);
      map[y * dd.width + x] = 0.f;
    });
    process_dmap(map, dd);
  });
}

