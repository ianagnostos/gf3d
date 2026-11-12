#include "simple_logger.h"
#include "gfc_config.h"
#include "gfc_config_def.h"

#include "level.h"
#include "item.h"
#include "player.h"
#include "station.h"
#include "order.h"

#define MAX_STATIONS 44

#define REACH 8.0f
#define REACH_SLACK 2.0f

#define BAR_HEIGHT 7.0f	//how high progress bar is above station
#define BAR_SIZE 2.0f		//background bar y and z scale values (lowest layer)
#define BAR_Y_INC 0.4f		//scale added to y so progress bar layers dont overlap completely
#define BAR_Z_INC 0.5f		//scale added to z so progress bar layers dont overlap completely

#define SLAB_HEIGHT      0.9f		//thickness of one drawn plate slab
#define MAX_SLAB_DRAWN  8	

#define DIRTY_PLATE_COLOR gfc_color(0.45f, 0.38f, 0.30f, 1.0f)

extern float dt;

typedef struct
{
	char* mode;
	void (*setup)(StationData* st, SJson* def);
} StationType;

static StationData* stations[MAX_STATIONS] = { 0 };
static Model* barModel = NULL;

int station_update(Entity* self);

//drawing functions
int station_draw(Entity* self);
static void draw_slabs(Entity* self, int count, float offset_x, float offset_y, float size, GFC_Color color);
static void draw_cube(GFC_Vector3D center, GFC_Vector3D size, GFC_Color color);

//general helpers
static GFC_Vector3D slot_position(Entity* station, Entity* item); //place item onto a counter, NOT PLATE
static int  no_accept(StationData* self, Entity* item); //placeholder if stations does not accept anything
static int  place_ingredient_on_plate(StationData* self, Entity* player, Entity* held); //place ingredient onto plate, while plate is on counter
static const StationType* find_station_type(const char* mode); //find station type for setup functions

//counter
static int  counter_accept(StationData* self, Entity* item);
static void counter_interact(StationData* self, Entity* player);
static void counter_setup(StationData* st, SJson* def);

//crate
static void crate_interact(StationData* self, Entity* player);
static void crate_setup(StationData* st, SJson* def);
static void crate_draw(StationData* self);

//sink
static int  sink_accept(StationData* self, Entity* item);
static void sink_interact(StationData* self, Entity* player);
static void sink_setup(StationData* st, SJson* def);
static void sink_use(StationData* self, Entity* player, float dt);
static void sink_draw(StationData* self);

//chopping board
static int  board_accept(StationData* self, Entity* item);
static void board_interact(StationData* self, Entity* player);
static void board_use(StationData* self, Entity* player, float dt);
static void board_setup(StationData* st, SJson* def);

//stove
static int  stove_accept(StationData* self, Entity* item);
static void stove_interact(StationData* self, Entity* player);
static void stove_setup(StationData* st, SJson* def);
static void stove_tick(StationData* self, float dt);

// fryer
static int  fryer_accept(StationData* self, Entity* item);
static void fryer_interact(StationData* self, Entity* player);
static void fryer_setup(StationData* st, SJson* def);
static int  fryer_is_cooking(StationData* self);
static void fryer_tick(StationData* self, float dt);
static void fryer_draw(StationData* self);
static void fryer_use(StationData* self, Entity* player, float dt);
static float fryer_marker_position(StationData* self);

//serving window
static int  serving_accept(StationData* self, Entity* item);
static void serving_interact(StationData* self, Entity* player);
static void serving_setup(StationData* st, SJson* def);
static void serving_tick(StationData* self, float dt);
static void serving_draw(StationData* self);

//trash can
static int  trash_accept(StationData* self, Entity* item);
static void trash_interact(StationData* self, Entity* player);
static void trash_setup(StationData* st, SJson* def);

static const StationType station_types[] =
{
	{ "counter",  counter_setup },
	{ "crate",    crate_setup },
	{ "chopping", board_setup },
	{ "serving",  serving_setup },
	{ "cooking",    stove_setup },
	{ "trash",    trash_setup },
	{ "cleaning",     sink_setup },
	{ "frying",    fryer_setup },
};


Entity* station_spawn(const char* name, GFC_Vector3D position)
{
	SJson* def;
	Entity* ent;
	StationData* data;
	StationType* type;
	const char* str;
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
	ent->update = station_update;

	data->ent = ent;
	data->stationColor = ent->color;
	
	//setup specific to station type
	type->setup(data, def);

	ent->data = data;
	ent->free = station_free;
	stations[i] = data;
	return ent;
}

int station_update(Entity* self)
{
	StationData* data;
	if ((!self) || (!self->data)) return 0;
	data = (StationData*)self->data;
	if (data->tick) data->tick(data, dt);
	return 0;
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

int station_draw(Entity* self)
{
	GFC_Matrix4 barMat;
	StationData* data;
	GFC_Color barColor = GFC_COLOR_GREEN;
	float percentDone = 0;
	if ((!self) || (!self->data)) return 0;
	data = self->data;

	if ((data->duration > 0) && (data->progress > 0))
		percentDone = data->progress / data->duration;
	else if ((data->burnDuration > 0) && (data->burn > 0))
	{
		percentDone = data->burn / data->burnDuration;
		barColor = GFC_COLOR_RED;
	}


	if (percentDone > 0)
	{
		float barWidth = self->bounds.w;
		float barX = self->position.x + self->bounds.x + (self->bounds.w * 0.5f);
		float barY = self->position.y + self->bounds.y + (self->bounds.h * 0.5f);
		float barZ = self->position.z + self->bounds.z + self->bounds.d + BAR_HEIGHT;

		if (!barModel)
		{
			barModel = model_load("models/primitives/cube.model");
			if (!barModel)
			{
				slog("no progress bar model found");
				return 0;
			}
		}

		//black background bar - layer 0
		gfc_matrix4_from_vectors
		(barMat,
			gfc_vector3d(barX, barY, barZ),
			gfc_vector3d(0, 0, 0),
			gfc_vector3d(barWidth, BAR_SIZE, BAR_SIZE));
		gf3d_model_queue_render(barModel, barMat, GFC_COLOR_BLACK);

		//progress bar - layer 1
		gfc_matrix4_from_vectors
		(barMat,
			gfc_vector3d(barX - barWidth * 0.5f + (barWidth * percentDone) * 0.5f, barY, barZ + 0.5f),
			gfc_vector3d(0, 0, 0),
			gfc_vector3d(barWidth * percentDone, BAR_SIZE + BAR_Y_INC, BAR_SIZE + BAR_Z_INC));
		gf3d_model_queue_render(barModel, barMat, barColor);
	}
	if (data->draw) data->draw(data);
	return 1;
}

static void draw_slabs(Entity* self, int count, float offset_x, float offset_y, float size, GFC_Color color)
{
	GFC_Matrix4 slabMatrix;
	float slabX, slabY, slabZ;
	int i;

	if (count <= 0) return;

	if (!barModel) //barModel is just a cube, and everything is cube, change if everything is not a cube anymore
	{
		barModel = model_load("models/primitives/cube.model");
		if (!barModel) return;
	}

	slabX = self->position.x + self->bounds.x + self->bounds.w * 0.5f + offset_x;
	slabY = self->position.y + self->bounds.y + self->bounds.h * 0.5f + offset_y;
	slabZ = self->position.z + self->bounds.z + self->bounds.d;

	for (i = 0; (i < count) && (i < MAX_SLAB_DRAWN); i++)
	{
		gfc_matrix4_from_vectors(slabMatrix,
			gfc_vector3d(slabX, slabY, slabZ + SLAB_HEIGHT * 0.5f + i * SLAB_HEIGHT),
			gfc_vector3d(0, 0, 0),
			gfc_vector3d(size, size, SLAB_HEIGHT));
		gf3d_model_queue_render(barModel, slabMatrix, color);
	}
}

static void draw_cube(GFC_Vector3D center, GFC_Vector3D size, GFC_Color color)
{
	GFC_Matrix4 mat;
	if (!barModel)
	{
		barModel = model_load("models/primitives/cube.model");
		if (!barModel) return;
	}
	gfc_matrix4_from_vectors(mat, center, gfc_vector3d(0, 0, 0), size);
	gf3d_model_queue_render(barModel, mat, color);
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
	if ((self->stock == 0) || (player_get_holding(player))) return;
	item = spawn_item(self->gives, self->ent->position);
	if (!item) return;
	player_set_holding(player, item);
	if (self->stock > 0) self->stock--; //only decrement from stock if its not -1
	return;
}

static void crate_setup(StationData* st, SJson* def)
{
	if (!sj_object_line_value(def, "gives", st->gives))
		slog("crate has no gives in its def");
	st->stock = -1;
	if (sj_object_get_value(def, "stock"))
	{
		float amount = 0;
		sj_get_float_value(sj_object_get_value(def, "stock"), &amount);
		st->stock = amount;
	}
	st->interact = crate_interact;
	st->can_accept = no_accept;
	st->draw = crate_draw;
}

static void crate_draw(StationData* self)
{
	draw_slabs(self->ent, self->stock, 0, 0, 6, GFC_COLOR_WHITE); //plates
}

//sink
static int sink_accept(StationData* self, Entity* plate)
{
	return (plate_is_dirty(plate)) && (self->dirtyCount < MAX_PLATES);
}

static void sink_interact(StationData* self, Entity* player)
{
	Entity* held = player_get_holding(player);
	Entity* plate;

	//player holding something
	if (held)                                     
	{
		if (!self->can_accept(self, held)) return;
		player_set_holding(player, NULL);          
		entity_free(held);
		self->dirtyCount++;
		return;
	}

	//player has empty hands
	if (self->cleanCount <= 0) return;             
	plate = spawn_item("plate", self->ent->position);
	if (!plate) return;
	player_set_holding(player, plate);
	self->cleanCount--;
}

static void sink_setup(StationData* st, SJson* def)
{
	st->interact = sink_interact;
	st->can_accept = sink_accept;
	st->use = sink_use;
	st->buttonMashFlag = 1;
	sj_get_float_value(sj_object_get_value(def, "duration"), &st->duration);
	st->draw = sink_draw;
}

static void sink_use(StationData* self, Entity* player, float dt)
{
	if (self->dirtyCount <= 0) return;
	self->progress += 1.0f;                
	if (self->progress >= self->duration)
	{
		self->progress = 0;
		self->dirtyCount--;
		self->cleanCount++;
	}
}

static void sink_draw(StationData* self)
{
	Entity* ent = self->ent;
	float x = ent->position.x + ent->bounds.x + ent->bounds.w * 0.5f;
	float y = ent->position.y + ent->bounds.y + ent->bounds.h * 0.5f;
	float top = ent->position.z + ent->bounds.z + ent->bounds.d;

	//sink basin
	draw_cube(gfc_vector3d(x + 5.0f, y, top + 0.15f),
		gfc_vector3d(9.0f, 8.0f, 0.3f),
		gfc_color(0.25f, 0.45f, 0.8f, 1.0f));

	//dirty plates
	draw_slabs(ent, self->dirtyCount, 5, 0, 4, DIRTY_PLATE_COLOR);
	//clean plates
	draw_slabs(ent, self->cleanCount, -5, 0, 4, GFC_COLOR_WHITE);
}

//chopping board
static int board_accept(StationData* self, Entity* item)
{
	ItemData* data = item_get_data(item);
	return (!self->slot) && (data) && (data->choppable) && (data->state == ITEM_RAW);
}

static void board_interact(StationData* self, Entity* player)
{
	counter_interact(self, player);
	if (!self->slot) self->progress = 0;
}

static void board_use(StationData* self, Entity* player, float dt)
{
	ItemData* data = item_get_data(self->slot);
	if ((!data) || (!data->choppable) || (data->state != ITEM_RAW)) return;
	self->progress += dt;
	if (self->progress >= self->duration)
	{
		data->state = ITEM_CHOPPED;
		self->slot->color = data->choppedColor;
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

//stove
static int stove_accept(StationData* self, Entity* item)
{
	ItemData* data = item_get_data(item);
	if ((self->slot) || (!data) || (!data->cookable)) return 0;
	if ((data->state == ITEM_COOKED) || (data->state == ITEM_BURNT)) return 0;
	if ((data->choppable) && (data->state != ITEM_CHOPPED)) return 0;
	return 1;
}

static void stove_interact(StationData* self, Entity* player)
{
	counter_interact(self, player);			 //same logic, make prettier if theres time
	if (!self->slot)
	{
		//reset timers
		self->progress = 0;     
		self->burn = 0;
	}
}

static void stove_setup(StationData* st, SJson* def)
{
	st->interact = stove_interact;
	st->can_accept = stove_accept;
	st->tick = stove_tick;
	sj_get_float_value(sj_object_get_value(def, "duration"), &st->duration);
	sj_get_float_value(sj_object_get_value(def, "burnDuration"), &st->burnDuration);
}

static void stove_tick(StationData* self, float dt)
{
	ItemData* data = item_get_data(self->slot);
	if (!data) return;

	if ((data->cookable) && (data->state != ITEM_COOKED) && (data->state != ITEM_BURNT))
	{
		self->progress += dt;
		if (self->progress >= self->duration)
		{
			data->state = ITEM_COOKED;
			self->slot->color = data->cookedColor;
			self->progress = 0;
		}
	}
	else if (data->state == ITEM_COOKED)
	{
		self->burn += dt;
		if (self->burn >= self->burnDuration)
		{
			data->state = ITEM_BURNT;
			self->slot->color = data->burntColor;
			self->burn = 0;
			slog("item burnt: %s", data->name);
		}
	}
}

//fryer
static int fryer_accept(StationData* self, Entity* item)
{
	ItemData* data = item_get_data(item);
	if ((self->slot) || (!data) || (!data->fryable)) return 0;
	return (data->state == ITEM_CHOPPED);
}

static void fryer_interact(StationData* self, Entity* player)
{
	counter_interact(self, player);
	//reset timers
	if (!self->slot) self->sweepTime = 0;       
}

static void fryer_setup(StationData* st, SJson* def)
{
	st->interact = fryer_interact;
	st->can_accept = fryer_accept;
	st->tick = fryer_tick;
	st->draw = fryer_draw;
	st->use = fryer_use;
	st->buttonMashFlag = 1;
	sj_get_float_value(sj_object_get_value(def, "sweepSpeed"), &st->sweepSpeed);
	sj_get_float_value(sj_object_get_value(def, "zoneStart"), &st->zoneStart);
	sj_get_float_value(sj_object_get_value(def, "zoneWidth"), &st->zoneWidth);
}

static int fryer_is_cooking(StationData* self)
{
	ItemData* data = item_get_data(self->slot);
	return (data) && (data->fryable) && (data->state == ITEM_CHOPPED);
}

static void fryer_tick(StationData* self, float dt)
{
	if (fryer_is_cooking(self)) self->sweepTime += dt;
}

static float fryer_marker_position(StationData* self)
{
	float pos = fmodf(self->sweepTime * self->sweepSpeed, 2.0f);
	if (pos <= 1.0f)
		return pos;
	else
		return 2.0f - pos;
}

static void fryer_draw(StationData* self)
{
	Entity* ent = self->ent;
	float barWidth, barLeftpos, barY, barZ, position;

	if (!fryer_is_cooking(self)) return;

	barWidth = ent->bounds.w;
	barLeftpos = ent->position.x + ent->bounds.x;
	barY = ent->position.y + ent->bounds.y + ent->bounds.h * 0.5f;
	barZ = ent->position.z + ent->bounds.z + ent->bounds.d + BAR_HEIGHT;

	//get position of player cube as fraction from 0 to 1
	position = fryer_marker_position(self);

	//background bar - layer 0
	draw_cube(
		gfc_vector3d(barLeftpos + barWidth * 0.5f, barY, barZ), 
		gfc_vector3d(barWidth, BAR_SIZE, BAR_SIZE), GFC_COLOR_BLACK);

	//green zone where player should press - layer 1
	draw_cube(
		gfc_vector3d(barLeftpos + barWidth * (self->zoneStart + self->zoneWidth * 0.5f), barY, barZ + 0.5f), 
		gfc_vector3d(barWidth * self->zoneWidth, BAR_SIZE + BAR_Y_INC, BAR_SIZE + BAR_Z_INC), GFC_COLOR_LIGHTGREEN);

	//cube representing player - layer 2
	draw_cube(
		gfc_vector3d(barLeftpos + barWidth * position, barY, barZ + 1.0f), 
		gfc_vector3d(1.0f, BAR_SIZE + (BAR_Y_INC * 2), BAR_SIZE + (BAR_Z_INC * 2)), GFC_COLOR_WHITE);
}

static void fryer_use(StationData* self, Entity* player, float dt)
{
	ItemData* data;
	float markerPos;

	if (!fryer_is_cooking(self)) return;           

	data = item_get_data(self->slot);
	markerPos = fryer_marker_position(self);
	if ((markerPos >= self->zoneStart) && (markerPos <= self->zoneStart + self->zoneWidth))
	{
		//hit
		data->state = ITEM_COOKED;                 
		self->slot->color = data->cookedColor;
	}
	else
	{
		//miss
		data->state = ITEM_BURNT;                  
		self->slot->color = data->burntColor;
		slog("item burnt: %s", data->name);
	}
	self->sweepTime = 0;
}

//serving window
static int serving_accept(StationData* self, Entity* item)
{
	return (plate_ingredient_count(item) > 0);
}

static void serving_interact(StationData* self, Entity* player)
{
	Entity* held = player_get_holding(player);
	Entity* plate;

	//if empty hands and there are dirty plates, player picks up dirty plates
	if (!held)                                 
	{
		if (self->dirtyCount <= 0) return;
		plate = spawn_item("plate", self->ent->position);
		if (!plate) return;
		plate_make_dirty(plate);
		player_set_holding(player, plate);
		self->dirtyCount--;
		return;
	}

	if (!self->can_accept(self, held)) return;
	
	if (order_serve(held))
	{
		player_set_holding(player, NULL);
		entity_free(held);
		if (self->returningPlateCount < MAX_PLATES)
			self->returningPlateTimers[self->returningPlateCount++] = self->returnDelay;
	}
}

static void serving_setup(StationData* st, SJson* def)
{
	st->interact = serving_interact;
	st->can_accept = serving_accept;
	st->tick = serving_tick;
	sj_get_float_value(sj_object_get_value(def, "returnDelay"), &st->returnDelay);
	st->draw = serving_draw;
}

static void serving_tick(StationData* self, float dt)
{
	int i = 0;
	while (i < self->returningPlateCount) //while there are plates missing
	{
		self->returningPlateTimers[i] -= dt;
		if (self->returningPlateTimers[i] <= 0)
		{
			self->dirtyCount++;
			self->returningPlateTimers[i] = self->returningPlateTimers[self->returningPlateCount - 1];   //fill the gap with the last one
			self->returningPlateCount--;										
		}
		else
		{
			i++;
		}
	}
}

static void serving_draw(StationData* self)
{
	//dirty plates waiting at the window
	draw_slabs(self->ent, self->dirtyCount, 0, -3, 4, DIRTY_PLATE_COLOR);
}

//trash
static int trash_accept(StationData* self, Entity* item)
{
	return (item != NULL);
}

static void trash_interact(StationData* self, Entity* player)
{
	Entity* held = player_get_holding(player);
	if ((!held) || (!self->can_accept(self, held))) return;

	if (item_is_plate(held))
	{
		plate_clear(held);                 // keep the plate
		return;
	}
	player_set_holding(player, NULL);
	entity_free(held);
}

static void trash_setup(StationData* st, SJson* def)
{
	st->interact = trash_interact;
	st->can_accept = trash_accept;
}