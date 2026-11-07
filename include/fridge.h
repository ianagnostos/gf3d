#ifndef __FRIDGE_H_
#define __FRIDGE_H_

#include "gf3d_entity.h"

Entity* fridge_spawn(GFC_Vector3D position);

int fridge_update(Entity* self);

void fridge_free(Entity* self);

int fridge_think(Entity* self);

#endif