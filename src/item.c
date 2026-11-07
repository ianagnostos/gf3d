#include "simple_logger.h"

#include "gfc_config_def.h"

#include "gfc_config.h"
#include "item.h"
#include "player.h"

void item_free(Entity* self)
{
	ItemData* data;
	if ((!self) || (!self->data)) return;
	data = (ItemData*)self->data;
	free(data);
	self->data = NULL;
}

int item_think(Entity* self)
{
	if (!self) return 0;

	if (entity_collision_other( self, player_get(1)))
	{
		slog("give player 1 item");
		entity_free(self);
		return 1;
	}

	if (entity_collision_other(self, player_get(2)))
	{
		slog("give player 2 item");
		entity_free(self);
		return 1;
	}

	return 0;
}

Entity* spawn_item(const char* itemName, GFC_Vector3D position)
{
	SJson* itemdef;
	Entity* self;
	ItemData* data;
	const char* str;

	if (!itemName) return NULL;

	itemdef = gfc_config_def_get_by_name("items", itemName);
	if (!itemdef)
	{
		slog("cannot load item names");
		return NULL;
	}
	self = entity_new();
	if (!self) return NULL;

	data = gfc_allocate_array(sizeof(ItemData), 1);
	
	str = sj_object_get_string(itemdef, "model");
	if (str)
	{
		self->model = model_load(str);
	}

	slog("item %s model %p", itemName, (void*)self->model);

	self->think = item_think;
	self->bounds = gfc_box_from_config(sj_object_get_value(itemdef, "bounds"));
	self->data = data;
	self->scale = gfc_vector3d(1, 1, 1);
	self->solid = 0;
	self->position = position;
	self->free = item_free;
	self->color = sj_object_get_color(itemdef, "color");
	data = self->data;

	if (data)
	{
		data->name = sj_object_get_string(itemdef, "name");
		data->state = ITEM_RAW;
	}
	return self;
}

ItemData* item_get_data(Entity* item)
{
	if ((!item) || (!item->_inuse)) return NULL;
	return item->data;
}