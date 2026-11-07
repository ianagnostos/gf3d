#include "simple_logger.h"
#include "gfc_matrix.h"
#include "gf3d_entity.h"

#include "level.h"

#include "draw_3d.h"

typedef struct
{
	Entity* entityList;
	Uint32 maxEnts;
	//other
}EntityManager;

static EntityManager entity_manager = { 0 };

void entity_free(Entity* ent);

void entity_close()
{
	int i;
	if (!entity_manager.entityList) return;
	for (i = 0; i < entity_manager.maxEnts;i++)
	{
		if(entity_manager.entityList[i]._inuse)
			entity_free(&entity_manager.entityList[i]);
	}
	free(entity_manager.entityList);
	memset(&entity_manager, 0, sizeof(EntityManager));
	slog("entity system closed");
}

void entity_init(Uint32 maxEnts)
{
	if (!maxEnts)
	{
		slog("failed to initalize entity system: maxEnts is zero");
		return;
	}

	if (entity_manager.maxEnts != 0)
	{
		slog("You already have one");
		return;
	}

	entity_manager.entityList = gfc_allocate_array(sizeof(Entity), maxEnts);
	if (!entity_manager.entityList)
	{
		slog("failed to allocate entity list for %i entities", maxEnts);
		return;
	}
	entity_manager.maxEnts = maxEnts;
	slog("entity system initialized");
	atexit(entity_close);
}

Entity* entity_new()
{
	int i;
	if (!entity_manager.entityList)
	{
		slog("No enitity list");
		return NULL;
	}

	for (i = 0; i < entity_manager.maxEnts; i++)
	{
		Entity* ent = &entity_manager.entityList[i];
		if (ent->_inuse) continue;
		memset(ent, 0, sizeof(Entity));
		//any boilerplate code goes here
		ent->_inuse = 1;
		ent->scale = gfc_vector3d(1, 1, 1);
		ent->color = GFC_COLOR_WHITE;
		return ent;
	}
	return NULL;
}

void entity_free(Entity* ent)
{
	if (!ent) return;
	if (!ent->_inuse) return;
	if (ent->free) ent->free(ent);
	model_free(ent->model);
	memset(ent, 0, sizeof(Entity));
}

void entity_draw(Entity* self)
{
	GFC_Box debug;
	if ((!self) || (!self->_inuse)) return;
	if (self->draw)
	{
		self->draw(self);
	}

	gfc_matrix4_from_vectors(
		self->mat,
		self->position,
		self->rotation,
		self->scale);
	gf3d_model_queue_render(self->model, self->mat, self->color);

	/*
	debug = self->bounds;
	gfc_vector3d_add(debug, self->bounds, self->position);
	draw_3d_cube(debug);
	*/
}

void entity_system_draw()
{
	int i;
	for (i = 0; i < entity_manager.maxEnts;i++)
	{
		if (!entity_manager.entityList[i]._inuse) continue;
		entity_draw(&entity_manager.entityList[i]);
	}
}

void entity_think(Entity *self)
{
	if ((!self) || (!self->_inuse)) return;
	if (self->think)
	{
		self->think(self);
	}

}

void entity_system_think()
{
	int i;
	for (i = 0; i < entity_manager.maxEnts;i++)
	{
		if (!entity_manager.entityList[i]._inuse) continue;
		entity_think(&entity_manager.entityList[i]);
	}
}

void entity_update(Entity *self)
{
	if ((!self) || (!self->_inuse)) return;

	if (!entity_move_check(self, self->velocity))
	{
		gfc_vector3d_add(self->position, self->position, self->velocity);
	}

	if (self->update)
	{
		self->update(self);
	}
}

void entity_system_update()
{
	int i;
	for (i = 0; i < entity_manager.maxEnts;i++)
	{
		if (!entity_manager.entityList[i]._inuse) continue;
		entity_update(&entity_manager.entityList[i]);
	}
}

int entity_collision_other(Entity* self, Entity* other)
{
	GFC_Box a, b;
	if ((!self) || (!other)) return 0;

	a = self->bounds;
	b = other->bounds;

	gfc_vector3d_add(a, a, self->position);
	gfc_vector3d_add(b, b, other->position);

	return gfc_box_overlap(a, b);
}

int entity_move_check(Entity* self, GFC_Vector3D offset)
{
	GFC_Box aabb, other;
	Entity* temp;
	if (!self) return 0;

	aabb = self->bounds;
	gfc_vector3d_add(aabb, aabb, self->position);
	gfc_vector3d_add(aabb, aabb, offset);

	if (level_bounds_check(level_get_the(), aabb)) return 1;

	for (int i = 0; i < entity_manager.maxEnts; i++)
	{
		temp = &entity_manager.entityList[i];
		if ((!temp->_inuse) || (temp == self) || (!temp->solid)) continue;
		other = temp->bounds;
		gfc_vector3d_add(other, other, temp->position);

		if (gfc_box_overlap(other, aabb))
		{
			return 1;
		}
		
	}
	return 0;
}