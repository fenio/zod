#include "gmm_options.h"
#include "zprefs.h"

//defined in zobject.cpp
extern bool zod_render_smoothing;

GMMOptions::GMMOptions() : ZGuiMainMenuBase()
{
	menu_type = GMM_OPTIONS;
	title = "Options";
	w = 112;
	h = 118;

	SetupLayout1();
}

void GMMOptions::SetupLayout1()
{
	int next_y;

	next_y = GMM_TITLE_MARGIN;

	//volume and game speed: click cycles to the next setting; the button text
	//shows the current value (refreshed each frame in Process from live state)
	volume_button.SetType(MMGENERIC_BUTTON);
	volume_button.SetText("Volume:");
	volume_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	volume_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&volume_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	speed_button.SetType(MMGENERIC_BUTTON);
	speed_button.SetText("Game Speed:");
	speed_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	speed_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&speed_button);
	next_y += GMMWBUTTON_HEIGHT + 7;

	//player preference toggles (persisted to ~/.zod_prefs)
	mouse_button.SetType(MMGENERIC_BUTTON);
	mouse_button.SetText("Mouse: Classic");
	mouse_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	mouse_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&mouse_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	smooth_button.SetType(MMGENERIC_BUTTON);
	smooth_button.SetText("Smoothing: On");
	smooth_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	smooth_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&smooth_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	pause_button.SetType(MMGENERIC_BUTTON);
	pause_button.SetText("Pause Game");
	pause_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	pause_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&pause_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	reshuffle_button.SetType(MMGENERIC_BUTTON);
	reshuffle_button.SetText("Reshuffle Teams");
	reshuffle_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	reshuffle_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&reshuffle_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	reset_button.SetType(MMGENERIC_BUTTON);
	reset_button.SetText("Reset Map");
	reset_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	reset_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&reset_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	h = next_y + GMM_BOTTOM_MARGIN;

	//needed if w is ever changed / set
	UpdateDimensions();
}

void GMMOptions::Process()
{
	SetVolumeStatus();

	SetTimeStatuses();

	mouse_button.SetText(zod_classic_mouse ? "Mouse: Classic" : "Mouse: Modern");
	mouse_button.SetGreen(zod_classic_mouse);
	smooth_button.SetText(zod_render_smoothing ? "Smoothing: On" : "Smoothing: Off");
	smooth_button.SetGreen(zod_render_smoothing);

	ProcessWidgets();
}

void GMMOptions::SetTimeStatuses()
{
	if(!ztime) return;

	pause_button.SetGreen(ztime->IsPaused());

	char speed_str[64];
	sprintf(speed_str, "Game Speed: %.0lf%%", 100 * ztime->GameSpeed());
	speed_button.SetText(speed_str);
}

void GMMOptions::SetVolumeStatus()
{
	int si;

	if(!sound_setting) return;

	si = *sound_setting;

	if(si < 0) return;
	if(si >= MAX_SOUND_SETTINGS) return;

	volume_button.SetText("Volume: " + sound_setting_string[si]);
}

void GMMOptions::HandleWidgetEvent(int event_type, ZGMMWidget *event_widget)
{
	int w_ref_id;

	if(!event_widget) return;

	w_ref_id = event_widget->GetRefID();

	switch(event_type)
	{
	case GMM_UNCLICK_EVENT:
		if(w_ref_id == reshuffle_button.GetRefID())
		{
			gmm_flags.reshuffle_teams = true;
		}
		else if(w_ref_id == reset_button.GetRefID())
		{
			//gmm_flags.reset_map = true;
			gmm_flags.open_main_menu = true;
			gmm_flags.open_main_menu_type = GMM_WARNING;
			gmm_flags.warning_flags.text1 = "Are you sure you want";
			gmm_flags.warning_flags.text2 = "to reset the map?";
			gmm_flags.warning_flags.reset_map = true;
		}
		else if(w_ref_id == pause_button.GetRefID())
		{
			gmm_flags.pause_game = true;
		}
		else if(w_ref_id == mouse_button.GetRefID())
		{
			zod_classic_mouse = !zod_classic_mouse;
			ZPrefs_Save();
		}
		else if(w_ref_id == smooth_button.GetRefID())
		{
			zod_render_smoothing = !zod_render_smoothing;
			ZPrefs_Save();
		}
		else if(w_ref_id == volume_button.GetRefID())
		{
			//cycle to the next volume setting (wraps round)
			int si = sound_setting ? *sound_setting : 0;
			si = (si + 1) % MAX_SOUND_SETTINGS;
			gmm_flags.set_volume = true;
			gmm_flags.set_volume_value = si;
		}
		else if(w_ref_id == speed_button.GetRefID())
		{
			//find the current speed, advance to the next (wraps round)
			int cur = MAX_GMMOPTIONS_SPEED_SETTINGS - 1;
			for(int i=0;i<MAX_GMMOPTIONS_SPEED_SETTINGS;i++)
				if(ztime && ztime->GameSpeed() <= gmmoption_speed_setting_value[i] + 0.01)
					{ cur = i; break; }
			int next = (cur + 1) % MAX_GMMOPTIONS_SPEED_SETTINGS;
			gmm_flags.set_game_speed = true;
			gmm_flags.set_game_speed_value = gmmoption_speed_setting_value[next];
		}
		break;
	}
}
