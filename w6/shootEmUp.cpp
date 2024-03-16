#include <algorithm>
#include <memory>
#include <raylib.h>
#include "shootEmUp.h"
#include "ecsTypes.h"
#include "rlikeObjects.h"
#include "steering.h"
#include "dungeonGen.h"
#include "dungeonUtils.h"
#include "dijkstraMapGen.h"
#include "math.h"
#include "blackboard.h"
#include "aiLibrary.h"
#include "pathfinder.h"

using dungeon::tile_size;

static void register_roguelike_systems(flecs::world &ecs)
{
  static auto playerPosQuery = ecs.query<const Position, const IsPlayer>();

  ecs.system<Velocity, const MoveSpeed, const Position, const IsPlayer>()
    .each([&](Velocity &vel, const MoveSpeed &ms, const Position pos, const IsPlayer)
    {
      bool left = IsKeyDown(KEY_LEFT);
      bool right = IsKeyDown(KEY_RIGHT);
      bool up = IsKeyDown(KEY_UP);
      bool down = IsKeyDown(KEY_DOWN);
      float dt = ecs.delta_time();
      Position up_left_hit = pos + Position{0.15f * dungeon::tile_size, 0.75f * dungeon::tile_size};
      Position up_right_hit = pos + Position{0.75f * dungeon::tile_size, 0.75f * dungeon::tile_size};
      Position down_left_hit = pos + Position{0.15f * dungeon::tile_size, 0.95f * dungeon::tile_size};
      Position down_right_hit = pos + Position{0.75f * dungeon::tile_size, 0.95f * dungeon::tile_size};
      float vx = ((left ? -1 : 0) + (right ? 1 : 0));
      Position delta = Position{vx * ms.speed, 0.0f} * dt;
      if (vx > 0.f)
      {
        if (!dungeon::is_tile_walkable(ecs, up_right_hit + delta) || !dungeon::is_tile_walkable(ecs, down_right_hit + delta))
        {
          vx = -0.01f;
        }
      }
      else if (vx < 0.f)
      {
        if (!dungeon::is_tile_walkable(ecs, up_left_hit + delta) || !dungeon::is_tile_walkable(ecs, down_left_hit + delta))
        {
          vx = 0.01f;
        }
      }
      float vy = ((up ? -1 : 0) + (down ? 1 : 0));
      delta = Position{0.0f, vy * ms.speed} * dt;
      if (vy > 0.f)
      {
        if (!dungeon::is_tile_walkable(ecs, down_left_hit + delta) || !dungeon::is_tile_walkable(ecs, down_right_hit + delta))
        {
          vy = -0.01f;
        }
      }
      else
      {
        if (!dungeon::is_tile_walkable(ecs, up_left_hit + delta) || !dungeon::is_tile_walkable(ecs, up_right_hit + delta))
        {
          vy = 0.01f;
        }
      }
      vel = Velocity{normalize(Velocity{vx, vy}) * ms.speed};
    });
  ecs.system<Position, const Velocity>()
    .each([&](Position &pos, const Velocity &vel)
    {
      pos += vel * ecs.delta_time();
    });
  ecs.system<Position, const IsPlayer>()
    .each([&](Position &pos, const IsPlayer)
    {
      Position exit_pos = *ecs.lookup("exit").get<Position>() + Position{dungeon::tile_size / 2, dungeon::tile_size / 2};
      Position foot_pos = pos + Position{0.45f * dungeon::tile_size, 0.85f * dungeon::tile_size};
      if (dist(exit_pos, foot_pos) <= dungeon::tile_size / 2)
      {
        ecs.quit();
      }
    });
  ecs.system<const Position, const Color>()
    .term<TextureSource>(flecs::Wildcard)
    .term<BackgroundTile>()
    .each([&](flecs::entity e, const Position &pos, const Color color)
    {
      const auto textureSrc = e.target<TextureSource>();
      DrawTextureQuad(*textureSrc.get<Texture2D>(),
          Vector2{1, 1}, Vector2{0, 0},
          Rectangle{float(pos.x) * dungeon::tile_size, float(pos.y) * dungeon::tile_size, dungeon::tile_size, dungeon::tile_size}, color);
    });
  ecs.system<const Position, const Color>()
    .term<TextureSource>(flecs::Wildcard).not_()
    .each([&](const Position &pos, const Color color)
    {
      const Rectangle rect = {float(pos.x) * dungeon::tile_size, float(pos.y) * dungeon::tile_size, dungeon::tile_size, dungeon::tile_size};
      DrawRectangleRec(rect, color);
    });
  ecs.system<const Position, const DungeonExit>()
    .each([&](const Position &pos, const DungeonExit)
    {
      DrawCircle(pos.x + dungeon::tile_size / 2, pos.y + dungeon::tile_size / 2, dungeon::tile_size / 2, RAYWHITE);
      for (int i = 1; i <= 3; ++i)
      {
        float r = int(ecs.get_info()->world_time_total * dungeon::tile_size / i) % int(dungeon::tile_size / 2);
        DrawCircleLines(pos.x + dungeon::tile_size / 2, pos.y + dungeon::tile_size / 2, dungeon::tile_size / 2 - r, BLACK);
      }
    });
  ecs.system<const Position, const MonsterSpawner>()
    .each([&](const Position &pos, const MonsterSpawner&)
    {
      DrawCircle(pos.x + dungeon::tile_size / 2, pos.y + dungeon::tile_size / 2, dungeon::tile_size / 2, BLACK);
      for (int i = 1; i <= 3; ++i)
      {
        float r = int(ecs.get_info()->world_time_total * dungeon::tile_size / i) % int(dungeon::tile_size / 2);
        DrawCircleLines(pos.x + dungeon::tile_size / 2, pos.y + dungeon::tile_size / 2, r, WHITE);
      }
    });
  ecs.system<const Position, const Color>()
    .term<TextureSource>(flecs::Wildcard)
    .term<BackgroundTile>().not_()
    .each([&](flecs::entity e, const Position &pos, const Color color)
    {
      const auto textureSrc = e.target<TextureSource>();
      DrawTextureQuad(*textureSrc.get<Texture2D>(),
          Vector2{1, 1}, Vector2{0, 0},
          Rectangle{float(pos.x), float(pos.y), dungeon::tile_size, dungeon::tile_size}, color);
    });
  ecs.system<const Position, const Hitpoints>()
    .each([&](const Position &pos, const Hitpoints &hp)
    {
      constexpr float hpPadding = 0.05f;
      const float hpWidth = 1.f - 2.f * hpPadding;
      const Rectangle underRect = {pos.x + hpPadding * dungeon::tile_size, pos.y - 0.25 * dungeon::tile_size,
                                   hpWidth * dungeon::tile_size, 0.1f * dungeon::tile_size};
      DrawRectangleRec(underRect, BLACK);
      const Rectangle hpRect = {pos.x + hpPadding * dungeon::tile_size, pos.y - 0.25 * dungeon::tile_size,
                                hp.hitpoints / 100.f * hpWidth * dungeon::tile_size, 0.1f * dungeon::tile_size};
      DrawRectangleRec(hpRect, RED);
    });

  ecs.system<Texture2D>()
    .each([&](Texture2D &tex)
    {
      SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    });

  ecs.system<MonsterSpawner, const Position>()
    .each([&](MonsterSpawner &ms, const Position& pos)
    {
      //playerPosQuery.each([&](const Position &pp, const IsPlayer &)
      {
        ms.timeToSpawn -= ecs.delta_time();
        while (ms.timeToSpawn < 0.f)
        {
          int v = GetRandomValue(0, steer::Type::Num - 1);
          const Color colors[steer::Type::Num] = {WHITE, RED, BLUE, GREEN};
          //const float distances[steer::Type::Num] = {800.f, 800.f, 300.f, 300.f};
          //const float dist = distances[st];
          //constexpr int angRandMax = 1 << 16;
          //const float angle = float(GetRandomValue(0, angRandMax)) / float(angRandMax) * PI * 2.f;
          Color col = colors[v];

          flecs::entity e = steer::create_steerer(create_monster(ecs, pos, col, "minotaur_tex"));
          e.set(Blackboard{});
          BehNode *root =
            utility_selector({
              std::make_pair(
                move_to_entity(e, "approach_enemy"),
                [](Blackboard &bb)
                {
                  const float hp = bb.get<float>("hp");
                  const float alliesNum = bb.get<float>("alliesNum");
                  return 2.f * hp + 63.f * sqrtf(alliesNum);
                }
              ),
              std::make_pair(
                flee(e, "flee_enemy"),
                [](Blackboard &bb)
                {
                  const float hp = bb.get<float>("hp");
                  const float alliesNum = bb.get<float>("alliesNum");
                  return 4.2f * (100.f - hp) - 50 * sqrtf(alliesNum);
                }
              )
            });
          e.add<WorldInfoGatherer>();
          e.set(BehaviourTree{root});

          ms.timeToSpawn += ms.timeBetweenSpawns;
        }
      }
    });

  static auto cameraQuery = ecs.query<const Camera2D>();
  ecs.system<const DungeonPortals, const DungeonData>()
    .each([&](const DungeonPortals &dp, const DungeonData &dd)
    {
      size_t w = dd.width;
      size_t ts = dp.tileSplit;
      for (size_t y = 0; y < dd.height / ts; ++y)
        DrawLineEx(Vector2{0.f, y * ts * tile_size},
                   Vector2{dd.width * tile_size, y * ts * tile_size}, 1.f, GetColor(0xff000080));
      for (size_t x = 0; x < dd.width / ts; ++x)
        DrawLineEx(Vector2{x * ts * tile_size, 0.f},
                   Vector2{x * ts * tile_size, dd.height * tile_size}, 1.f, GetColor(0xff000080));
      /*cameraQuery*/ecs.each([&](Camera2D cam)
      {
        Vector2 mousePosition = GetScreenToWorld2D(GetMousePosition(), cam);
        size_t wd = w / ts;
        for (size_t y = 0; y < dd.height / ts; ++y)
        {
          if (mousePosition.y < y * ts * tile_size || mousePosition.y > (y + 1) * ts * tile_size)
            continue;
          for (size_t x = 0; x < dd.width / ts; ++x)
          {
            if (mousePosition.x < x * ts * tile_size || mousePosition.x > (x + 1) * ts * tile_size)
              continue;
            for (size_t idx : dp.tilePortalsIndices[y * wd + x])
            {
              const PathPortal &portal = dp.portals[idx];
              Rectangle rect{portal.startX * tile_size, portal.startY * tile_size,
                             (portal.endX - portal.startX + 1) * tile_size,
                             (portal.endY - portal.startY + 1) * tile_size};
              DrawRectangleLinesEx(rect, 3, BLACK);
            }
          }
        }
        for (const PathPortal &portal : dp.portals)
        {
          Rectangle rect{portal.startX * tile_size, portal.startY * tile_size,
                         (portal.endX - portal.startX + 1) * tile_size,
                         (portal.endY - portal.startY + 1) * tile_size};
          Vector2 fromCenter{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
          DrawRectangleLinesEx(rect, 1, WHITE);
          if (mousePosition.x < rect.x || mousePosition.x > rect.x + rect.width ||
              mousePosition.y < rect.y || mousePosition.y > rect.y + rect.height)
            continue;
          DrawRectangleLinesEx(rect, 4, WHITE);
          for (const PortalConnection &conn : portal.conns)
          {
            const PathPortal &endPortal = dp.portals[conn.connIdx];
            Vector2 toCenter{(endPortal.startX + endPortal.endX + 1) * tile_size * 0.5f,
                             (endPortal.startY + endPortal.endY + 1) * tile_size * 0.5f};
            DrawLineEx(fromCenter, toCenter, 1.f, WHITE);
            DrawText(TextFormat("%d", int(conn.score)),
                     (fromCenter.x + toCenter.x) * 0.5f,
                     (fromCenter.y + toCenter.y) * 0.5f,
                     16, WHITE);
          }
        }
      });
    });

    ecs.system<const DungeonPortals, const DungeonData>()
    .each([&](const DungeonPortals &dp, const DungeonData &dd)
    {
      auto p = ecs.lookup("player");
      if (p != 0 && p.is_alive())
      {
        Position exit_pos = *ecs.lookup("exit").get<Position>() + Position{dungeon::tile_size / 2, dungeon::tile_size / 2};
        Position foot_pos = *p.get<Position>() + Position{0.45f * dungeon::tile_size, 0.85f * dungeon::tile_size};
        auto path = find_approximated_path(dp, dd, foot_pos, exit_pos);
        for (int i = 0; i + 1 < path.size(); ++i)
        {
          auto [x1, y1] = path[i];
          auto [x2, y2] = path[i + 1];
          DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, 5.f, RED);
          DrawText(TextFormat("%d", int(path.size()) - i - 1), x1, y1, 16, WHITE);
        }
        DrawText(TextFormat("%d", 0), path.back().x, path.back().y, 16, WHITE);
      }
    });
  steer::register_systems(ecs);
}


static void init_dungeon(flecs::world &ecs, char *tiles, size_t w, size_t h)
{
  flecs::entity wallTex = ecs.entity("wall_tex")
    .set(Texture2D{LoadTexture("assets/wall.png")});
  flecs::entity floorTex = ecs.entity("floor_tex")
    .set(Texture2D{LoadTexture("assets/floor.png")});

  std::vector<char> dungeonData;
  dungeonData.resize(w * h);
  for (size_t y = 0; y < h; ++y)
    for (size_t x = 0; x < w; ++x)
      dungeonData[y * w + x] = tiles[y * w + x];
  ecs.entity("dungeon")
    .set(DungeonData{dungeonData, w, h});

  for (size_t y = 0; y < h; ++y)
    for (size_t x = 0; x < w; ++x)
    {
      char tile = tiles[y * w + x];
      flecs::entity tileEntity = ecs.entity()
        .add<BackgroundTile>()
        .set(Position{int(x), int(y)})
        .set(Color{255, 255, 255, 255});
      if (tile == dungeon::wall)
        tileEntity.add<TextureSource>(wallTex);
      else if (tile == dungeon::floor)
        tileEntity.add<TextureSource>(floorTex);
    }
    prebuild_map(ecs);
    printf("+++++++++++++++++++++++++++++++++++\n");
  
    dungeon::is_tile_walkable(ecs, Position{0,0}); // init static query, only allowed when app is not in progress
    printf("+++++++++++++++++++++++++++++++++++\n");

}


void init_shoot_em_up(flecs::world &ecs, size_t w, size_t h, size_t spawn_cnt)
{
  std::unique_ptr<char[]> tiles(new char[w * h]);
  gen_drunk_dungeon(tiles.get(), w, h);
  init_dungeon(ecs, tiles.get(), w, h);

  register_roguelike_systems(ecs);

  ecs.entity("swordsman_tex")
    .set(Texture2D{LoadTexture("assets/swordsman.png")});
  ecs.entity("minotaur_tex")
    .set(Texture2D{LoadTexture("assets/minotaur.png")});

  //steer::create_seeker(create_monster(ecs, {+400, +400}, WHITE, "minotaur_tex"));
  //steer::create_pursuer(create_monster(ecs, {-400, +400}, RED, "minotaur_tex"));
  //steer::create_evader(create_monster(ecs, {-400, -400}, BLUE, "minotaur_tex"));
  //steer::create_fleer(create_monster(ecs, {+400, -400}, GREEN, "minotaur_tex"));

  Position player_pos = dungeon::find_walkable_tile(ecs);
  create_player(ecs, player_pos, "swordsman_tex");

  std::vector<float> dm;
  dmaps::gen_player_approach_map(ecs, dm);
  int n = std::ranges::max_element(dm, {}, [](float v){ return v == 1e5 ? 0 : v; }) - dm.begin();
  int i = n / w;
  int j = n % w;

  Position exit_pos = {j * dungeon::tile_size, i * dungeon::tile_size};
  ecs.entity("exit")
    .add<DungeonExit>()
    .set(Position{exit_pos});

  std::vector<Position> obj_pos = {player_pos, exit_pos};
  for (int k = 0; k < spawn_cnt; ++k)
  {
    dmaps::gen_multiobject_approach_map(ecs, obj_pos, dm);
    int n = std::ranges::max_element(dm, {}, [](float v){ return v == 1e5 ? 0 : v; }) - dm.begin();
    int i = n / w;
    int j = n % w;
    Position spawn_pos = {j * dungeon::tile_size, i * dungeon::tile_size};
    obj_pos.push_back(spawn_pos);
    ecs.entity()
      .set(MonsterSpawner{0.f, 10.0f})
      .set(Position{spawn_pos});
  }
}


static bool is_reachable(flecs::world &ecs, const Position& a, const Position& b)
{
  auto get_pos = [&](const Position pos) -> IntPos
  {
    Position foot_pos = pos + Position{0.45f * dungeon::tile_size, 0.85f * dungeon::tile_size};
    return {foot_pos.x / dungeon::tile_size, foot_pos.y / dungeon::tile_size};
  };
  auto from = get_pos(a);
  auto to = get_pos(b);

  while (from != to)
  {
    int deltaX = to.x - from.x;
    int deltaY = to.y - from.y;
    if (abs(deltaX) > abs(deltaY))
      from.x += deltaX > 0 ? 1 : -1;
    else
      from.y += deltaY < 0 ? -1 : 1;
    if (!dungeon::is_tile_walkable(ecs, from))
    {
      return false;
    }
  }
  return true;
}


static void process_actions(flecs::world &ecs)
{
  static auto processActions = ecs.query<const Position, const MeleeDamage, const MeleeDist, const Team>();
  static auto checkAttacks = ecs.query<const Position, Hitpoints, const Team>();
  ecs.defer([&]
  {
    /*processActions*/ecs.each([&](flecs::entity entity, const Position &pos, const MeleeDamage &dmg, const MeleeDist& hit_dist, const Team &team)
    {
      /*checkAttacks*/ecs.each([&](flecs::entity enemy, const Position& enemy_pos, Hitpoints &hp, const Team &enemy_team)
      {
        if (team.team != enemy_team.team && dist_sq(pos, enemy_pos) <= sqr(hit_dist.dist) && is_reachable(ecs, pos, enemy_pos))
        {
          //push_to_log(ecs, "damaged entity");
          hp.hitpoints -= dmg.damage * ecs.delta_time();
        }
      });
    });
  });

  static auto deleteAllDead = ecs.query<const Hitpoints>();
  ecs.defer([&]
  {
    /*deleteAllDead*/ecs.each([&](flecs::entity entity, const Hitpoints &hp)
    {
      if (hp.hitpoints <= 0.f)
        entity.destruct();
    });
  });
}


template<typename T>
static void push_info_to_bb(Blackboard &bb, const char *name, const T &val)
{
  size_t idx = bb.regName<T>(name);
  bb.set(idx, val);
}


static void gather_world_info(flecs::world &ecs)
{
  static auto gatherWorldInfo = ecs.query<Blackboard,
                                          const Position, const Hitpoints,
                                          const WorldInfoGatherer,
                                          const Team>();
  static auto alliesQuery = ecs.query<const Position, const Team>();
  /*gatherWorldInfo*/ecs.each([&](Blackboard &bb, const Position &pos, const Hitpoints &hp,
                           WorldInfoGatherer, const Team &team)
  {
    // first gather all needed names (without cache)
    push_info_to_bb(bb, "hp", hp.hitpoints);
    float numAllies = 0; // note float
    float closestEnemyDist = 100.f;
    /*alliesQuery*/ecs.each([&](const Position &apos, const Team &ateam)
    {
      constexpr float limitDist = 4.f * dungeon::tile_size;
      if (team.team == ateam.team && dist_sq(pos, apos) < sqr(limitDist))
        numAllies += 1.f;
      if (team.team != ateam.team)
      {
        const float enemyDist = dist(pos, apos);
        if (enemyDist < closestEnemyDist)
          closestEnemyDist = enemyDist;
      }
    });
    push_info_to_bb(bb, "alliesNum", numAllies);
    push_info_to_bb(bb, "enemyDist", closestEnemyDist);
    auto player = ecs.lookup("player");
    if (player != 0 && player.is_alive())
    {
      push_info_to_bb(bb, "flee_enemy", player);
      push_info_to_bb(bb, "approach_enemy", player);
    }
  });
}


void process_game(flecs::world &ecs)
{
  //static auto stateMachineAct = ecs.query<StateMachine>();
  static auto behTreeUpdate = ecs.query<BehaviourTree, Blackboard>();
  //static auto turnIncrementer = ecs.query<TurnCounter>();
  //if (is_player_acted(ecs))
  {
    //if (upd_player_actions_count(ecs))
    {
      // Plan action for NPCs
      gather_world_info(ecs);
      ecs.defer([&]
      {
        /*stateMachineAct.each([&](flecs::entity e, StateMachine &sm)
        {
          sm.act(0.f, ecs, e);
        });*/
        /*behTreeUpdate*/ecs.each([&](flecs::entity e, BehaviourTree &bt, Blackboard &bb)
        {
          bt.update(ecs, e, bb);
        });
        //process_dmap_followers(ecs);
      });
      //turnIncrementer.each([](TurnCounter &tc) { tc.count++; });
    }
    process_actions(ecs);

    std::vector<float> approachMap;
    dmaps::gen_player_approach_map(ecs, approachMap);
    ecs.entity("approach_map")
      .set(DijkstraMapData{approachMap});

    std::vector<float> fleeMap;
    dmaps::gen_player_flee_map(ecs, fleeMap);
    ecs.entity("flee_map")
      .set(DijkstraMapData{fleeMap});

    /*std::vector<float> hiveMap;
    dmaps::gen_hive_pack_map(ecs, hiveMap);
    ecs.entity("hive_map")
      .set(DijkstraMapData{hiveMap});

    //ecs.entity("flee_map").add<VisualiseMap>();
    ecs.entity("hive_follower_sum")
      .set(DmapWeights{{{"hive_map", {1.f, 1.f}}, {"approach_map", {1.8f, 0.8f}}}})
      .add<VisualiseMap>();*/
  }
}

