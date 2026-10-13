#ifndef __ENTITY_H__
#define __ENTITY_H__

#include "gf3d_model.h"

typedef struct Entity_S
{
	Uint8			_inuse;
	Model*			model;
	GFC_Vector3D	position;
	GFC_Vector3D	rotation;
	GFC_Vector3D	scale;
	GFC_Matrix4		mat;

	GFC_Vector3D	velocity;
	GFC_Color		color;

	int			(*think)(struct Entity_S *self);
	int			(*draw)(struct Entity_S* self);
	int			(*update)(struct Entity_S* self);

	void* data;

}Entity;

/**
* @brief initalize the entity manager and pre - allocate a bunch
* @param maxEnts how many entities can exist at any single moment
*/
void entity_init(Uint32 MaxEnts);

Entity *entity_new();

void entity_free(Entity* ent);

/*
* @brief run updates on all active entities
*/
void entity_system_update();

void entity_system_think();

/*
* queue renders on all entities
*/
void entity_system_draw();


#endif