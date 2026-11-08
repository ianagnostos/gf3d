#include "simple_logger.h"
#include "gfc_config.h"
#include "gfc_config_def.h"

#include "level.h"
#include "item.h"
#include "player.h"
#include "station.h"

#define MAX_STATIONS 32
#define REACH 8.0f

static StationData* stations[MAX_STATIONS] = { 0 };
static Model* barModel = NULL;

GFC_Vector3D slot_position(Entity* self)
{
	GFC_Box b = self->bounds;
	return gfc_vector3d(self->position.x + b.x + b.w * 0.5f,
		self->position.y + b.y + b.h * 0.5f,
		self->position.z + b.z + b.d + 2.0f);
}

int no_accept(StationData* self, Entity* item) { return 0; }
int counter_accept(StationData* self, Entity* item) { return (item) && (!self->slot); }

int board_accept(StationData* self, Entity* item)
{
	ItemData* d = item_get_data(item);
	return (!self->slot) && (d) && (d->choppable) && (d->state == ITEM_RAW);
}

void board_use(StationData* self, Entity* player, float dt)
{
	ItemData* d = item_get_data(self->slot);
	if ((!d) || (!d->choppable) || (d->state != ITEM_RAW)) return;
	self->progress += dt;
	if (self->progress >= self->duration)
	{
		d->state = ITEM_CHOPPED;
		self->slot->color = d->choppedColor;
		self->progress = 0;
	}
}


void counter_interact(StationData* self, Entity* player)
{
	Entity* held = player_get_holding(player);
	if ((held) && (self->can_accept(self, held)))
	{
		player_set_holding(player, NULL);
		self->slot = held;
		held->position = slot_position(self->ent);
	}
	else if ((!held) && (self->slot))
	{
		player_set_holding(player, self->slot);
		self->slot = NULL;
	}
}

void board_interact(StationData* self, Entity* player)
{
	counter_interact(self, player);
	if (!self->slot) self->progress = 0;
}

void crate_interact(StationData* self, Entity* player)
{
	Entity *item;
	if (player_get_holding(player)) return;

	slog("crate: gives '%s'", self->gives);
	item = spawn_item(self->gives, self->ent->position);
	slog("crate: spawned %p", (void*)item);

	if (item) player_set_holding(player, item);

}

void station_free(Entity* ent)
{
	int i;
	StationData* sd;
	if ((!ent) || (!ent->data)) return;
	sd = (StationData*)ent->data;
	for (i = 0; i < MAX_STATIONS; i++)
		if (stations[i] == sd) stations[i] = NULL;
	free(sd);
	ent->data = NULL;
}

int station_draw(Entity* self)
{
	GFC_Matrix4 barMat;
	StationData* data;
	if ((!self) || (!self->data)) return 0;
	data = self->data;

	if ((data->duration > 0) && (data->progress > 0) && (data->slot))
	{
		float percent_done = data->progress / data->duration;
		float bar_width = self->bounds.w;
		float bar_x = self->position.x + self->bounds.x + (self->bounds.w * 0.5f);
		float bar_y = self->position.y + self->bounds.y + (self->bounds.h * 0.5f);
		float bar_z = self->position.z + self->bounds.z + self->bounds.d + 7.0f;

		if (!barModel)
		{
			barModel = model_load("models/primitives/cube.model");
			if (!barModel)
			{
				slog("no progress bar model found");
				return 0;
			}
		}

		gfc_matrix4_from_vectors
		(barMat,
			gfc_vector3d(bar_x, bar_y, bar_z),
			gfc_vector3d(0, 0, 0),
			gfc_vector3d(bar_width, 2, 2));
		gf3d_model_queue_render(barModel, barMat, GFC_COLOR_BLACK);

		gfc_matrix4_from_vectors
		(barMat,
			gfc_vector3d(bar_x - bar_width * 0.5f + (bar_width * percent_done) * 0.5f, bar_y, bar_z + 0.5f),
			gfc_vector3d(0, 0, 0),
			gfc_vector3d(bar_width * percent_done, 2, 2.5f));
		gf3d_model_queue_render(barModel, barMat, GFC_COLOR_LIGHTGREEN);
	}
	return 0;
}


Entity* station_spawn(const char* name, GFC_Vector3D position)
{
	slog("in station spawn");
	SJson* def;
	Entity* ent;
	StationData* st;
	const char* str;
	GFC_Vector3D v;
	int i;

	if (!name) return NULL;
	def = gfc_config_def_get_by_name("stations", name);
	if (!def)
	{
		slog("no station def named");
		return NULL;
	}
	for (i = 0; i < MAX_STATIONS; i++) 
		if (!stations[i]) break;

	if (i == MAX_STATIONS)
	{
		slog("too many stations");
		return NULL;
	}

	ent = entity_new();
	if (!ent) return NULL; 
	st = gfc_allocate_array(sizeof(StationData), 1);
	if (!st)
	{
		entity_free(ent);
		return NULL;
	}

	str = sj_object_get_string(def, "model");
	if (str) ent->model = model_load(str);
	ent->bounds = gfc_box_from_config(sj_object_get_value(def, "bounds"));
	ent->color = sj_object_get_color(def, "color");
	if (sj_object_get_vector3d(def, "scale", &v)) ent->scale = v;
	ent->solid = 1;
	ent->position = position;
	ent->draw = station_draw;

	st->ent = ent;
	st->stationColor = ent->color;
	str = sj_object_get_string(def, "mode");
	if ((str) && (strcmp(str, "crate") == 0))
	{
		st->interact = crate_interact;
		st->can_accept = no_accept;
		str = sj_object_get_string(def, "gives");
		if (str)
		{
			slog("%s", str);
			st->gives = str;
		}
	}
	else if ((str) && strcmp(str, "chopping") == 0)
	{
		st->interact = board_interact;
		st->can_accept = board_accept;
		st->use = board_use;
		sj_get_float_value(sj_object_get_value(def, "duration"), &st->duration); 
	}
	else
	{
		st->interact = counter_interact;
		st->can_accept = counter_accept;
	}

	ent->data = st;
	ent->free = station_free;
	stations[i] = st;
	return ent;
}

StationData* station_find_target(float x, float y, float fx, float fy)
{
	int i;
	StationData* target = NULL;
	float distance = 2.0f;
	float rx = x + fx * REACH;
	float ry = y + fy * REACH;

	for (i = 0; i < MAX_STATIONS; i++)
	{
		GFC_Box b;
		float dx, dy, d;
		StationData* sd = stations[i];
		if (!sd) continue;

		b = sd->ent->bounds;
		b.x += sd->ent->position.x;
		b.y += sd->ent->position.y;
		dx = fmaxf(fmaxf(b.x - rx, 0), rx - (b.x + b.w));   
		dy = fmaxf(fmaxf(b.y - ry, 0), ry - (b.y + b.h));    
		d = sqrtf(dx * dx + dy * dy);
		if (d < distance)
		{ 
			distance = d; 
			target = stations[i]; 
		}
	}
	return target;
}

void station_clear_highlights(void)
{
	for (int i = 0; i < MAX_STATIONS; i++)
	{
		if (!stations[i]) continue;
		stations[i]->ent->color = stations[i]->stationColor;
	}
}

void station_highlight(StationData* st)
{
	if (st) st->ent->color = gfc_color(1.0f, 1.0f, 0.4f, 1.0f);
}


