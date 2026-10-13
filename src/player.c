#include "simple_logger.h"

#include "player.h"
#include "gfc_vector.h"
#include "gf3d_model.h"

int player_think(Entity* self);

Entity* player_spawn(GFC_Vector3D position)
{
	Entity* self;
	self = entity_new();
	if (!self) return NULL;
	self->model = model_load("models/dino/dino.model");
	self->position = position;
	self->think = player_think;
	return self;
}

int player_think(Entity* self)
{
	if (!self) return 0;
	self->position.z = sin(SDL_GetTicks()) * 5;
	return 0;
}