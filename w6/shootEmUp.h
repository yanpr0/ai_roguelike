#pragma once
#include <flecs.h>

//void init_dungeon(flecs::world &ecs, char *tiles, size_t w, size_t h);
void init_shoot_em_up(flecs::world &ecs, size_t w, size_t h, size_t spawn_cnt);
void process_game(flecs::world &ecs);

