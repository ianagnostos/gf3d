#ifndef __ORDER_H_
#define __ORDER_H_

#include "gf3d_entity.h"

//loads recipes from a def file and resets orders and score.
//CALL AFTER ITEMS.DEF LOADED
int orders_init(const char* recipeFile);

//counts down orders, expires if time runs out, and spawn new ones if enough time passed
void order_update(float dt);

//checks plate against current orders, returns 1 if valid (order closed and player given score), 0 if invalid
int order_serve(Entity* plateEnt);  

//draws score HUD and orders HUD
void order_draw();

//returns players score
int order_get_score();

#endif

