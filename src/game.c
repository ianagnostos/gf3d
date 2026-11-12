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

#include "order.h"


extern int __DEBUG;

static int _done = 0;
static Uint32 frame_delay = 33;
static float fps = 0;

float dt = 0.033f;

void parse_arguments(int argc,char *argv[]);
void game_frame_delay();
void spawn_everything();

void exitGame()
{
    _done = 1;
}


int main(int argc,char *argv[])
{

    Level* level;
    Uint64 NOW;
    Uint64 LAST = 0;
  
    //initializtion    
    parse_arguments(argc,argv);
    init_logger("gf3d.log",0);
    slog("gf3d begin");

    gfc_config_def_init();
    gfc_action_init(1024);

    //gf3d init
    gf3d_vgraphics_init("config/setup.cfg");
    gfc_sound_init_config("config/audio.cfg");
    gfc_input_init("config/input.cfg");
    gf2d_font_init("config/font.cfg");
    gf2d_actor_init(1000);
    draw_3d_init();
    entity_init(1000);

    //game defs
    gfc_config_def_load("defs/items.def");
    slog("item defs loaded");
    gfc_config_def_load("defs/stations.def");
    slog("station defs loaded");
    
    if (!orders_init("defs/recipes.def"))
    {
        slog("recipe defs failed to load");
        slog_sync();
        return 1;
    }
    else
        slog("recipe defs loaded");

    //game init
    srand(SDL_GetTicks());
    gf2d_mouse_load("actors/mouse.actor");
    level = level_load("levels/level.json");
    slog_sync();

    NOW = SDL_GetPerformanceCounter();

    spawn_everything();

    // main game loop    
    while(!_done)
    {
        LAST = NOW;
        NOW = SDL_GetPerformanceCounter();

        dt = (float)(NOW - LAST) / (float)SDL_GetPerformanceFrequency();
        if (dt > 0.05f) dt = 0.05f; //avoid hitches

        SDL_JoystickUpdate();
        gfc_input_update();
        gf2d_mouse_update();
        gf2d_font_update();

        station_clear_highlights();
        entity_system_update();
        entity_system_think();
        order_update(dt);
        
        gf3d_vgraphics_render_start();
                //3D draws
                slog_sync();
                level_draw(level);
                entity_system_draw();

                //2D draws
                gf2d_font_draw_line_tag("ALT+F4 to exit", FT_H1, GFC_COLOR_WHITE, gfc_vector2d(10, 10));
                order_draw();
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
}

static void spawn_everything()
{
    station_spawn("counter", gfc_vector3d(45, -35, 3));
    station_spawn("stove", gfc_vector3d(35, -35, 3));
    station_spawn("stove", gfc_vector3d(25, -35, 3));
    station_spawn("counter", gfc_vector3d(15, -35, 3));
    station_spawn("counter", gfc_vector3d(5, -35, 3));
    station_spawn("counter", gfc_vector3d(-5, -35, 3));
    station_spawn("counter", gfc_vector3d(-15, -35, 3));
    station_spawn("fryer", gfc_vector3d(-25, -35, 3));
    station_spawn("fryer", gfc_vector3d(-35, -35, 3));
    station_spawn("counter", gfc_vector3d(-45, -35, 3));

    station_spawn("patty_crate", gfc_vector3d(55, -15, 3));
    station_spawn("counter", gfc_vector3d(55, -5, 3));
    station_spawn("bun_crate", gfc_vector3d(55, 5, 3));
    station_spawn("counter", gfc_vector3d(55, 15, 3));
    station_spawn("plate_stack", gfc_vector3d(55, 25, 3));

    station_spawn("potato_crate", gfc_vector3d(-55, -15, 3));
    station_spawn("counter", gfc_vector3d(-55, -5, 3));
    station_spawn("tomato_crate", gfc_vector3d(-55, 5, 3));
    station_spawn("counter", gfc_vector3d(-55, 15, 3));
    station_spawn("lettuce_crate", gfc_vector3d(-55, 25, 3));

    station_spawn("counter", gfc_vector3d(25, -5, 3));
    station_spawn("counter", gfc_vector3d(15, -5, 3));
    station_spawn("sink", gfc_vector3d(0, -5, 3));
    station_spawn("counter", gfc_vector3d(-15, -5, 3));
    station_spawn("counter", gfc_vector3d(-25, -5, 3));

    for (int x = -25; x <= 25; x += 10)
        station_spawn("counter", gfc_vector3d(x, 5, 3));

    station_spawn("counter", gfc_vector3d(25, 15, 3));
    station_spawn("chopping_board", gfc_vector3d(15, 15, 3));
    station_spawn("counter", gfc_vector3d(5, 15, 3));
    station_spawn("counter", gfc_vector3d(-5, 15, 3));
    station_spawn("chopping_board", gfc_vector3d(-15, 15, 3));
    station_spawn("counter", gfc_vector3d(-25, 15, 3));

    station_spawn("trash_can", gfc_vector3d(65, 45, 3));
    station_spawn("trash_can", gfc_vector3d(-65, 45, 3));
    station_spawn("serving_window", gfc_vector3d(0, 55, 4));

    player_spawn(gfc_vector3d(15, 35, 4), 1);
    player_spawn(gfc_vector3d(-15, 35, 4), 2);
}
/*eol@eof*/
