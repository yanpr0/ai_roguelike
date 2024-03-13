#pragma once
#include <flecs.h>
#include <vector>

#include "ecsTypes.h"

struct PortalConnection
{
  size_t connIdx;
  float score;
};

struct PathPortal
{
  size_t startX, startY;
  size_t endX, endY;
  std::vector<PortalConnection> conns;
};

struct DungeonPortals
{
  size_t tileSplit;
  std::vector<PathPortal> portals;
  std::vector<std::vector<size_t>> tilePortalsIndices;
};

void prebuild_map(flecs::world &ecs);
std::vector<Position> find_approximated_path(const DungeonPortals &dp, const DungeonData &dd, const Position& pos_from, const Position& pos_to);

