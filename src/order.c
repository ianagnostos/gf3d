#include "simple_logger.h"
#include "simple_json.h"
#include "gf2d_font.h"
#include "gfc_config.h"
#include "item.h"
#include "order.h"

#define MAX_RECIPES   8				
#define MAX_ORDERS    4				
#define SPAWN_FIRST_ORDER   3.0f	//seconds until first order after start of game
#define SPAWN_ORDER_EVERY   12.0f	//seconds interval beteen spawning new orders
#define ORDER_EXPIRE_PENALTY 10		//points lost when order time runs out

#define HUD_X                10
#define HUD_SCORE_Y          40
#define HUD_ORDERS_Y         70
#define HUD_LINE_HEIGHT      30

typedef struct
{
	GFC_TextLine name;		
	float points;			
	float patience;			//how long customer waits
	int   count;			//how many ingredients
	struct 
	{ 
		GFC_TextLine ing;
		ItemState state; 
	} ingredients[MAX_INGREDIENTS];
} Recipe;

typedef struct
{
	int   active;			//flag for if order is active
	int   recipe;			//index into recipes[]
	float timeLeft;			//number of seconds before order expires
} Order;

static Recipe recipes[MAX_RECIPES];				//"menu" loads values from def and store in here, doesnt change
static int    recipeCount = 0;					//amount of valid recipes, used when traversing recipes[]
static Order  orders[MAX_ORDERS];				//stores orders when spawned, changes every frame
static float  spawnTimer = SPAWN_FIRST_ORDER;
static int    score = 0;

//private helpers
static ItemState string_to_state(const char* s);						//convert def file value to ENUM
static int  order_abort(SJson* json);									//called after failed load
static int  recipe_matches_plate(Recipe* recipe, ItemData* plate);		//checks to see if recipe matches plate data
static void order_spawn();												// if inactive order slot -> sets order to active, chooses random recipe, and starts timer

int orders_init(const char* recipeDef)
{
	SJson* json;
	SJson* list_of_recipes;
	SJson* recipe_json;
	SJson* ingredents_json;
	SJson* ing_json;
	Recipe* recipe;
	GFC_TextLine text;
	int i, j, numRecipes, numIngredients;
	float points, patience;

	json = sj_load(recipeDef);
	if (!json)
	{
		slog("failed to load recipes from %s", recipeDef);
		return 0;
	}

	list_of_recipes = sj_object_get_value(json, "recipes");
	if (!list_of_recipes)
	{
		slog("recipes: %s has no recipes array", recipeDef);
		return order_abort(json);
	}
	numRecipes = sj_array_get_count(list_of_recipes);

	for (i = 0; i < numRecipes; i++)
	{
		recipe = &recipes[i];
		recipe_json = sj_array_get_nth(list_of_recipes, i);
		if (!recipe_json)
		{
			slog("recipes: entry %i is not valid json", i);
			return order_abort(json);
		}

		if (!sj_object_line_value(recipe_json, "name", recipe->name))
		{
			slog("recipes: entry %i has no name", i);
			return order_abort(json);
		}

		points = 0;
		sj_object_get_value_as_float(recipe_json, "points", &points);
		recipe->points = points;

		patience = 0;
		sj_object_get_value_as_float(recipe_json, "patience", &patience);
		recipe->patience = patience;

		ingredents_json = sj_object_get_value(recipe_json, "ingredients");
		if (!ingredents_json)
		{
			slog("recipes: '%s' has no ingredients array", recipe->name);
			return order_abort(json);
		}
		numIngredients = sj_array_get_count(ingredents_json);

		for (j = 0; j < numIngredients; j++)
		{
			ing_json = sj_array_get_nth(ingredents_json, j);
			if (!ing_json)
			{
				slog("recipes: '%s' ingredient %i is not valid json", recipe->name, j);
				return order_abort(json);
			}

			if (!sj_object_line_value(ing_json, "item", recipe->ingredients[j].ing))
			{
				slog("recipes: '%s' ingredient %i has no item", recipe->name, j);
				return order_abort(json);
			}
			
			if (!sj_object_line_value(ing_json, "state", text))
			{
				slog("recipes: '%s' ingredient %i has no state", recipe->name, j);
				return order_abort(json);
			}
			recipe->ingredients[j].state = string_to_state(text);
		}
		recipe->count = numIngredients;
	}
	recipeCount = numRecipes;
	sj_free(json);
	slog("loaded %i recipes", recipeCount);
	return 1;
}

void order_update(float dt)
{
	spawnTimer -= dt;
	if (spawnTimer <= 0)
	{
		order_spawn();
		spawnTimer = SPAWN_ORDER_EVERY;
	}

	for (int i = 0; i < MAX_ORDERS; i++)
	{
		if (!orders[i].active) continue;
		orders[i].timeLeft -= dt;
		if (orders[i].timeLeft <= 0)
		{
			slog("order expired: %s", recipes[orders[i].recipe].name);
			orders[i].active = 0;
			score -= ORDER_EXPIRE_PENALTY;
			if (score < 0) score = 0; //no negative scores
		}
	}
}

int order_serve(Entity* plateEnt)
{
	ItemData* plate = item_get_data(plateEnt);
	Recipe* recipe;
	int most_urgent_index = -1;
	int points;

	//dont waste resources searching if there is nothing to search with
	if ((!plate) || (!plate->isPlate) || (plate->ingredientCount == 0)) return 0;

	//find order closest to expiring
	for (int i = 0; i < MAX_ORDERS; i++)
	{
		if (!orders[i].active) continue;
		if (!recipe_matches_plate(&recipes[orders[i].recipe], plate)) continue;
		if ((most_urgent_index < 0) || (orders[i].timeLeft < orders[most_urgent_index].timeLeft))
			most_urgent_index = i;
	}
	if (most_urgent_index < 0) return 0; //if index is still -1 that means nothing was found

	recipe = &recipes[orders[most_urgent_index].recipe];
	points = recipe->points + (recipe->points * (orders[most_urgent_index].timeLeft / recipe->patience));
	score += points;
	slog("served %s: +%i", recipe->name, points);

	orders[most_urgent_index].active = 0;
	return 1;
}

void order_draw()
{
	GFC_TextLine text;
	int i, line = 0;

	//score HUD
	gfc_line_sprintf(text, "Score: %i", score);
	gf2d_font_draw_line_tag(text, FT_H1, GFC_COLOR_WHITE, gfc_vector2d(HUD_X, HUD_SCORE_Y));


	//orders HUD
	for (i = 0; i < MAX_ORDERS; i++)
	{
		if (!orders[i].active) continue;
		gfc_line_sprintf(text, "%s  %.0fs", recipes[orders[i].recipe].name, orders[i].timeLeft);
		gf2d_font_draw_line_tag(text, FT_H1, GFC_COLOR_WHITE, gfc_vector2d(HUD_X, HUD_ORDERS_Y + HUD_LINE_HEIGHT * line));
		line++;
	}
}

int order_get_score() { return score; }

static int recipe_matches_plate(Recipe* recipe, ItemData* plate)
{
	int used[MAX_INGREDIENTS] = { 0 };
	int i, j, found;

	if (recipe->count != plate->ingredientCount) return 0;
	for (i = 0; i < recipe->count; i++)
	{
		found = 0;
		for (j = 0; j < plate->ingredientCount; j++)
		{
			if (used[j]) continue;
			if ((plate->ingredientName[j][0] != '\0')
				&& (plate->ingredientState[j] == recipe->ingredients[i].state)
				&& (strcmp(plate->ingredientName[j], recipe->ingredients[i].ing) == 0))
			{
				used[j] = 1;   //each plate item is one ingredient, no double dipping            
				found = 1;
				break;
			}
		}
		if (!found) return 0;
	}
	return 1;
}

static ItemState string_to_state(const char* str)
{
	if (!str) return ITEM_RAW;
	if (strcmp(str, "chopped") == 0) return ITEM_CHOPPED;
	if (strcmp(str, "cooked") == 0)  return ITEM_COOKED;
	if (strcmp(str, "burnt") == 0)   return ITEM_BURNT;
	return ITEM_RAW;
}

static int order_abort(SJson* json)
{
	sj_free(json);
	memset(recipes, 0, sizeof(recipes));
	return 0;
}

static void order_spawn()
{
	for (int i = 0; i < MAX_ORDERS; i++)
	{
		if (orders[i].active) continue;
		orders[i].active = 1;
		orders[i].recipe = gfc_random_int(recipeCount);
		orders[i].timeLeft = recipes[orders[i].recipe].patience;
		return;
	}
}




