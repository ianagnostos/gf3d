#include "simple_logger.h"
#include "gfc_config.h"
#include "gfc_config_def.h"

#include "level.h"
#include "item.h"
#include "player.h"
#include "station.h"
#include "order.h"

#define MAX_STATIONS 32
#define REACH 8.0f
#define REACH_SLACK 2.0f
#define PROGRESS_BAR_HEIGHT 7.0f

typedef struct
{
	char* mode;
	void (*setup)(StationData* st, SJson* def);
} StationType;

static StationData* stations[MAX_STATIONS] = { 0 };
static Model* barModel = NULL;

int station_draw(Entity* self);

//general helpers
static GFC_Vector3D slot_position(Entity* station, Entity* item); //place item onto a counter, NOT PLATE
static int  no_accept(StationData* self, Entity* item); //placeholder if stations does not accept anything
static int  place_ingredient_on_plate(StationData* self, Entity* player, Entity* held); //place ingredient onto plate, while plate is on counter
static const StationType* find_station_type(char* mode); //find station type for setup functions

// counter
static int  counter_accept(StationData* self, Entity* item);
static void counter_interact(StationData* self, Entity* player);
static void counter_setup(StationData* st, SJson* def);

// crate
static void crate_interact(StationData* self, Entity* player);
static void crate_setup(StationData* st, SJson* def);

// chopping board
static int  board_accept(StationData* self, Entity* item);
static void board_interact(StationData* self, Entity* player);
static void board_use(StationData* self, Entity* player, float dt);
static void board_setup(StationData* st, SJson* def);

// serving window
static int  serving_accept(StationData* self, Entity* item);
static void serving_interact(StationData* self, Entity* player);
static void serving_setup(StationData* st, SJson* def);

static const StationType station_types[] =
{
	{ "counter",  counter_setup },
	{ "crate",    crate_setup },
	{ "chopping", board_setup },
	{ "serving",  serving_setup },
};


Entity* station_spawn(const char* name, GFC_Vector3D position)
{
	SJson* def;
	Entity* ent;
	StationData* data;
	StationType* type;
	char* str;
	GFC_Vector3D v;
	int i;

	if (!name) return NULL;
	def = gfc_config_def_get_by_name("stations", name);
	if (!def)
	{
		slog("no station def named %s", name);
		return NULL;
	}

	type = find_station_type(sj_object_get_string(def, "mode"));
	if (!type)
	{
		slog("station '%s' has a missing or unknown mode", name);
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
	data = gfc_allocate_array(sizeof(StationData), 1);
	if (!data)
	{
		entity_free(ent);
		return NULL;
	}

	//setup that is the same for all stations
	str = sj_object_get_string(def, "model");
	if (str) ent->model = model_load(str);
	ent->bounds = gfc_box_from_config(sj_object_get_value(def, "bounds"));
	ent->color = sj_object_get_color(def, "color");
	if (sj_object_get_vector3d(def, "scale", &v)) ent->scale = v;
	ent->solid = 1;
	ent->position = position;
	ent->draw = station_draw;

	data->ent = ent;
	data->stationColor = ent->color;
	str = sj_object_get_string(def, "mode");
	
	//setup specific to station type
	type->setup(data, def);

	ent->data = data;
	ent->free = station_free;
	stations[i] = data;
	return ent;
}

void station_free(Entity* ent)
{
	int i;
	StationData* data;
	if ((!ent) || (!ent->data)) return;
	data = (StationData*)ent->data;
	for (i = 0; i < MAX_STATIONS; i++)
		if (stations[i] == data) stations[i] = NULL;
	free(data);
	ent->data = NULL;
}

//only draws progress bar
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

		//black background bar
		gfc_matrix4_from_vectors
		(barMat,
			gfc_vector3d(bar_x, bar_y, bar_z),
			gfc_vector3d(0, 0, 0),
			gfc_vector3d(bar_width, 2, 2));
		gf3d_model_queue_render(barModel, barMat, GFC_COLOR_BLACK);

		//green bar
		gfc_matrix4_from_vectors
		(barMat,
			gfc_vector3d(bar_x - bar_width * 0.5f + (bar_width * percent_done) * 0.5f, bar_y, bar_z + 0.5f),
			gfc_vector3d(0, 0, 0),
			gfc_vector3d(bar_width * percent_done, 2, 2.5f));
		gf3d_model_queue_render(barModel, barMat, GFC_COLOR_LIGHTGREEN);
	}
	return 0;
}

StationData* station_get_data(Entity* station)
{
	if ((!station) || (!station->_inuse)) return NULL;
	return (StationData*)station->data;
}

StationData* station_find_target(float x, float y, float fx, float fy)
{
	int i;
	StationData* target = NULL;
	float distance = REACH_SLACK;
	float reach_x = x + fx * REACH;
	float reach_y = y + fy * REACH;

	for (i = 0; i < MAX_STATIONS; i++)
	{
		GFC_Box b;
		float dx = 0, dy = 0, d = 0;
		StationData* data = stations[i];
		if (!data) continue;

		b = data->ent->bounds;
		b.x += data->ent->position.x;
		b.y += data->ent->position.y;

		if (reach_x < b.x)
		{
			dx = b.x - reach_x;
		}
		else if (reach_x > b.x + b.w)
		{
			dx = reach_x - (b.x + b.w);
		}

		if (reach_y < b.y)
		{
			dy = b.y - reach_y;
		}
		else if (reach_y > b.y + b.h)
		{
			dy = reach_y - (b.y + b.h);
		}

		d = sqrtf(dx * dx + dy * dy);
		if (d < distance)
		{
			distance = d;
			target = stations[i];
		}
	}
	return target;
}

void station_highlight(StationData* st)
{
	if (st) st->ent->color = GFC_COLOR_LIGHTYELLOW;
}

void station_clear_highlights()
{
	for (int i = 0; i < MAX_STATIONS; i++)
	{
		if (!stations[i]) continue;
		stations[i]->ent->color = stations[i]->stationColor;
	}
}

//general helpers
static GFC_Vector3D slot_position(Entity* station, Entity *item)
{
	GFC_Box b = station->bounds;
	return gfc_vector3d(station->position.x + b.x + b.w * 0.5f,
						station->position.y + b.y + b.h * 0.5f,
						station->position.z + b.z + b.d - item->bounds.z);
}

static int no_accept(StationData* self, Entity* item) { return 0; }

static int place_ingredient_on_plate(StationData* self, Entity* player, Entity* held)
{
	Entity* plate;
	Entity* ingredient;
	int fromHand;

	if (!self->slot) return 0;

	if ((item_is_plate(held)) && (!item_is_plate(self->slot)))
	{
		plate = held;
		ingredient = self->slot;
		fromHand = 0;
	}
	else if ((!item_is_plate(held)) && (item_is_plate(self->slot)))
	{
		plate = self->slot;
		ingredient = held;
		fromHand = 1;
	}
	else
		return 0;

	if (!plate_add(plate, ingredient)) return 0;

	if (fromHand)
		player_set_holding(player, NULL);
	else
		self->slot = NULL;

	entity_free(ingredient);
	return 1;
}

static const StationType* find_station_type(char* mode)
{
	if (!mode) return NULL;
	for (int i = 0; i < (int)(sizeof(station_types) / sizeof(station_types[0])); i++)
	{
		if (strcmp(mode, station_types[i].mode) == 0) return &station_types[i];
	}
	return NULL;
}

//counter
static int counter_accept(StationData* self, Entity* item)
{ 
	return (item) && (!self->slot); 
}

static void counter_interact(StationData* self, Entity* player)
{
	Entity* held = player_get_holding(player);

	if ((held) && (place_ingredient_on_plate(self, player, held))) return;

	if ((held) && (self->can_accept(self, held)))
	{
		player_set_holding(player, NULL);
		self->slot = held;
		held->position = slot_position(self->ent, held);
	}
	else if ((!held) && (self->slot))
	{
		player_set_holding(player, self->slot);
		self->slot = NULL;
	}
}

static void counter_setup(StationData* st, SJson* def)
{
	st->interact = counter_interact;
	st->can_accept = counter_accept;
}

//crate
static void crate_interact(StationData* self, Entity* player)
{
	Entity* item;
	if (player_get_holding(player)) return;
	item = spawn_item(self->gives, self->ent->position);
	if (item) player_set_holding(player, item);
	return;
}

static void crate_setup(StationData* st, SJson* def)
{
	if (!sj_object_line_value(def, "gives", st->gives))
		slog("crate has no gives in its def");
	st->interact = crate_interact;
	st->can_accept = no_accept;
}

//chopping board
static int board_accept(StationData* self, Entity* item)
{
	ItemData* d = item_get_data(item);
	return (!self->slot) && (d) && (d->choppable) && (d->state == ITEM_RAW);
}

static void board_interact(StationData* self, Entity* player)
{
	counter_interact(self, player);
	if (!self->slot) self->progress = 0;
}

static void board_use(StationData* self, Entity* player, float dt)
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

static void board_setup(StationData* st, SJson* def)
{
	st->interact = board_interact;
	st->can_accept = board_accept;
	st->use = board_use;
	sj_get_float_value(sj_object_get_value(def, "duration"), &st->duration);
}

//serving window
int serving_accept(StationData* self, Entity* item)
{
	return (plate_ingredient_count(item) > 0);
}

void serving_interact(StationData* self, Entity* player)
{
	Entity* held = player_get_holding(player);
	if ((!held) || (!self->can_accept(self, held))) return;
	
	if (order_serve(held))
	{
		player_set_holding(player, NULL);
		entity_free(held);
	}
}

static void serving_setup(StationData* st, SJson* def)
{
	st->interact = serving_interact;
	st->can_accept = serving_accept;
}
