#ifndef __PLAYER_H_
#define __PLAYER_H_

#include "gf3d_entity.h"

Entity* player_spawn(GFC_Vector3D position);

int player_update(Entity* self);

void player_free(Entity* self);
int player_think(Entity* self);

#endif 
