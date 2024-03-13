#include "steering.h"
#include "ecsTypes.h"
#include "dungeonUtils.h"

struct SteerAccel { float accel = 1.f; };

static flecs::entity create_separation(flecs::entity e)
{
  return e.add<Separation>();
}

static flecs::entity create_alignment(flecs::entity e)
{
  return e.add<Alignment>();
}

static flecs::entity create_cohesion(flecs::entity e)
{
  return e.add<Cohesion>();
}

flecs::entity steer::create_steerer(flecs::entity e)
{
  return //create_cohesion(
      create_alignment(
        create_separation(e.set(SteerDir{0.f, 0.f}).set(SteerAccel{1.f}))
        //)
      );
}

flecs::entity steer::create_seeker(flecs::entity e)
{
  return create_steerer(e).add<Seeker>();
}

flecs::entity steer::create_pursuer(flecs::entity e)
{
  return create_steerer(e).add<Pursuer>();
}

flecs::entity steer::create_evader(flecs::entity e)
{
  return create_steerer(e).add<Evader>();
}

flecs::entity steer::create_fleer(flecs::entity e)
{
  return create_steerer(e).add<Fleer>();
}

typedef flecs::entity (*create_foo)(flecs::entity);

flecs::entity steer::create_steer_beh(flecs::entity e, Type type)
{
  create_foo steerFoo[Type::Num] =
  {
    create_seeker,
    create_pursuer,
    create_evader,
    create_fleer
  };
  return steerFoo[type](e);
}


void steer::register_systems(flecs::world &ecs)
{
  static auto playerPosQuery = ecs.query<const Position, const Velocity, const IsPlayer>();

  ecs.system<Velocity, const MoveSpeed, const SteerDir, const SteerAccel, const Position>()
    .each([&](Velocity &vel, const MoveSpeed &ms, const SteerDir &sd, const SteerAccel &sa, const Position& pos)
    {
      vel = Velocity{truncate(vel + truncate(sd, ms.speed) * ecs.delta_time() * sa.accel, ms.speed)};
      float dt = ecs.delta_time();
      Position up_left_hit = pos + Position{0.15f * dungeon::tile_size, 0.75f * dungeon::tile_size};
      Position up_right_hit = pos + Position{0.75f * dungeon::tile_size, 0.75f * dungeon::tile_size};
      Position down_left_hit = pos + Position{0.15f * dungeon::tile_size, 0.95f * dungeon::tile_size};
      Position down_right_hit = pos + Position{0.75f * dungeon::tile_size, 0.95f * dungeon::tile_size};
      float vx = vel.x;
      Position delta = Position{vx, 0.0f} * dt;
      if (vx > 0.f)
      {
        if (!dungeon::is_tile_walkable(ecs, up_right_hit + delta) || !dungeon::is_tile_walkable(ecs, down_right_hit + delta))
        {
          vx = -1.f;
        }
      }
      else if (vx < 0.f)
      {
        if (!dungeon::is_tile_walkable(ecs, up_left_hit + delta) || !dungeon::is_tile_walkable(ecs, down_left_hit + delta))
        {
          vx = 1.f;
        }
      }
      float vy = vel.y;
      delta = Position{0.0f, vy} * dt;
      if (vy > 0.f)
      {
        if (!dungeon::is_tile_walkable(ecs, down_left_hit + delta) || !dungeon::is_tile_walkable(ecs, down_right_hit + delta))
        {
          vy = -1.f;
        }
      }
      else
      {
        if (!dungeon::is_tile_walkable(ecs, up_left_hit + delta) || !dungeon::is_tile_walkable(ecs, up_right_hit + delta))
        {
          vy = 1.f;
        }
      }
      vel = {vx, vy};
    });

  // reset steer dir
  ecs.system<SteerDir>().each([&](SteerDir &sd) { sd = {0.f, 0.f}; });

  static auto dungeonDataQuery = ecs.query<const DungeonData>();
  // seeker
  ecs.system<SteerDir, const MoveSpeed, const Velocity, const Position, const Seeker>()
    .each([&](SteerDir &sd, const MoveSpeed &ms, const Velocity &vel,
              const Position &pos, const Seeker &)
    {
      /*
      playerPosQuery.each([&](const Position &pp, const Velocity &, const IsPlayer &)
      {
        sd += SteerDir{normalize(pp - pos) * ms.speed - vel};
      });
      */
      
      auto get_dmap_at = [&](const DijkstraMapData &dmap, const DungeonData &dd, size_t x, size_t y)
      {
        return dmap.map[y * dd.width + x];
      };
      auto get_vec = [&](Actions a)
      {
        switch (a)
        {
          case EA_NOP: return Position{0,0};
          case EA_MOVE_LEFT: return Position{-1,0};
          case EA_MOVE_RIGHT: return Position{1,0};
          case EA_MOVE_UP: return Position{0,-1};
          case EA_MOVE_DOWN: return Position{0,1};
        }
      };
      auto get_pos = [&](const Position pos) -> std::pair<int, int>
      {
        Position foot_pos = pos + Position{0.45f * dungeon::tile_size, 0.85f * dungeon::tile_size};
        return {foot_pos.x / dungeon::tile_size, foot_pos.y / dungeon::tile_size};
      };
      /*dungeonDataQuery*/ecs.each([&](const DungeonData &dd)
      {
        float moveWeights[EA_MOVE_END];
        for (size_t i = 0; i < EA_MOVE_END; ++i)
          moveWeights[i] = 0.f;
        ecs.entity("approach_map").get([&](const DijkstraMapData &dmap)
        {
          auto [x, y] = get_pos(pos);
          moveWeights[EA_NOP]         += get_dmap_at(dmap, dd, x+0, y+0);
          moveWeights[EA_MOVE_LEFT]   += get_dmap_at(dmap, dd, x-1, y+0);
          moveWeights[EA_MOVE_RIGHT]  += get_dmap_at(dmap, dd, x+1, y+0);
          moveWeights[EA_MOVE_UP]     += get_dmap_at(dmap, dd, x+0, y-1);
          moveWeights[EA_MOVE_DOWN]   += get_dmap_at(dmap, dd, x+0, y+1);
        });
        float minWt = moveWeights[EA_NOP];
        SteerDir dsd;
        for (size_t i = 0; i < EA_MOVE_END; ++i)
          if (moveWeights[i] < minWt)
          {
            minWt = moveWeights[i];
            dsd = SteerDir{get_vec(Actions(i)) * ms.speed - vel};
          }
        sd = {dsd.x * 1.1, dsd.y * 1.1};
      });
    });

  // fleer
  ecs.system<SteerDir, const MoveSpeed, const Velocity, const Position, const Fleer>()
    .each([&](SteerDir &sd, const MoveSpeed &ms, const Velocity &vel, const Position &pos, const Fleer &)
    {
      /*
      playerPosQuery.each([&](const Position &pp, const Velocity &, const IsPlayer &)
      {
        sd += SteerDir{normalize(pos - pp) * ms.speed - vel};
      });
      */
      
      auto get_dmap_at = [&](const DijkstraMapData &dmap, const DungeonData &dd, size_t x, size_t y)
      {
        return dmap.map[y * dd.width + x];
      };
      auto get_vec = [&](Actions a)
      {
        switch (a)
        {
          case EA_NOP: return Position{0,0};
          case EA_MOVE_LEFT: return Position{-1,0};
          case EA_MOVE_RIGHT: return Position{1,0};
          case EA_MOVE_UP: return Position{0,-1};
          case EA_MOVE_DOWN: return Position{0,1};
        }
      };
      auto get_pos = [&](const Position pos) -> std::pair<int, int>
      {
        Position foot_pos = pos + Position{0.45f * dungeon::tile_size, 0.85f * dungeon::tile_size};
        return {foot_pos.x / dungeon::tile_size, foot_pos.y / dungeon::tile_size};
      };
      /*dungeonDataQuery*/ecs.each([&](const DungeonData &dd)
      {
        float moveWeights[EA_MOVE_END];
        for (size_t i = 0; i < EA_MOVE_END; ++i)
          moveWeights[i] = 0.f;
        ecs.entity("flee_map").get([&](const DijkstraMapData &dmap)
        {
          auto [x, y] = get_pos(pos);
          moveWeights[EA_NOP]         += get_dmap_at(dmap, dd, x+0, y+0);
          moveWeights[EA_MOVE_LEFT]   += get_dmap_at(dmap, dd, x-1, y+0);
          moveWeights[EA_MOVE_RIGHT]  += get_dmap_at(dmap, dd, x+1, y+0);
          moveWeights[EA_MOVE_UP]     += get_dmap_at(dmap, dd, x+0, y-1);
          moveWeights[EA_MOVE_DOWN]   += get_dmap_at(dmap, dd, x+0, y+1);
        });
        float minWt = moveWeights[EA_NOP];
        SteerDir dsd;
        for (size_t i = 0; i < EA_MOVE_END; ++i)
          if (moveWeights[i] < minWt)
          {
            minWt = moveWeights[i];
            dsd = SteerDir{get_vec(Actions(i)) * ms.speed - vel};
          }
        sd = {dsd.x * 1.1, dsd.y * 1.1};
      });
    });

  // pursuer
  ecs.system<SteerDir, const MoveSpeed, const Velocity, const Position, const Pursuer>()
    .each([&](SteerDir &sd, const MoveSpeed &ms, const Velocity &vel, const Position &p, const Pursuer &)
    {
      /*playerPosQuery*/ecs.each([&](const Position &pp, const Velocity &pvel, const IsPlayer &)
      {
        constexpr float predictTime = 4.f;
        const Position targetPos = pp + pvel * predictTime;
        sd += SteerDir{normalize(targetPos - p) * ms.speed - vel};
      });
    });

  // evader
  ecs.system<SteerDir, const MoveSpeed, const Velocity, const Position, const Evader>()
    .each([&](SteerDir &sd, const MoveSpeed &ms, const Velocity &vel, const Position &p, const Evader &)
    {
      /*playerPosQuery*/ecs.each([&](const Position &pp, const Velocity &pvel,
                              const IsPlayer &)
      {
        constexpr float maxPredictTime = 4.f;
        const Position dpos = p - pp;
        const float dist = length(dpos);
        const Position dvel = vel - pvel;
        const float dotProduct = (dvel.x * dpos.x + dvel.y * dpos.y) * safeinv(dist);
        const float interceptTime = dotProduct * safeinv(length(dvel));
        const float predictTime = std::max(std::min(maxPredictTime, interceptTime * 0.9f), 1.f);

        const Position targetPos = pp + pvel * predictTime;
        sd += SteerDir{normalize(p - targetPos) * ms.speed - vel};
      });
    });

  static auto otherPosQuery = ecs.query<const Position>();

  // separation is expensive!!!
  ecs.system<SteerDir, const Velocity, const MoveSpeed, const Position, const Separation>()
    .each([&](flecs::entity ent, SteerDir &sd, const Velocity &vel, const MoveSpeed &ms,
              const Position &p, const Separation &)
    {
      /*otherPosQuery*/ecs.each([&](flecs::entity oe, const Position &op)
      {
        if (oe == ent)
          return;
        constexpr float thresDist = 70.f;
        constexpr float thresDistSq = thresDist * thresDist;
        const float distSq = length_sq(op - p);
        if (distSq > thresDistSq)
          return;
        sd += SteerDir{(p - op) * safeinv(distSq) * ms.speed * thresDist - vel};
      });
    });

  static auto otherVelQuery = ecs.query<const Position, const Velocity>();
  ecs.system<SteerDir, const Velocity, const MoveSpeed, const Position, const Alignment>()
    .each([&](flecs::entity ent, SteerDir &sd, const Velocity &vel, const MoveSpeed &ms,
              const Position &p, const Alignment &)
    {
      /*otherVelQuery*/ecs.each([&](flecs::entity oe, const Position &op, const Velocity &ovel)
      {
        if (oe == ent)
          return;
        constexpr float thresDist = 100.f;
        constexpr float thresDistSq = thresDist * thresDist;
        const float distSq = length_sq(op - p);
        if (distSq > thresDistSq)
          return;
        sd += SteerDir{ovel * 0.8f};
      });
    });

  ecs.system<SteerDir, const Velocity, const MoveSpeed, const Position, const Cohesion>()
    .each([&](flecs::entity ent, SteerDir &sd, const Velocity &vel, const MoveSpeed &ms,
              const Position &p, const Cohesion &)
    {
      Position avgPos{0.f, 0.f};
      size_t count = 0;
      /*otherPosQuery*/ecs.each([&](flecs::entity oe, const Position &op)
      {
        if (oe == ent)
          return;
        constexpr float thresDist = 500.f;
        constexpr float thresDistSq = thresDist * thresDist;
        const float distSq = length_sq(op - p);
        if (distSq > thresDistSq)
          return;
        count++;
        avgPos += op;
      });
      constexpr float avgPosMult = 100.f;
      sd += SteerDir{normalize(avgPos * safeinv(float(count)) - p) * avgPosMult - vel};
    });

}

