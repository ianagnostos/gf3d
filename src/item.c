#include "simple_logger.h"

#include "gfc_config_def.h"

#include "gfc_config.h"
#include "item.h"
#include "player.h"


#define INGREDIENT_SIZE_ON_PLATE 2.0f
#define INGREDIENT_HEIGHT_ON_PLATE 1.5f

static Model* ingredientModel = NULL;

//plate
static int plate_can_add(Entity* plate, Entity* ingredient);
void plate_clear(Entity* plate);


Entity* spawn_item(const char* itemName, GFC_Vector3D position)
{
	SJson* itemdef;
	Entity* self;
	ItemData* data;
	GFC_Vector3D scale;
	const char* str;
	short int choppable = 0, isPlate = 0, cookable = 0, fryable = 0;

	if (!itemName) return NULL;

	itemdef = gfc_config_def_get_by_name("items", itemName);
	if (!itemdef)
	{
		slog("cannot load item named: %s", itemName);
		return NULL;
	}

	self = entity_new();
	if (!self) return NULL;

	data = gfc_allocate_array(sizeof(ItemData), 1);
	if (!data)
	{
		entity_free(self);
		return NULL;
	}

	str = sj_object_get_string(itemdef, "model");
	if (str)
	{
		self->model = model_load(str);
	}

	//self->think = item_think;
	self->bounds = gfc_box_from_config(sj_object_get_value(itemdef, "bounds"));
	self->data = data;
	if (sj_object_get_vector3d(itemdef, "scale", &scale))
		self->scale = scale;
	else
		self->scale = gfc_vector3d(1, 1, 1);
	self->solid = 0;
	self->position = position;
	self->free = item_free;
	self->color = sj_object_get_color(itemdef, "color");

	if (data)
	{
		gfc_line_cpy(data->name, itemName);
		data->state = ITEM_RAW;
		sj_get_bool_value(sj_object_get_value(itemdef, "choppable"), &choppable);
		data->choppable = choppable;
		data->choppedColor = sj_object_get_color(itemdef, "choppedColor");
		sj_get_bool_value(sj_object_get_value(itemdef, "cookable"), &cookable);
		data->cookable = cookable;
		sj_get_bool_value(sj_object_get_value(itemdef, "fryable"), &fryable);
		data->fryable = fryable;
		data->cookedColor = sj_object_get_color(itemdef, "cookedColor");
		data->burntColor = sj_object_get_color(itemdef, "burntColor");
		sj_get_bool_value(sj_object_get_value(itemdef, "plate"), &isPlate);
		data->isPlate = isPlate;
		if (data->isPlate)
		{
			data->dirtyColor = sj_object_get_color(itemdef, "dirtyColor");
			data->isDirty = 0;
			self->draw = plate_draw;
		}
	}
	return self;
}

void item_free(Entity* self)
{
	ItemData* data;
	if ((!self) || (!self->data)) return;
	data = (ItemData*)self->data;
	free(data);
	self->data = NULL;
}

ItemData* item_get_data(Entity* item)
{
	if ((!item) || (!item->_inuse)) return NULL;
	return item->data;
}

int item_is_plate(Entity* item)
{
	ItemData* data = item_get_data(item);
	if (!data)
	{
		slog("unable to get item data");
		return 0;
	}
	return data->isPlate;
}

int plate_ingredient_count(Entity* plate)
{
	ItemData* data = item_get_data(plate);
	if ((!data) || (!data->isPlate)) return 0;
	return data->ingredientCount;
}

int plate_add(Entity* plate, Entity* ingredient)
{
	ItemData* plateData;
	ItemData* ingredientData;
	int index;
	if (!plate_can_add(plate, ingredient)) return 0;
	plateData = item_get_data(plate);
	ingredientData = item_get_data(ingredient);
	index = plateData->ingredientCount++;
	gfc_line_cpy(plateData->ingredientName[index], ingredientData->name);
	plateData->ingredientState[index] = ingredientData->state;
	plateData->ingredientColor[index] = ingredient->color;
	return 1;
}

int plate_draw(Entity* self)
{
	ItemData* data;
	GFC_Matrix4 ingredientMatrix;
	//ingredient positions on plate 
	float offset_x[MAX_INGREDIENTS] = { -1.25f, 1.25f, -1.25f, 1.25f };
	float offset_y[MAX_INGREDIENTS] = { -1.25f, -1.25f, 1.25f, 1.25f };

	if (!self) return 0;

	data = (ItemData*)self->data;
	if ((!data) || (!data->ingredientCount)) return 0;
	
	if (!ingredientModel)
	{
		ingredientModel = model_load("models/primitives/cube.model");
		if (!ingredientModel)
		{
			slog("no ingredient model found");
			return 0;
		}
	}

	for (int i = 0; i < data->ingredientCount; i++)
	{
		gfc_matrix4_from_vectors(ingredientMatrix,
			gfc_vector3d(self->position.x + offset_x[i], self->position.y + offset_y[i], self->position.z + 1.5f),
			gfc_vector3d(0, 0, 0),
			gfc_vector3d(2, 2, 2));
		gf3d_model_queue_render(ingredientModel, ingredientMatrix, data->ingredientColor[i]);
	}
	return 0;
}

//plate
static int plate_can_add(Entity* plate, Entity* ingredient)
{
	ItemData* plateData = item_get_data(plate);
	ItemData* ingredientData = item_get_data(ingredient);
	//if there isnt a plate, or ingredient return. If plate or ingredient is not plate or ingredient return
	if ((!plateData) || (!ingredientData) || (!plateData->isPlate) || (ingredientData->isPlate)) return 0;
	//dirty plate can't hold food
	if (plateData->isDirty) return 0;
	//if plate is full return
	if (plateData->ingredientCount >= MAX_INGREDIENTS) return 0;
	//food that needs chopping or cooking can't go on when raw
	if (((ingredientData->choppable) || (ingredientData->cookable)) && (ingredientData->state == ITEM_RAW)) return 0;
	//fryable food must be cooked
	if ((ingredientData->fryable) && (ingredientData->state != ITEM_COOKED)) return 0;
	//burnt food is never allowed
	if (ingredientData->state == ITEM_BURNT) return 0;
	return 1;
}

void plate_clear(Entity* plate)
{
	ItemData* data = item_get_data(plate);
	if ((!data) || (!data->isPlate)) return;

	data->ingredientCount = 0;
	memset(data->ingredientName, 0, sizeof(data->ingredientName));
	memset(data->ingredientState, 0, sizeof(data->ingredientState));
	memset(data->ingredientColor, 0, sizeof(data->ingredientColor));
}

void plate_make_dirty(Entity* plate)
{
	ItemData* data = item_get_data(plate);
	if ((!data) || (!data->isPlate)) return;
	data->isDirty = 1;
	plate->color = data->dirtyColor;
}

int plate_is_dirty(Entity* item)
{
	ItemData* data = item_get_data(item);
	return (data) && (data->isDirty);
}
