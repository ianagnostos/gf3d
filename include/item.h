#ifndef __ITEM_H_
#define __ITEM_H_

#include "gf3d_entity.h"

#define MAX_INGREDIENTS 4	//plate can hold MAX_INGREDIENT items (placing_item_onto_plate code assumes 4)

typedef enum { ITEM_RAW, ITEM_CHOPPED, ITEM_COOKED, ITEM_BURNT } ItemState;

typedef struct
{
	GFC_TextLine	name;

	//ingredients
	ItemState		state;
	int				choppable;
	GFC_Color		choppedColor;

	//cooking
	int         cookable;
	int			fryable;
	GFC_Color   cookedColor;
	GFC_Color   burntColor;

	//plates
	int isPlate;
	int isDirty;
	GFC_Color dirtyColor;
	int ingredientCount;
	GFC_TextLine ingredientName[MAX_INGREDIENTS];
	ItemState ingredientState[MAX_INGREDIENTS];
	GFC_Color ingredientColor[MAX_INGREDIENTS];

}ItemData;


//spawns station at given position from items.def based on name given
Entity* spawn_item(const char* itemName, GFC_Vector3D position);

//free additonal data from item
void item_free(Entity* self);

//returns additional data pointer from item
ItemData* item_get_data(Entity* item);

//plates
// 1 if plate, 0 if not
int item_is_plate(Entity* item);

// returns amount of ingredients of plate, 0 if none
int plate_ingredient_count(Entity* plate);

//copies the ingredient's name, state, and color onto the plate, 0 if the plate can't take it. 
// the ingredient entity is NOT freed
int plate_add(Entity* plate, Entity* ingredient);

//draws each ingredient item on plate
int plate_draw(Entity* self);

//empties contents on plate (sets ingredient count to zero and memset 0 ing arrays)
void plate_clear(Entity* plate);

//set isDirty to 1
void plate_make_dirty(Entity* plate);

//returns 1 if dirty plate, 0 if not
int plate_is_dirty(Entity* item);

#endif