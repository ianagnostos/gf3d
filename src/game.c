#include <SDL.h>            

#include "simple_json.h"
#include "simple_logger.h"

#include "gfc_input.h"
#include "gfc_config_def.h"
#include "gfc_vector.h"
#include "gfc_matrix.h"
#include "gfc_audio.h"
#include "gfc_string.h"
#include "gfc_actions.h"

#include "gf2d_sprite.h"
#include "gf2d_font.h"
#include "gf2d_actor.h"
#include "gf2d_mouse.h"

#include "gf3d_vgraphics.h"
#include "gf3d_pipeline.h"
#include "gf3d_swapchain.h"
#include "gf3d_model.h"

#include "gf3d_entity.h"
#include "player.h"
#include "level.h"
#include "item.h"
#include "station.h"
#include "fridge.h"
#include "draw_3d.h"


extern int __DEBUG;
extern float dt;

static int _done = 0;
static Uint32 frame_delay = 33;
static float fps = 0;
float dt = 0.033f;

void parse_arguments(int argc,char *argv[]);
void game_frame_delay();

void exitGame()
{
    _done = 1;
}


int main(int argc,char *argv[])
{
    //local 
    //Sprite* bg;

    //Model* model; 
    Level* level;
    //GFC_Matrix4 mat; 
    //GFC_Matrix4 *view; 
    //initializtion    
    parse_arguments(argc,argv);
    init_logger("gf3d.log",0);
    slog("gf3d begin");
    //gfc init
    gfc_input_init("config/input.cfg");
    gfc_config_def_init();
    gfc_action_init(1024);
    //gf3d init
    gf3d_vgraphics_init("config/setup.cfg");
    gfc_sound_init_config("config/audio.cfg");
    gf2d_font_init("config/font.cfg");
    gf2d_actor_init(1000);
    draw_3d_init();
    entity_init(1000);

    gfc_config_def_load("defs/items.def");
    gfc_config_def_load("defs/stations.def");
    
    //game init
    srand(SDL_GetTicks());

    Uint64 NOW = SDL_GetPerformanceCounter();
    Uint64 LAST = 0;

    slog_sync();
    //bg = gf2d_sprite_load_image("images/bg_flat.png");
    gf2d_mouse_load("actors/mouse.actor");
    level = level_load("levels/level.json");

    player_spawn(gfc_vector3d(-20, 20, 4), 1);
    player_spawn(gfc_vector3d(-10, 10, 4), 2);

    station_spawn("tomato_crate", gfc_vector3d(-40, -30, 3));
    station_spawn("lettuce_crate", gfc_vector3d(-20, -30, 3));
    station_spawn("counter", gfc_vector3d(0, -30, 3));
    station_spawn("counter", gfc_vector3d(20, -30, 3));
    station_spawn("chopping_board", gfc_vector3d(40, -30, 3));

    // main game loop    
    while(!_done)
    {
        LAST = NOW;
        NOW = SDL_GetPerformanceCounter();

        dt = (float)(NOW - LAST) / (float)SDL_GetPerformanceFrequency();
        if (dt > 0.05f) dt = 0.05f;


        gfc_input_update();
        gf2d_mouse_update();
        gf2d_font_update();
        //camera updates

        station_clear_highlights();
        entity_system_update();
        entity_system_think();
        
        gf3d_vgraphics_render_start();
                //3D draws
                slog_sync();
                level_draw(level);
                entity_system_draw();
                //2D draws
                //gf2d_sprite_draw_image(bg,gfc_vector2d(0,0));
                gf2d_font_draw_line_tag("ALT+F4 to exit", FT_H1, GFC_COLOR_WHITE, gfc_vector2d(10, 10));
                //slog("Time: %f", deltaTime);
                gf2d_mouse_draw();
        gf3d_vgraphics_render_end();
        if (gfc_input_command_down("exit"))_done = 1; // exit condition
        game_frame_delay();
    }    
    vkDeviceWaitIdle(gf3d_vgraphics_get_default_logical_device());    
    //cleanup
    slog("gf3d program end");
    exit(0);
    slog_sync();
    return 0;
}

void parse_arguments(int argc,char *argv[])
{
    int a;

    for (a = 1; a < argc;a++)
    {
        if (strcmp(argv[a],"--debug") == 0)
        {
            __DEBUG = 1;
        }
    }    
}

void game_frame_delay()
{
    Uint32 diff;
    static Uint32 now;
    static Uint32 then;
    then = now;
    slog_sync();// make sure logs get written when we have time to write it
    now = SDL_GetTicks();
    diff = (now - then);
    if (diff < frame_delay)
    {
        SDL_Delay(frame_delay - diff);
    }
    fps = 1000.0/MAX(SDL_GetTicks() - then,0.001);
    //slog("fps: %f",fps);
}
/*eol@eof*/
