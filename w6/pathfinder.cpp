#include "pathfinder.h"
#include "dungeonUtils.h"
#include "math.h"
#include <algorithm>

float heuristic(IVec2 lhs, IVec2 rhs)
{
  return sqrtf(sqr(float(lhs.x - rhs.x)) + sqr(float(lhs.y - rhs.y)));
};

template<typename T>
static size_t coord_to_idx(T x, T y, size_t w)
{
  return size_t(y) * w + size_t(x);
}

static std::vector<IVec2> reconstruct_path(std::vector<IVec2> prev, IVec2 to, size_t width)
{
  IVec2 curPos = to;
  std::vector<IVec2> res = {curPos};
  while (prev[coord_to_idx(curPos.x, curPos.y, width)] != IVec2{-1, -1})
  {
    curPos = prev[coord_to_idx(curPos.x, curPos.y, width)];
    res.insert(res.begin(), curPos);
  }
  return res;
}

static std::vector<IVec2> find_path_a_star(const DungeonData &dd, IVec2 from, IVec2 to,
                                           IVec2 lim_min, IVec2 lim_max)
{
  if (from.x < 0 || from.y < 0 || from.x >= int(dd.width) || from.y >= int(dd.height))
    return std::vector<IVec2>();
  size_t inpSize = dd.width * dd.height;

  std::vector<float> g(inpSize, std::numeric_limits<float>::max());
  std::vector<float> f(inpSize, std::numeric_limits<float>::max());
  std::vector<IVec2> prev(inpSize, {-1,-1});

  auto getG = [&](IVec2 p) -> float { return g[coord_to_idx(p.x, p.y, dd.width)]; };
  auto getF = [&](IVec2 p) -> float { return f[coord_to_idx(p.x, p.y, dd.width)]; };

  g[coord_to_idx(from.x, from.y, dd.width)] = 0;
  f[coord_to_idx(from.x, from.y, dd.width)] = heuristic(from, to);

  std::vector<IVec2> openList = {from};
  std::vector<IVec2> closedList;

  while (!openList.empty())
  {
    size_t bestIdx = 0;
    float bestScore = getF(openList[0]);
    for (size_t i = 1; i < openList.size(); ++i)
    {
      float score = getF(openList[i]);
      if (score < bestScore)
      {
        bestIdx = i;
        bestScore = score;
      }
    }
    if (openList[bestIdx] == to)
      return reconstruct_path(prev, to, dd.width);
    IVec2 curPos = openList[bestIdx];
    openList.erase(openList.begin() + bestIdx);
    if (std::find(closedList.begin(), closedList.end(), curPos) != closedList.end())
      continue;
    size_t idx = coord_to_idx(curPos.x, curPos.y, dd.width);
    closedList.emplace_back(curPos);
    auto checkNeighbour = [&](IVec2 p)
    {
      // out of bounds
      if (p.x < lim_min.x || p.y < lim_min.y || p.x >= lim_max.x || p.y >= lim_max.y)
        return;
      size_t idx = coord_to_idx(p.x, p.y, dd.width);
      // not empty
      if (dd.tiles[idx] == dungeon::wall)
        return;
      float edgeWeight = 1.f;
      float gScore = getG(curPos) + 1.f * edgeWeight; // we're exactly 1 unit away
      if (gScore < getG(p))
      {
        prev[idx] = curPos;
        g[idx] = gScore;
        f[idx] = gScore + heuristic(p, to);
      }
      bool found = std::find(openList.begin(), openList.end(), p) != openList.end();
      if (!found)
        openList.emplace_back(p);
    };
    checkNeighbour({curPos.x + 1, curPos.y + 0});
    checkNeighbour({curPos.x - 1, curPos.y + 0});
    checkNeighbour({curPos.x + 0, curPos.y + 1});
    checkNeighbour({curPos.x + 0, curPos.y - 1});
  }
  // empty path
  return std::vector<IVec2>();
}


void prebuild_map(flecs::world &ecs)
{
  auto mapQuery = ecs.query<const DungeonData>();

  constexpr size_t splitTiles = 10;
  ecs.defer([&]()
  {
    mapQuery.each([&](flecs::entity e, const DungeonData &dd)
    {
      // go through each super tile
      const size_t width = dd.width / splitTiles;
      const size_t height = dd.height / splitTiles;

      auto check_border = [&](size_t xx, size_t yy,
                              size_t dir_x, size_t dir_y,
                              int offs_x, int offs_y,
                              std::vector<PathPortal> &portals)
      {
        int spanFrom = -1;
        int spanTo = -1;
        for (size_t i = 0; i < splitTiles; ++i)
        {
          size_t x = xx * splitTiles + i * dir_x;
          size_t y = yy * splitTiles + i * dir_y;
          size_t nx = x + offs_x;
          size_t ny = y + offs_y;
          if (dd.tiles[y * dd.width + x] != dungeon::wall &&
              dd.tiles[ny * dd.width + nx] != dungeon::wall)
          {
            if (spanFrom < 0)
              spanFrom = i;
            spanTo = i;
          }
          else if (spanFrom >= 0)
          {
            // write span
            portals.push_back({xx * splitTiles + spanFrom * dir_x + offs_x,
                               yy * splitTiles + spanFrom * dir_y + offs_y,
                               xx * splitTiles + spanTo * dir_x,
                               yy * splitTiles + spanTo * dir_y});
            spanFrom = -1;
          }
        }
        if (spanFrom >= 0)
        {
          portals.push_back({xx * splitTiles + spanFrom * dir_x + offs_x,
                             yy * splitTiles + spanFrom * dir_y + offs_y,
                             xx * splitTiles + spanTo * dir_x,
                             yy * splitTiles + spanTo * dir_y});
        }
      };

      std::vector<PathPortal> portals;
      std::vector<std::vector<size_t>> tilePortalsIndices;

      auto push_portals = [&](size_t x, size_t y,
                              int offs_x, int offs_y,
                              const std::vector<PathPortal> &new_portals)
      {
        for (const PathPortal &portal : new_portals)
        {
          size_t idx = portals.size();
          portals.push_back(portal);
          tilePortalsIndices[y * width + x].push_back(idx);
          tilePortalsIndices[(y + offs_y) * width + x + offs_x].push_back(idx);
        }
      };
      for (size_t y = 0; y < height; ++y)
        for (size_t x = 0; x < width; ++x)
        {
          tilePortalsIndices.push_back(std::vector<size_t>{});
          // check top
          if (y > 0)
          {
            std::vector<PathPortal> topPortals;
            check_border(x, y, 1, 0, 0, -1, topPortals);
            push_portals(x, y, 0, -1, topPortals);
          }
          // left
          if (x > 0)
          {
            std::vector<PathPortal> leftPortals;
            check_border(x, y, 0, 1, -1, 0, leftPortals);
            push_portals(x, y, -1, 0, leftPortals);
          }
        }
      for (size_t tidx = 0; tidx < tilePortalsIndices.size(); ++tidx)
      {
        const std::vector<size_t> &indices = tilePortalsIndices[tidx];
        size_t x = tidx % width;
        size_t y = tidx / width;
        IVec2 limMin{int((x + 0) * splitTiles), int((y + 0) * splitTiles)};
        IVec2 limMax{int((x + 1) * splitTiles), int((y + 1) * splitTiles)};
        for (size_t i = 0; i < indices.size(); ++i)
        {
          PathPortal &firstPortal = portals[indices[i]];
          for (size_t j = i + 1; j < indices.size(); ++j)
          {
            PathPortal &secondPortal = portals[indices[j]];
            // check path from i to j
            // check each position (to find closest dist) (could be made more optimal)
            bool noPath = false;
            size_t minDist = 0xffffffff;
            for (size_t fromY = std::max(firstPortal.startY, size_t(limMin.y));
                        fromY <= std::min(firstPortal.endY, size_t(limMax.y - 1)) && !noPath; ++fromY)
            {
              for (size_t fromX = std::max(firstPortal.startX, size_t(limMin.x));
                          fromX <= std::min(firstPortal.endX, size_t(limMax.x - 1)) && !noPath; ++fromX)
              {
                for (size_t toY = std::max(secondPortal.startY, size_t(limMin.y));
                            toY <= std::min(secondPortal.endY, size_t(limMax.y - 1)) && !noPath; ++toY)
                {
                  for (size_t toX = std::max(secondPortal.startX, size_t(limMin.x));
                              toX <= std::min(secondPortal.endX, size_t(limMax.x - 1)) && !noPath; ++toX)
                  {
                    IVec2 from{int(fromX), int(fromY)};
                    IVec2 to{int(toX), int(toY)};
                    std::vector<IVec2> path = find_path_a_star(dd, from, to, limMin, limMax);
                    if (path.empty() && from != to)
                    {
                      noPath = true; // if we found that there's no path at all - we can break out
                      break;
                    }
                    minDist = std::min(minDist, path.size());
                  }
                }
              }
            }
            // write pathable data and length
            if (noPath)
              continue;
            firstPortal.conns.push_back({indices[j], float(minDist)});
            secondPortal.conns.push_back({indices[i], float(minDist)});
          }
        }
      }
      e.set(DungeonPortals{splitTiles, portals, tilePortalsIndices});
    });
  });
}


static std::vector<int> reconstruct_path(const std::vector<int>& prev, int to)
{
  int curPos = to;
  std::vector<int> res = {curPos};
  while (prev[curPos] != -1)
  {
    curPos = prev[curPos];
    res.push_back(curPos);
  }
  std::reverse(res.begin(), res.end());
  return res;
}


static std::pair<std::vector<int>, float> find_path_portal_a_star(const DungeonPortals& dp, int from, int to)
{
  std::vector<float> g(dp.portals.size(), std::numeric_limits<float>::max());
  std::vector<float> f(dp.portals.size(), std::numeric_limits<float>::max());
  std::vector<int> prev(dp.portals.size(), -1);

  auto getG = [&](int i) -> float { return g[i]; };
  auto getF = [&](int i) -> float { return f[i]; };
  auto center = [&](const PathPortal& p) { return IVec2{(p.startX + p.endX) / 2.0, (p.startY + p.endY) / 2.0}; };
  auto getH = [&](int i, int j) { return heuristic(center(dp.portals[i]), center(dp.portals[j])); }; // maybe WA*-like

  g[from] = 0;
  f[from] = getH(from, to);

  std::vector<int> openList = {from};
  std::vector<int> closedList;

  while (!openList.empty())
  {
    size_t bestIdx = 0;
    float bestScore = getF(openList[0]);
    for (size_t i = 1; i < openList.size(); ++i)
    {
      float score = getF(openList[i]);
      if (score < bestScore)
      {
        bestIdx = i;
        bestScore = score;
      }
    }
    if (openList[bestIdx] == to)
      return {reconstruct_path(prev, to), bestScore};
    int curPos = openList[bestIdx];
    openList.erase(openList.begin() + bestIdx);
    if (std::find(closedList.begin(), closedList.end(), curPos) != closedList.end())
      continue;
    closedList.emplace_back(curPos);

    for (auto [next, score] : dp.portals[curPos].conns)
    {
      size_t idx = next;
      float gScore = getG(curPos) + score;
      if (gScore < getG(next))
      {
        prev[idx] = curPos;
        g[idx] = gScore;
        f[idx] = gScore + getH(next, to);
      }
      bool found = std::find(openList.begin(), openList.end(), next) != openList.end();
      if (!found)
        openList.emplace_back(next);
    }
  }
  // empty path
  return {std::vector<int>(), std::numeric_limits<float>::max()};
}


// in one supertile
static std::vector<IVec2> find_path_tile_to_tile(const DungeonPortals &dp, const DungeonData &dd, const IVec2& tile_from, const IVec2& tile_to)
{
  auto splitTiles = dp.tileSplit;
  int from = (dd.width / dp.tileSplit) * (tile_from.y / dp.tileSplit) + (tile_from.x / dp.tileSplit);
  int width = dd.width / splitTiles;

  size_t x = from % width;
  size_t y = from / width;
  IVec2 limMin{int((x + 0) * splitTiles), int((y + 0) * splitTiles)};
  IVec2 limMax{int((x + 1) * splitTiles), int((y + 1) * splitTiles)};
 
  std::vector<IVec2> path = find_path_a_star(dd, tile_from, tile_to, limMin, limMax);
  return path;
}


// in one supertile
static std::vector<IVec2> find_path_tile_to_portal(const DungeonPortals &dp, const DungeonData &dd, const IVec2& tile_from, int to)
{
  auto splitTiles = dp.tileSplit;
  int from = (dd.width / dp.tileSplit) * (tile_from.y / dp.tileSplit) + (tile_from.x / dp.tileSplit);
  int width = dd.width / splitTiles;

  size_t x = from % width;
  size_t y = from / width;
  IVec2 limMin{int((x + 0) * splitTiles), int((y + 0) * splitTiles)};
  IVec2 limMax{int((x + 1) * splitTiles), int((y + 1) * splitTiles)};

  const PathPortal& portal = dp.portals[to];

  bool noPath = false;
  size_t minDist = 0xffffffff;
  std::vector<IVec2> minPath;
 
  for (size_t toY = std::max(portal.startY, size_t(limMin.y));
      toY <= std::min(portal.endY, size_t(limMax.y - 1)) && !noPath; ++toY)
  {
    for (size_t toX = std::max(portal.startX, size_t(limMin.x));
        toX <= std::min(portal.endX, size_t(limMax.x - 1)) && !noPath; ++toX)
    {
      IVec2 to{int(toX), int(toY)};
      std::vector<IVec2> path = find_path_a_star(dd, tile_from, to, limMin, limMax);
      if (path.empty() && tile_from != to)
      {
        noPath = true; // if we found that there's no path at all - we can break out
        break;
      }
      if (path.size() < minDist)
      {
        minDist = path.size();
        minPath = path;
      }
    }
  }
  return minPath;
}


std::vector<Position> find_approximated_path(const DungeonPortals &dp, const DungeonData &dd, const Position& pos_from, const Position& pos_to)
{
  IVec2 tile_from = {pos_from.x / dungeon::tile_size, pos_from.y / dungeon::tile_size};
  IVec2 tile_to = {pos_to.x / dungeon::tile_size, pos_to.y / dungeon::tile_size};
  int from = (dd.width / dp.tileSplit) * (tile_from.y / dp.tileSplit) + (tile_from.x / dp.tileSplit);
  int to = (dd.width / dp.tileSplit) * (tile_to.y / dp.tileSplit) + (tile_to.x / dp.tileSplit);

  auto tiles_to_pos = [&](const std::vector<IVec2>& a)
  {
    std::vector<Position> res;
    for (auto [x, y] : a)
    {
      res.emplace_back(x * dungeon::tile_size, y * dungeon::tile_size);
    }
    return res;
  };

  auto portals_to_pos = [&](const std::vector<int>& a)
  {
    std::vector<Position> res;
    for (auto& i : a)
    {
      const auto& p = dp.portals[i];
      res.emplace_back((p.startX + p.endX + 1) / 2.0 * dungeon::tile_size, (p.startY + p.endY + 1) / 2.0 * dungeon::tile_size);
    }
    return res;
  };

  float min_len = std::numeric_limits<float>::max();
  std::vector<Position> min_path;

  if (to == from)
  {
    min_path = tiles_to_pos(find_path_tile_to_tile(dp, dd, tile_from, tile_to));
    if (!min_path.empty())
    {
      min_len = min_path.size();
    }
  }

  for (int i : dp.tilePortalsIndices[from])
  {
    for (int j : dp.tilePortalsIndices[to])
    {
      auto start = find_path_tile_to_portal(dp, dd, tile_from, i);
      auto [mid, mid_len] = find_path_portal_a_star(dp, i, j);
      auto end = find_path_tile_to_portal(dp, dd, tile_to, j);

      if (start.empty() || end.empty())
      {
        continue;
      }

      float len = start.size() + mid_len + end.size();
      if (len < min_len)
      {
        min_len = len;
        auto path = tiles_to_pos(start);
        auto mid_path = portals_to_pos(mid);
        auto end_path = tiles_to_pos(end);
        path.insert(path.end(), mid_path.begin(), mid_path.end());
        std::reverse(end_path.begin(), end_path.end());
        path.insert(path.end(), end_path.begin(), end_path.end());
        min_path = path;
      }
    }
  }
  return min_path;
}

