#include "simple_logger.h"

#include "gf3d_obj_load.h"
#include "gfc_primitives.h"

#include "gf3d_model.h"
#include "level.h"

static Level* theLevel = NULL;

/*
Uint8 level_line_test(Level* level, GFC_Edge3D edge, GFC_Vector3D *contact)
{
	MeshPrimitive* prim;
	GFC_Triangle3D t;
	int i, c, j;
	GFC_Vector3D out = { -1 };
	if ((!level) || (!level->terrain) || (!level->terrain->mesh)) 
	{
		slog("no level data");
		return 0;
	}
	c = gfc_list_count(level->terrain->mesh->primitives);
	for (i = 0; i < c; i++)
	{
		prim = gfc_list_mth(level->terrain->mesh->primitves, i);
		if (!prim)
		{
			slog("prim is NULL");
			continue;
		}
		if (!prim->objData)
		{
			slog("prim has no objData");
			continue;
		}
		slog("terrain has %i faces", prim->objData->face_count);
		for (j = 0; j < prim->objData->face_Count; j++)
		{
			t.a = prim->objData-faceVertices[prim->objData->outFace->verts[0]].vertex;
			t.b = prim->objData - faceVertices[prim->objData->outFace->verts[1]].vertex;
			t.c = prim->objData - faceVertices[prim->objData->outFace->verts[2]].vertex;
			if (gfc_triangle_edge_text(edge, t, &out))
			{
				if (contact) *contact = out;
				return 1;
			}
		}
	}
	return 0;
}
*/


Level* level_new()
{
	slog("making new level");
	Level* level;
	level = gfc_allocate_array(sizeof(Level), 1);
	if (!level)
	{
		slog("no level allocated");
		return NULL;
	}

	gfc_matrix4_identity(level->terrainMatrix);
	return level;
}

Level* level_load(const char* filename)
{
	SJson* json;
	SJson* data;
	const char* str;
	Level* level;

	slog("trying to load level");

	json = sj_load(filename);
	if (!json)
	{
		slog("unable to load json for %s", filename);
		return NULL;
	}


	slog("loading1");

	data = sj_object_get_value(json, "level");
	if (!data)
	{
		sj_free(json);
		slog("could not open json for level");
		return NULL;
	}

	slog("loading2");

	level = level_new();
	if (!level)
	{
		slog("no level !!");
		return NULL;
	}

	slog("loading3");

	str = sj_object_get_string(data, "sky");
	if (str)
	{
		level->sky = model_load(str);
		if (level->sky)
			gfc_line_cpy(level->skyFile, str);
		else
		{
			slog("no sky!");
		}
	}

	slog("loading4");

	str = sj_object_get_string(data, "terrain");
	if (str)
	{
		level->terrain = model_load(str);
		if (level->terrain)
			gfc_line_cpy(level->terrainFile, str);
		else
		{
			slog("no terrain!");
		}
	}

	slog("loading5");

	level->bounds = gfc_box_from_config(sj_object_get_value(data, "bounds"));
	/*
	str = sj_object_get_string(data, "music");
	if (str)
	{
		level->backgroundMusic = gfc_sound_load_music(str);
		if (level->backgroundMusic)
		{
			gfc_line_cpy(level->musicFile, str);
			Mix_PlayMusic(level->backgroundMusic, -1);
		}
	}
	*/

	slog("loading6");
	level->json = json;
	theLevel = level;

	slog("end of level");

	return level;


}

Level* level_get_the()
{
	return theLevel;
}

void level_free(Level* level)
{
	if (!level) return;
	model_free(level->sky);
	model_free(level->terrain);
	Mix_HaltMusic();
	Mix_FreeMusic(level->backgroundMusic);
	sj_free(level->json);
}

void level_draw(Level* level)
{
	if ((!level) || (!level->terrain))
	{
		slog("no terrain!");
		return;
	}

	gfc_matrix4_from_vectors(
		level->terrainMatrix,
		gfc_vector3d(0,0,0),
		gfc_vector3d(0, 0, 0),
		gfc_vector3d(1, 1, 1));
	gf3d_model_queue_render(level->terrain, level->terrainMatrix, GFC_COLOR_WHITE);
}