#include "simple_logger.h"

#include "gfc_types.h"


#include "gf3d_buffers.h"
#include "gf3d_swapchain.h"
#include "gf3d_vgraphics.h"
#include "gf3d_pipeline.h"
#include "gf3d_commands.h"
#include "gf3d_model.h"
#include "gf3d_mesh.h"

typedef struct
{
	Model*	modelList;
	Uint32	modelCount;
	Pipeline*	pipe;
	VkDevice device;
    Texture* defaultTexture;
} ModelManager;

static ModelManager model_manager = { 0 };

void model_close();
void model_delete(Model* model);

void model_init_system(Uint32 modelCount)
{
    gf3d_mesh_init(1024); 
    if (model_manager.modelCount != 0)
    {
        slog("Cannot init mesh system, already initialized");
        return;
    }
    if (modelCount == 0)
    {
        slog("Cannot init mesh system for ZERO meshes");
        return;
    }

    model_manager.modelList = gfc_allocate_array(sizeof(Model), modelCount);
    model_manager.device = gf3d_vgraphics_get_default_logical_device();
    if (!model_manager.modelList) return;
    model_manager.modelCount = modelCount;


    model_manager.pipe = gf3d_pipeline_create_from_config(
        gf3d_vgraphics_get_default_logical_device(),
        "config/model_pipeline.cfg",
        gf3d_vgraphics_get_view_extent(),
        modelCount,
        gf3d_mesh_get_bind_description(),
        gf3d_mesh_get_attribute_descriptions(NULL),
        3,
        sizeof(ModelUBO),
        VK_INDEX_TYPE_UINT32
    );
    if (!model_manager.pipe)
    {
        slog("failed to make pipeline for models");
        slog_sync();
        exit(-1);
    }
    model_manager.defaultTexture = gf3d_texture_load("images/default.png");
    if (!model_manager.defaultTexture)
    {
        slog("feault texture DOES not exist");
        return;
    }
    atexit(model_close);
    return;
}

void model_close()
{
    int i;
    gf3d_texture_free(model_manager.defaultTexture);
    for (i = 0; i < model_manager.modelCount;i++)
    {
        model_delete(&model_manager.modelList[i]);
    }
    free(model_manager.modelList);
    gf3d_pipeline_free(model_manager.pipe);
    memset(&model_manager, 0, sizeof(ModelManager));
}

void model_free(Model *model)
{ 
    if (!model) return;
    model->_refCount--;
}

void model_delete(Model* model)
{
    if (!model) return;
    gf3d_texture_free(model->texture);
    gf3d_mesh_free(model->mesh);
    memset(model, 0, sizeof(Model));
}

Pipeline* model_get_pipeline()
{
    return model_manager.pipe;
}

Model* model_get_by_filename(const char* filename)
{
    if (!filename) return NULL;
    for (int i = 0; i < model_manager.modelCount;i++)
    {
        if (model_manager.modelList[i]._refCount == 0) continue;
        if (gfc_strlcmp(model_manager.modelList[i].filename, filename) == 0)
        {
            return  &model_manager.modelList[i];
        }
    }
    return NULL;
}

Model* model_new()
{
    int i;
    for (i = 0; i < model_manager.modelCount; i++)
    {
        if ((model_manager.modelList[i]._refCount == 0) && (strlen(model_manager.modelList[i].filename) == 0))
        {
            model_manager.modelList[i]._refCount = 1;
            return &model_manager.modelList[i];
        }
    }

    for (i = 0; i < model_manager.modelCount; i++)
    {
        if (model_manager.modelList[i]._refCount == 0)
        {
            if (strlen(model_manager.modelList[i].filename) > 0)
            {
                model_delete(&model_manager.modelList[i]);
            }
            model_manager.modelList[i]._refCount = 1;
            return &model_manager.modelList[i];
        }
    }
    return NULL;
}

Model *model_load(const char *filename)
{
    Model* model;
    const char* str = NULL;
    const char* str2 = NULL;
    Mesh* mesh;
    Texture* texture;
    SJson* json;
    SJson* data;
    if (!filename) return NULL;

    /*
    model = model_get_by_filename(filename);
    if (!model)
    {
        slog("model found ");
        model->_refCount++;
        return model;
    }
    */

    json = sj_load(filename);
    if (!json)
    {
        slog("failed to load model %s", filename);
        return NULL;
    }

    data = sj_object_get_value(json, "model");
    if (!data)
    {
        slog("failed to get file info from %s", filename);
        sj_free(json);
        return NULL;
    }

    str = sj_object_get_string(data, "obj");
    if (!str)
    {
        slog("failed ot find obj data in %s", filename);
        sj_free(json);
        return NULL;
    }

    mesh = gf3d_mesh_load_obj(str);
    if (!mesh)
    {
        slog("no mesh loaded in model.c");
        return NULL;
    }
    str2 = sj_object_get_string(data, "texture");
    if (str2)
    {
        texture = gf3d_texture_load(str2);
        if (!texture)
        {
            slog("failed to load texture %s, using default", str2);
            texture = model_manager.defaultTexture;
        }
    }
    else texture = model_manager.defaultTexture;
    model = model_new();
    if (!model)
    {
        slog("failed to get empty sapce in mem for model %s", filename);
        sj_free(json);
        return NULL;
    }
    model->mesh = mesh;
    model->texture = texture;
    gfc_line_cpy(model->filename, filename);
    sj_free(json);
    //gf3d_texture_free(texture);
    //gf3d_mesh_free(mesh);
    return model;
}

/**
 * @brief given a model matrix and basic color, build the meshUBO needed to render a model
 * @param modelMat the model Matrix
 * @param colorMod the color for the UBO
 */
ModelUBO gf3d_model_get_ubo(
    GFC_Matrix4 modelMat,
    GFC_Color colorMod)
{
    ModelUBO ubo = { 0 };
    GFC_Matrix4 view;
    GFC_Matrix4 proj;
    memcpy(ubo.model, modelMat, sizeof(GFC_Matrix4));

    gf3d_vgraphics_get_view(&view);
    memcpy(ubo.view, view, sizeof(GFC_Matrix4));

    gf3d_vgraphics_get_projection_matrix(&proj);
    memcpy(ubo.proj, proj, sizeof(GFC_Matrix4));

    ubo.color = gfc_color_to_vector4f(colorMod);

    /*
    gfc_matrix4_slog(ubo.model);
    slog("/n");
    gfc_matrix4_slog(ubo.view);
    slog("/n");
    gfc_matrix4_slog(ubo.proj);
    slog("/n");
    */
    return ubo;
}

void gf3d_model_queue_render(Model* model, GFC_Matrix4 mat, GFC_Color colorMod)
{
    ModelUBO ubo;
    if (!model)
    {
        slog("model is NULL");
        return;
    }
    ubo = gf3d_model_get_ubo(mat, colorMod);
    gf3d_mesh_queue_render(model->mesh, model_manager.pipe, &ubo, model->texture);
}
