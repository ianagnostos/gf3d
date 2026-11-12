#include "simple_logger.h"
#include "gf3d_vgraphics.h"
#include "gfc_input.h"
#include "gfc_vector.h"

#include "player.h"
#include "gf3d_model.h"
#include "level.h"
#include "station.h"
#include "draw_3d.h"

#define CAMERA_FOLLOW_DISTANCE 90
#define CAMERA_HEIGHT 150

#define ITEM_HELD_HEIGHT 9.0f
#define ITEM_HELD_FORWARD 0.0f

extern float dt;

static Entity* players[2] = { 0 };

static const char* pickupCmd[2] = { "item_pickup", "p2_pickup" };
static const char* useCmd[2] = { "item_use",    "p2_use" };

typedef struct
{
	int index;
	float speed;
	float facingX, facingY;
	Entity* holding;
	StationData* target;
	StationData* working_at;
}PlayerEntityData;

int player_think(Entity* self);
int player_update(Entity* self);
void player_free(Entity* self);

Entity* player_get(int index)
{
	return players[index - 1];
}

Entity* player_spawn(GFC_Vector3D position, int index)
{
	//slog("in player spawn");

	PlayerEntityData* data;
	Entity* self;

	if (players[index - 1]) return players[index - 1];

	self = entity_new();
	if (!self) return NULL;

	
	//slog("loading model");

	self->model = model_load("models/primitives/cube.model");
	self->position = position;
	self->rotation = gfc_vector3d(0, 0, 0);
	self->scale = gfc_vector3d(8, 8, 8);
	self->data = gfc_allocate_array(sizeof(PlayerEntityData), 1);
	self->update = player_update;
	self->think = player_think;
	self->free = player_free;
	self->bounds = gfc_box(-4, -4, -4, 8, 8, 8);

	self->solid = 1;
	data = self->data;

	if (index == 1) self->color = GFC_COLOR_GREY;
	else            self->color = GFC_COLOR_GREY;


	if (data)
	{
		data->speed = 1;
		data->index = index;
		data->facingY = 1;
		data->facingY = 0;
	}

	//slog("end of player spawn");
	players[index-1] = self;
	return self;
}

void player_free(Entity* self)
{
	PlayerEntityData* data;
	if ((!self) || (!self->data)) return;
	data = (PlayerEntityData*)self->data;
	players[data->index - 1] = NULL;
	free(data);
	self->data = NULL;
	//clean up borrowed resourcess
}

int player_update(Entity* self)
{
	GFC_Matrix4* view;
	GFC_Vector3D move = { 0 };
	GFC_Vector3D offset = { 0 };

	PlayerEntityData* data;
	if ((!self) || (!self->data)) return;
	data = (PlayerEntityData*)self->data;

	if (data->index == 1)
	{
		view = gf3d_vgraphics_get_view_matrix();
		offset = gfc_vector3d(0, CAMERA_FOLLOW_DISTANCE, CAMERA_HEIGHT);
		gfc_matrix4_view(*view, offset, gfc_vector3d(0, 0, 0), gfc_vector3d(0, 0, 1));
	}
	return 0;
}


int player_think(Entity* self)
{
	GFC_Vector3D floor;
	PlayerEntityData* data;
	GFC_Vector3D move = { 0 };

	if ((!self) || (!self->data)) return 0;
	data = self->data;

	if (data->index == 1)
	{
		if (gfc_input_command_down("item_up")) move.y = -1;
		if (gfc_input_command_down("item_down")) move.y = 1;
		if (gfc_input_command_down("item_left")) move.x = 1;
		if (gfc_input_command_down("item_right")) move.x = -1;
	}

	if (data->index == 2)
	{
		if (gfc_input_command_down("p2_up"))move.y = -1;
		if (gfc_input_command_down("p2_down"))move.y = 1;
		if (gfc_input_command_down("p2_left")) move.x = 1;
		if (gfc_input_command_down("p2_right")) move.x = -1;
	}
	
	self->velocity = gfc_vector3d(0, 0, 0);

	if ((move.x) || (move.y))
	{
		gfc_vector3d_normalize(&move);
		data->facingX = move.x;
		data->facingY = move.y;
		self->rotation.z = atan2f(move.y, move.x) + GFC_PI * 0.5f;
		gfc_vector3d_scale(self->velocity, move, data->speed);
	}

	
	data->target = station_find_target(self->position.x, self->position.y,data->facingX, data->facingY);
	
	station_highlight(data->target);

	if ((data->target) && (gfc_input_command_pressed(pickupCmd[data->index - 1])))
	{
		slog("picked up object");
		data->target->interact(data->target, self);
	}

	StationData* station_to_be_used = NULL;
	if ((data->target) && (data->target->use))
	{
		const char* cmd = useCmd[data->index - 1];
		if (data->target->buttonMashFlag)				//if player needs to mash, input is only pressed
		{
			if (gfc_input_command_pressed(cmd))
				station_to_be_used = data->target;
		}
		else
		{
			if (gfc_input_command_down(cmd))			//if player needs to hold, input is held
				station_to_be_used = data->target;
		}
	}


	if ((data->working_at) && (data->working_at != station_to_be_used))
	{
		if (data->working_at->user == self) 
			data->working_at->user = NULL;
		data->working_at = NULL;
	}

	if ((station_to_be_used) && ((!station_to_be_used->user) || (station_to_be_used->user == self)))
	{
		station_to_be_used->user = self;
		data->working_at = station_to_be_used;
		station_to_be_used->use(station_to_be_used, self, dt);
	}
	

	if (data->holding)
	{
		data->holding->position = gfc_vector3d(
			self->position.x + data->facingX * ITEM_HELD_FORWARD,
			self->position.y + data->facingY * ITEM_HELD_FORWARD,
			self->position.z + ITEM_HELD_HEIGHT);
	}

	return 0;
}

Entity* player_get_holding(Entity* player)
{
	PlayerEntityData* d;
	if ((!player) || (!player->data)) return NULL;
	d = player->data;
	return d->holding;
}

void player_set_holding(Entity* player, Entity* item) 
{
	PlayerEntityData* d;
	if ((!player) || (!player->data)) return;
	d = player->data;
	d->holding = item;
}