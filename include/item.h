#ifndef __ITEM_H__
#define __ITEM_H__

#include "gf3d_entity.h"

typedef enum { ITEM_RAW, ITEM_CHOPPED, ITEM_COOKED, ITEM_BURNT } ItemState;

typedef struct
{
	char* name;
	ItemState state;
}ItemData;

Entity* spawn_item(const char* itemName, GFC_Vector3D position);

ItemData* item_get_data(Entity* item);

void item_free(Entity* self);

#endif