#ifndef __STATION_H_
#define __STATION_H_

#include "gf3d_entity.h"

#define MAX_PLATES 4

typedef struct StationData_S
{
	Entity* ent;			//what entity this data belongs to
	GFC_Color stationColor; //color for turning station back to normal after highlighting

	Entity* user;			//player using the station, NULL if nobody using
	Entity* slot;			//items on the station, NULL if no items on station
	float progress;			//work done so far (time)
	float duration;			//amount of work that needs to be done (time)

	//stove
	float burn;				//seconds since cooked 
	float burnDuration;     //seconds till burnt

	//fryer
	float       sweepTime;       //how long its been sweeping
	float       sweepSpeed;      //bar lengths per second
	float       zoneStart;       //where the green zone begins (0 to 1 along the bar)
	float       zoneWidth;		 //how wide the green zone is (0 to 1)

	//crates
	GFC_TextLine gives;		//what station give player when pickup pressed
	int stock;				//how many objects left to give out, -1 = unlimited stock

	//sink
	int cleanCount;			//number of clean plates
	int buttonMashFlag;		//1 = button mash to move progress bar, 0 = waiting

	//serving window
	int         dirtyCount;							//number of dirty plates
	float       returnDelay;						//seconds before a served plate comes back
	float       returningPlateTimers[MAX_PLATES];   //countdowns for plates on their way back
	int         returningPlateCount;

	int (*can_accept)(struct StationData_S* self, Entity *item);
	void (*interact)(struct StationData_S* self, Entity* player);		//on tap
	void (*use)(struct StationData_S* self, Entity* player, float dt);	//held
	void (*tick)(struct StationData_S* self, float dt);					//ran every frame with no player input
	void (*draw)(struct StationData_S* self);
}StationData;

//spawns station at given position from stations.def based on name given
Entity* station_spawn(const char* name, GFC_Vector3D position);

//checks if station has a tick function and runs if it does
int station_update(Entity* self);

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