#ifndef __STATION_H_
#define __STATION_H_

#include "gf3d_entity.h"

typedef struct StationData_S
{
	Entity* ent;
	char* gives;
	Entity* slot;
	GFC_Color stationColor;
	int (*can_accept)(struct StationData_S* self, Entity *item);
	void (*interact)(struct StationData_S* self, Entity* player);
}StationData;

Entity* station_spawn(const char* name, GFC_Vector3D position);

StationData* station_get_data(Entity* station);

StationData* station_find_target(float x, float y, float fx, float fy);

void station_clear_highlights(void);

void station_highlight(StationData* st);

void station_free(Entity* ent);


#endif