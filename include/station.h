#ifndef __STATION_H_
#define __STATION_H_

#include "gf3d_entity.h"

typedef struct StationData_S
{
	Entity* ent;			//what entity this data belongs to
	GFC_Color stationColor; //color for turning station back to normal after highlighting

	Entity* user;			//player using the station, NULL if nobody using
	Entity* slot;			//items on the station, NULL if no items on station
	float progress;			//work done so far (time)
	float duration;			//amount of work that needs to be done (time)

	GFC_TextLine gives;			//what station give player when pickup pressed

	int (*can_accept)(struct StationData_S* self, Entity *item);
	void (*interact)(struct StationData_S* self, Entity* player);		//on tap
	void (*use)(struct StationData_S* self, Entity* player, float dt);	//held
}StationData;

//spawns station at given position from stations.def based on name given
Entity* station_spawn(const char* name, GFC_Vector3D position);

//free additonal data from station
void station_free(Entity* ent);

//returns additional data pointer from station
StationData* station_get_data(Entity* station);

//finds station nearest to point (x,y) + slack, in direction (fx, fy), returns NULL is no stations close enough
StationData* station_find_target(float x, float y, float fx, float fy);

//Called once a frame to avoid unessessary distance check
void station_clear_highlights();

//changes station color if it is targeted
void station_highlight(StationData* st);

#endif