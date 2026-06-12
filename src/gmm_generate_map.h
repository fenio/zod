#ifndef _ZGMM_GENERATE_MAP_H_
#define _ZGMM_GENERATE_MAP_H_

#include "zgui_main_menu_base.h"

//generated map size presets (tiles): index -> w,h
#define MAX_GMMGEN_SIZES 3
const int gmmgen_size_w[MAX_GMMGEN_SIZES] = { 80, 120, 160 };
const int gmmgen_size_h[MAX_GMMGEN_SIZES] = { 100, 150, 200 };
const char *const gmmgen_size_name[MAX_GMMGEN_SIZES] = { "Small", "Medium", "Large" };

#define MAX_GMMGEN_TERRAINS 5
const char *const gmmgen_terrain_name[MAX_GMMGEN_TERRAINS] =
	{ "Desert", "Volcanic", "Arctic", "Jungle", "City" };

//tech presets -> building level (controls what factories can produce)
#define MAX_GMMGEN_TECHS 3
const int gmmgen_tech_level[MAX_GMMGEN_TECHS] = { 0, 2, 5 };
const char *const gmmgen_tech_name[MAX_GMMGEN_TECHS] = { "Basic", "Advanced", "Full" };

class GMMGenerateMap : public ZGuiMainMenuBase
{
public:
	GMMGenerateMap();

	void Process();
private:
	//each setting is a button that cycles its value on click (the radio
	//widget's art was never shipped, so buttons are the reliable control)
	GMMWButton enemies_button;
	GMMWButton size_button;
	GMMWButton terrain_button;
	GMMWButton tech_button;
	GMMWButton generate_button;

	int enemies_i;   //0-2 -> 1-3 enemies
	int size_i;      //0-2
	int terrain_i;   //0-4
	int tech_i;      //0-2 -> Basic/Advanced/Full

	void SetupLayout1();
	void UpdateButtonText();
	void HandleWidgetEvent(int event_type, ZGMMWidget *event_widget);
};

#endif
