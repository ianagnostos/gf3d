#include "simple_logger.h"
#include "draw_3d.h"

typedef struct
{
	Model* cube;
	Model* sphere;
}Draw3DData;

Draw3DData Data3DManager = { 0 };

void draw_3d_close()
{
	model_free(Data3DManager.cube);
	model_free(Data3DManager.sphere);
}

void draw_3d_init()
{
	Data3DManager.cube = model_load("models/primitives/cube.model");
	Data3DManager.sphere = model_load("models/primitives/sphere.model");
	atexit(draw_3d_close);
}

void draw_3d_cube(GFC_Box cube)
{
	GFC_Matrix4 mat;

	gfc_matrix4_from_vectors(
		mat,
		gfc_vector3d(cube.x + (cube.w * 0.5), cube.y + (cube.h * 0.5), cube.z + (cube.d * 0.5)),
		gfc_vector3d(0, 0, 0),
		gfc_vector3d(cube.w, cube.h, cube.d));

	gf3d_high_queue_render(Data3DManager.cube, mat, GFC_COLOR_GREEN);
}



