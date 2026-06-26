#include "gmm_options.h"
#include "zprefs.h"
#include "constants.h"	//#75: DEFAULT_MAX_UNITS_PER_TEAM + UNIT_LIMIT bounds

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

	//#difficulty: cycles the bot AI difficulty (persisted, read live by the bot)
	difficulty_button.SetType(MMGENERIC_BUTTON);
	difficulty_button.SetText("Difficulty: Normal");
	difficulty_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	difficulty_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&difficulty_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	max_units_button.SetType(MMGENERIC_BUTTON);	//#75
	max_units_button.SetText("Max Units: 100");
	max_units_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	max_units_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&max_units_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	menu_size_button.SetType(MMGENERIC_BUTTON);	//#196
	menu_size_button.SetText("Menu Size: 2x");
	menu_size_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	menu_size_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&menu_size_button);
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

	{
		int d = (zod_bot_difficulty >= 0 && zod_bot_difficulty < MAX_BOT_DIFFICULTY) ? zod_bot_difficulty : BOT_DIFF_NORMAL;
		difficulty_button.SetText(string("Difficulty: ") + bot_difficulty_name[d]);
		difficulty_button.SetGreen(d != BOT_DIFF_NORMAL);
	}

	{
		char buf[32];
		snprintf(buf, sizeof(buf), "Max Units: %d", zod_max_units_per_team);	//#75
		max_units_button.SetText(buf);
		max_units_button.SetGreen(zod_max_units_per_team != DEFAULT_MAX_UNITS_PER_TEAM);
	}

	{
		char buf[32];
		snprintf(buf, sizeof(buf), "Menu Size: %gx", zod_menu_scale);	//#196
		menu_size_button.SetText(buf);
		menu_size_button.SetGreen(zod_menu_scale != 1.0);
	}

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
		else if(w_ref_id == difficulty_button.GetRefID())
		{
			//#difficulty: cycle Easy->Normal->Hard->Expert; the bot reads it live
			zod_bot_difficulty = (zod_bot_difficulty + 1) % MAX_BOT_DIFFICULTY;
			ZPrefs_Save();
		}
		else if(w_ref_id == max_units_button.GetRefID())
		{
			//#75: cycle the max units per team (wraps at the top); takes effect next game
			zod_max_units_per_team += UNIT_LIMIT_STEP;
			if(zod_max_units_per_team > UNIT_LIMIT_MAX) zod_max_units_per_team = UNIT_LIMIT_MIN;
			ZPrefs_Save();
		}
		else if(w_ref_id == smooth_button.GetRefID())
		{
			zod_render_smoothing = !zod_render_smoothing;
			ZPrefs_Save();
		}
		else if(w_ref_id == menu_size_button.GetRefID())
		{
			//#196: cycle the menu/UI scale (wraps). Persisted; applied to menus on
			//open and to this menu live. SetRenderScale auto-caps it to fit.
			//#207: capped at 2x - 3x was too large and caused graphical glitches.
			static const double sizes[] = { 1.0, 1.5, 2.0 };
			const int n = sizeof(sizes) / sizeof(sizes[0]);
			int cur = 0;
			for(int i = 0; i < n; i++) if(zod_menu_scale <= sizes[i] + 0.01) { cur = i; break; }
			zod_menu_scale = sizes[(cur + 1) % n];
			ZPrefs_Save();
			SetRenderScale(zod_menu_scale);   //re-scale this open menu immediately
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

	//#73: the mouse wheel nudges the value under the cursor up (wheel up) or
	//down (wheel down), clamped at the ends - so you can lower volume/speed,
	//not just cycle forward with a click.
	case GMM_WHEELUP_EVENT:
	case GMM_WHEELDOWN_EVENT:
		{
			int dir = (event_type == GMM_WHEELUP_EVENT) ? +1 : -1;

			if(w_ref_id == volume_button.GetRefID())
			{
				int si = sound_setting ? *sound_setting : 0;
				si += dir;
				if(si < 0) si = 0;
				if(si >= MAX_SOUND_SETTINGS) si = MAX_SOUND_SETTINGS - 1;
				gmm_flags.set_volume = true;
				gmm_flags.set_volume_value = si;
			}
			else if(w_ref_id == speed_button.GetRefID())
			{
				int cur = MAX_GMMOPTIONS_SPEED_SETTINGS - 1;
				for(int i=0;i<MAX_GMMOPTIONS_SPEED_SETTINGS;i++)
					if(ztime && ztime->GameSpeed() <= gmmoption_speed_setting_value[i] + 0.01)
						{ cur = i; break; }
				int next = cur + dir;
				if(next < 0) next = 0;
				if(next >= MAX_GMMOPTIONS_SPEED_SETTINGS) next = MAX_GMMOPTIONS_SPEED_SETTINGS - 1;
				gmm_flags.set_game_speed = true;
				gmm_flags.set_game_speed_value = gmmoption_speed_setting_value[next];
			}
			else if(w_ref_id == max_units_button.GetRefID())
			{
				//#75: wheel adjusts max units up/down, clamped at the ends
				zod_max_units_per_team += dir * UNIT_LIMIT_STEP;
				if(zod_max_units_per_team < UNIT_LIMIT_MIN) zod_max_units_per_team = UNIT_LIMIT_MIN;
				if(zod_max_units_per_team > UNIT_LIMIT_MAX) zod_max_units_per_team = UNIT_LIMIT_MAX;
				ZPrefs_Save();
			}
		}
		break;
	}
}
