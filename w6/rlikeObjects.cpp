#include "rlikeObjects.h"
#include "ecsTypes.h"
#include "dungeonUtils.h"

flecs::entity create_monster(flecs::world &ecs, Position pos, Color col, const char *texture_src)
{
  flecs::entity textureSrc = ecs.entity(texture_src);
  return ecs.entity()
    .set(Position{pos.x, pos.y})
    .set(Velocity{0.f, 0.f})
    .set(MoveSpeed{0.9f * dungeon::tile_size})
    .set(Hitpoints{100.f})
    .set(Action{EA_NOP})
    .set(Color{col})
    .add<TextureSource>(textureSrc)
    .set(Team{1})
    .set(NumActions{1, 0})
    .set(MeleeDamage{10.f})
    .set(MeleeDist{2 * dungeon::tile_size});
}

void create_player(flecs::world &ecs, Position pos, const char *texture_src)
{
  flecs::entity textureSrc = ecs.entity(texture_src);
  ecs.entity("player")
    .set(Position{pos.x, pos.y})
    .set(Velocity{0.f, 0.f})
    .set(MoveSpeed{dungeon::tile_size})
    .set(Hitpoints{100.f})
    .set(Action{EA_NOP})
    .add<IsPlayer>()
    .set(Team{0})
    .set(PlayerInput{})
    .set(NumActions{2, 0})
    .set(Color{255, 255, 255, 255})
    .add<TextureSource>(textureSrc)
    .set(MeleeDamage{15.f})
    .set(MeleeDist{3 * dungeon::tile_size});

}

