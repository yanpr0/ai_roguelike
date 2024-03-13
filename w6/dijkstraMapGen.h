#pragma once
#include <vector>
#include <flecs.h>

#include "ecsTypes.h"

namespace dmaps
{
  void gen_multiobject_approach_map(flecs::world &ecs, const std::vector<Position>& obj_pos, std::vector<float> &map);
  void gen_player_approach_map(flecs::world &ecs, std::vector<float> &map);
  void gen_player_flee_map(flecs::world &ecs, std::vector<float> &map);
  void gen_hive_pack_map(flecs::world &ecs, std::vector<float> &map);
};

