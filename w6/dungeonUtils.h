#pragma once
#include "ecsTypes.h"
#include <flecs.h>

namespace dungeon
{
  constexpr char wall = '#';
  constexpr char floor = ' ';

  constexpr float tile_size = 64.f;

  Position find_walkable_tile(flecs::world &ecs);
  bool is_tile_walkable(flecs::world &ecs, Position pos);
  bool is_tile_walkable(flecs::world &ecs, IntPos pos);
};
