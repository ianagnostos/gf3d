#include "simple_logger.h"

#include "gf3d_vgraphics.h"
#include "gfc_input.h"

#include "player.h"
#include "gfc_vector.h"
#include "gf3d_model.h"

typedef struct
{
	float cameraFollowDistance;
	float cameraHeight;
	GFC_Vector2D cameraRotation;

	//other player specific data
}PlayerEntityData;

int player_think(Entity* self);
int player_update(Entity* self);
void player_free(Entity* self);

Entity* player_spawn(GFC_Vector3D position)
{
	slog("in player spawn");

	PlayerEntityData* data;
	Entity* self;
	self = entity_new();
	if (!self) return NULL;

	slog("loading model");

	self->model = model_load("models/dino/dino.model");
	self->position = position;
	self->rotation = gfc_vector3d(0, 0, 0);
	self->data = gfc_allocate_array(sizeof(PlayerEntityData), 1);
	self->update = player_update;
	self->think = player_think;
	self->free = player_free;
	data = self->data;


	if (data)
	{
		data->cameraFollowDistance = 50;
		data->cameraHeight = 30;
		data->cameraRotation.x = 0;
		data->cameraRotation.y = GFC_PI/6;
	}

	slog("end of player spawn");
	return self;
}

void player_free(Entity* self)
{
	PlayerEntityData* data;
	if ((!self) || (!self->data)) return;
	data = (PlayerEntityData*)self->data;
	free(data);
	self->data = NULL;

	//clean up borrowed resources
}

int player_update(Entity* self)
{
	PlayerEntityData* data;
	GFC_Matrix4* view;
	GFC_Vector3D move = { 0 };
	GFC_Vector3D offset = { 0 };
	if ((!self) || (!self->data)) return 0;
	data = self->data;

	gfc_vector3d_add(self->position, self->position, self->velocity);
	view = gf3d_vgraphics_get_view_matrix();
	offset = gfc_vector3d(self->position.x, self->position.y + data->cameraFollowDistance, self->position.z + data->cameraHeight);
	gfc_matrix4_view(*view, offset, self->position, gfc_vector3d(0, 0, 1));
	return 0;
}


int player_think(Entity* self)
{
	//GFC_Edge3D edge;
	//GFC_Vector3D floor, offset;
	PlayerEntityData* data;
	GFC_Matrix4 *view;
	GFC_Vector3D move = { 0 };
	GFC_Vector3D offset = { 0 };
	if ((!self) || (!self->data)) return 0;
	data = self->data;
	
	if (gfc_input_command_down("item_up"))
	{
		move.y = -1;
	}
	if (gfc_input_command_down("item_down"))
	{
		move.y = 1;
	}
	if (gfc_input_command_down("item_left"))
	{
		move.x = 1;
	}
	if (gfc_input_command_down("item_right"))
	{
		move.x = -1;
	}
	/*
	offset = gfc_vector3d(0, 0, 100);
	gfc_vector3d_add(edge.a, self->position, offset);
	offset = gfc_vector3d(0, 0, -100);
	gfc_vector3d_add(edge.b, self->position, offset);

	if (level_line_test(level_get_the(), edge, &floor))
	{
		self->position.z = floor.z + 7;
	}
	*/
	self->velocity = gfc_vector3d(0, 0, 0);

	if ((move.x) || (move.y))
	{
		gfc_vector3d_normalize(&move);
		gfc_vector3d_scale(self->velocity, move, 0.5);
	}

	return 0;
}