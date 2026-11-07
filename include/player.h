#ifndef __PLAYER_H_
#define __PLAYER_H_

#include "gf3d_entity.h"

Entity* player_spawn(GFC_Vector3D position, int index);

Entity* player_get(int index);

int player_update(Entity* self);

void player_free(Entity* self);

int player_think(Entity* self);

Entity* player_get_holding(Entity* player);

void    player_set_holding(Entity* player, Entity* item);

#endif 
