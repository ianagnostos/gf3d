#include "simple_logger.h"

#include "gfc_input.h"

#include "player.h"

#include "fridge.h"

typedef struct
{
	int index;
}FridgeEntityData;


int fridge_update(Entity* self);
void fridge_free(Entity* self);
int fridge_think(Entity* self);

Entity* fridge_spawn(GFC_Vector3D position)
{
	slog("in player spawn");

	FridgeEntityData* data;
	Entity* self;

	self = entity_new();
	if(!self) return NULL;

	self->model = model_load("models/primitives/cube.model");
	self->position = position;
	self->rotation = gfc_vector3d(0, 0, 0);
	self->scale = gfc_vector3d(10, 10, 10);
	self->data = gfc_allocate_array(sizeof(FridgeEntityData), 1);
	self->update = fridge_update;
	self->think = fridge_think;
	self->free = fridge_free;
	self->bounds = gfc_box(-5, -5, -5, 10, 10, 10);
	self->solid = 1;
	data = self->data;

	self->color = GFC_COLOR_BROWN; 

	if (data)
	{
		
	}
	return self;
}

void fridge_free(Entity* self)
{
	FridgeEntityData* data;
	if ((!self) || (!self->data)) return;
	data = (FridgeEntityData*)self->data;
	free(data);
	self->data = NULL;

	//clean up other borrowed resources
}

int fridge_update(Entity* self)
{
	FridgeEntityData* data;
	if ((!self) || (!self->data)) return 0;
	data = self->data;

	return 0;
}

int fridge_think(Entity* self) 
{
	if (entity_collision_other(self, player_get(1)) && (gfc_input_command_pressed("item_use")))
	{
		slog("player 1 used the fridge");
	}

	if (entity_collision_other(self, player_get(2)) && (gfc_input_command_pressed("p2_use")))
	{
		slog("player 2 used the fridge");
	}
}

