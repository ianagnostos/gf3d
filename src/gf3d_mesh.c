#include "simple_logger.h"

#include "gf3d_vgraphics.h"
#include "gf3d_mesh.h"
#include "gf3d_commands.h"
#include "gf3d_pipeline.h"
#include "gf3d_buffers.h"
#include "gf3d_swapchain.h"
#include "gf3d_obj_load.h"

#define MESH_ATTRIBUTE_COUNT 3

typedef struct
{
    Uint32      meshCount;
    Mesh*       meshList;
    VkDevice    device;
    VkVertexInputAttributeDescription   attributeDescriptions[MESH_ATTRIBUTE_COUNT];
    VkVertexInputBindingDescription     bindingDescriptions;
}MeshManager;

static MeshManager mesh_manager = { 0 };

void gf3d_mesh_close();
void gf3d_mesh_delete(Mesh* mesh);
void gf3d_mesh_primitive_free(MeshPrimitive* prim);


void gf3d_mesh_init(Uint32 mesh_max)
{
    if (mesh_manager.meshCount != 0)
    {
        slog("Cannot init mesh system, already initialized");
        return;
    }
    if (mesh_max == 0)
    {
        slog("Cannot init mesh system for ZERO meshes");
        return;
    }

    mesh_manager.meshList = gfc_allocate_array(sizeof(Mesh), mesh_max);
    mesh_manager.device = gf3d_vgraphics_get_default_logical_device();
    if (!mesh_manager.meshList) return;
    mesh_manager.meshCount = mesh_max;
    atexit(gf3d_mesh_close);

}

void gf3d_mesh_close()
{
    int i;
    //go through list of meshes and list them all
    for (i = 0; i < mesh_manager.meshCount; i++)
    {
        gf3d_mesh_free(&mesh_manager.meshList[i]);
    }
    free(mesh_manager.meshList);
    memset(&mesh_manager, 0, sizeof(MeshManager));
}

Mesh* gf3d_mesh_get_by_filename(const char* filename)
{
    int i;
    if (!filename)return NULL;
    for (i = 0; i < mesh_manager.meshCount; i++)
    {
        if (strlen(mesh_manager.meshList[i].filename) == 0) continue;
        if (gfc_strlcmp(mesh_manager.meshList[i].filename, filename) == 0)
        {
            return &mesh_manager.meshList[i];
        }
    }
    return NULL;
}

int gf3d_mesh_primitive_buffer_create(MeshPrimitive* prim)
{
    Uint32 bufferSize = 0;
    void* data = NULL;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    if ((!prim) || (!prim->objData))return 0;

    //face buffers
    bufferSize = sizeof(Face) * prim->objData->face_count;

    gf3d_buffer_create(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

    vkMapMemory(mesh_manager.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, prim->objData->outFace, (size_t)bufferSize);
    vkUnmapMemory(mesh_manager.device, stagingBufferMemory);

    gf3d_buffer_create(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &prim->faceBuffer, &prim->faceBufferMemory);

    gf3d_buffer_copy(stagingBuffer, prim->faceBuffer, bufferSize);

    vkDestroyBuffer(mesh_manager.device, stagingBuffer, NULL);
    vkFreeMemory(mesh_manager.device, stagingBufferMemory, NULL);


    //vertex buffers
    bufferSize = sizeof(Vertex) * prim->objData->face_vert_count;

    gf3d_buffer_create(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

    vkMapMemory(mesh_manager.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, prim->objData->faceVertices, (size_t)bufferSize);
    vkUnmapMemory(mesh_manager.device, stagingBufferMemory);

    gf3d_buffer_create(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &prim->vertexBuffer, &prim->vertexBufferMemory);

    gf3d_buffer_copy(stagingBuffer, prim->vertexBuffer, bufferSize);

    vkDestroyBuffer(mesh_manager.device, stagingBuffer, NULL);
    vkFreeMemory(mesh_manager.device, stagingBufferMemory, NULL);

    prim->vertexCount = prim->objData->face_vert_count;
    prim->faceCount = prim->objData->face_count;
    return 1;
}

/**
 * @brief allocate a zero initialized mesh primitive
 * @return NULL on error or the primitive
 */
MeshPrimitive* gf3d_mesh_primitive_new()
{
    MeshPrimitive *prim;
    prim = gfc_allocate_array(sizeof(MeshPrimitive), 1);
    if (!prim) return NULL;
    return prim;
}

Mesh* gf3d_mesh_load_obj(const char* filename)  
{
    Mesh* mesh;
    mesh = gf3d_mesh_get_by_filename(filename);
    if (mesh)
    {
        mesh->_refCount++;
        return mesh;
    }

    mesh = gf3d_mesh_new();
    gfc_line_cpy(mesh->filename, filename);
    if (!mesh)
    {
        slog("failed to allocate new mesh");
        return NULL;
    }

    MeshPrimitive *prim = gf3d_mesh_primitive_new();
    if (!prim)
    {
        slog("failed to get primitive for mesh in %s", filename);
        gf3d_mesh_free(mesh);
        return NULL;
    }

    gfc_list_append(mesh->primitives, prim);
    prim->objData = gf3d_obj_load_from_file(filename);
    if (!prim->objData)
    {
        slog("Failed to parse file %s for obj data", filename);
        return NULL;
    }

    if (!gf3d_mesh_primitive_buffer_create(prim))
    {
        slog("Failed to build memory buffers for %s", filename);
        return NULL;
    }

    return mesh;

}

Mesh* gf3d_mesh_new()
{
    int i;
    for (i = 0; i < mesh_manager.meshCount; i++)
    {
        if ((mesh_manager.meshList[i]._refCount == 0)&&(strlen(mesh_manager.meshList[i].filename) == 0))
        {
            mesh_manager.meshList[i].primitives = gfc_list_new();

            if (mesh_manager.meshList[i].primitives == NULL)
            {
                slog("Cannot allocate more memory for a new mesh");
                return NULL;
            }
            mesh_manager.meshList[i]._refCount = 1;
            return &mesh_manager.meshList[i];
        }
    }

    for (i = 0; i < mesh_manager.meshCount; i++)
    {
        if (mesh_manager.meshList[i]._refCount == 0)
        {
            if (strlen(mesh_manager.meshList[i].filename) > 0)
            {
                gf3d_mesh_delete(&mesh_manager.meshList[i]);
            }
            mesh_manager.meshList[i].primitives = gfc_list_new();

            if (mesh_manager.meshList[i].primitives == NULL)
            {
                slog("Cannot allocate more meory for a new mesh");
                return NULL;
            }
            mesh_manager.meshList[i]._refCount = 1;
            return &mesh_manager.meshList[i];
        }
    }
    return NULL;
}

void gf3d_mesh_free(Mesh* mesh)
{
    if (!mesh)return;
    mesh->_refCount--;
    if (mesh->_refCount > 0) return;
    gf3d_mesh_delete(mesh);
}

void gf3d_mesh_delete(Mesh* mesh)
{
    int i, c;
    MeshPrimitive* prim;
    if (!mesh) return;
    c = gfc_list_count(mesh->primitives);
    for (i = 0; i < c; i++)
    {
        prim = gfc_list_nth(mesh->primitives, i);
        if (!prim) continue;
        gf3d_mesh_primitive_free(prim);
    }
    memset(mesh, 0, sizeof(Mesh));
}

void gf3d_mesh_primitive_free(MeshPrimitive* prim)
{
    if (!prim) return;

    if (prim->vertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mesh_manager.device, prim->vertexBuffer, NULL);
    }
    if (prim->vertexBufferMemory != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mesh_manager.device, prim->vertexBufferMemory, NULL);
    }
    if (prim->faceBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mesh_manager.device, prim->faceBuffer, NULL);
    }
    if (prim->faceBufferMemory != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(mesh_manager.device, prim->faceBufferMemory, NULL);
    }

    gf3d_obj_free(prim->objData);
}

/**
 * @brief get the input attribute descriptions for mesh based rendering
 * @param count (optional, output) the number of attributes
 * @return a pointer to a vertex input attribute description array
 */
VkVertexInputAttributeDescription* gf3d_mesh_get_attribute_descriptions(Uint32* count)
{
    if (count)*count = MESH_ATTRIBUTE_COUNT;
    mesh_manager.attributeDescriptions[0].binding = 0;
    mesh_manager.attributeDescriptions[0].location = 0;
    mesh_manager.attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    mesh_manager.attributeDescriptions[0].offset = offsetof(Vertex, vertex);

    mesh_manager.attributeDescriptions[1].binding = 0;
    mesh_manager.attributeDescriptions[1].location = 1;
    mesh_manager.attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    mesh_manager.attributeDescriptions[1].offset = offsetof(Vertex, normal);

    mesh_manager.attributeDescriptions[2].binding = 0;
    mesh_manager.attributeDescriptions[2].location = 2;
    mesh_manager.attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    mesh_manager.attributeDescriptions[2].offset = offsetof(Vertex, texel);

    return mesh_manager.attributeDescriptions;
}

/**
 * @brief get the binding description for mesh based rendering
 * @return vertex input binding descriptions compatible with mesh data
 */
VkVertexInputBindingDescription* gf3d_mesh_get_bind_description()
{
    mesh_manager.bindingDescriptions.binding = 0;
    mesh_manager.bindingDescriptions.stride = sizeof(Vertex);
    mesh_manager.bindingDescriptions.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return &mesh_manager.bindingDescriptions;
}


/**
 * @brief queue up a render for the current draw frame
 * @param mesh the mesh to render
 * @param pipe the pipeline to use
 * @param uboData the data to use to draw the mesh
 * @param texture texture data to use
 */
void gf3d_mesh_queue_render(Mesh* mesh, Pipeline* pipe, void* uboData, Texture* texture)
{
    int i, c;
    MeshPrimitive* prim;
    if (!mesh) return;
    if (!texture) return;
    if (!uboData) return;
    if (!pipe) return;
    c = gfc_list_count(mesh->primitives);
    for (i = 0; i < c; i++)
    {
        prim = gfc_list_nth(mesh->primitives, i);
        if (!prim) continue;

        gf3d_pipeline_queue_render(
            pipe,
            prim->vertexBuffer,
            prim->vertexCount,
            prim->faceBuffer,
            uboData,
            texture);
    }
}