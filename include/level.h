#ifndef __LEVEL_H_
#define __LEVEL_H_

#include "gfc_audio.h"
#include "gf3d_model.h"

typedef struct
{
	Model* terrain;
	GFC_Matrix4 terrainMatrix;
	Model* sky;
	GFC_TextLine skyFile;
	GFC_TextLine terrainFile;
	Mix_Music* backgroundMusic;
	GFC_TextLine musicFile;
	GFC_Box bounds;
	SJson* json;
} Level;

Level* level_new();

Level* level_load(const char* filename);

void level_free(Level* level);

void level_draw(Level* level);

Uint8 level_line_test(Level* level, GFC_Edge3D edge, GFC_Vector3D* contact);

Level* level_get_the();


/*
*@breif check if the following boundsing box is clipping or outside the level bounds
*/
Uint8 level_bounds_check(Level* level, GFC_Box aabb);

#endif