#ifndef _ZGMM_OPTIONS_H_
#define _ZGMM_OPTIONS_H_

#include "zgui_main_menu_base.h"

#define MAX_GMMOPTIONS_SPEED_SETTINGS 7

const double gmmoption_speed_setting_value[MAX_GMMOPTIONS_SPEED_SETTINGS] =
{
	0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 4.0
};

class GMMOptions : public ZGuiMainMenuBase
{
public:
	GMMOptions();

	void Process();
private:
	// volume and game speed use cycling BUTTONS, not radios: the radio widget's
	// art was never shipped, so the menu had no working volume/speed control
	// (issue #41); buttons are the reliable control this fork ships
	GMMWButton volume_button;
	GMMWButton speed_button;
	GMMWButton mouse_button;
	GMMWButton smooth_button;
	GMMWButton difficulty_button;	//#difficulty: cycles the bot AI difficulty
	GMMWButton max_units_button;	//#75: max units per team (takes effect next game)
	GMMWButton reshuffle_button;
	GMMWButton reset_button;
	GMMWButton pause_button;

	void SetupLayout1();

	void HandleWidgetEvent(int event_type, ZGMMWidget *event_widget);

	void SetVolumeStatus();
	void SetTimeStatuses();
};

#endif
