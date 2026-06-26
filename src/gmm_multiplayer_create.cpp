#include "gmm_multiplayer_create.h"
#include "zmpclient.h"
#include <cstdio>

static const char *bot_team_names[GMM_MP_CREATE_BOTS] = { "blue", "green", "yellow" };

GMMMultiplayerCreate::GMMMultiplayerCreate() : ZGuiMainMenuBase()
{
	menu_type = GMM_MULTIPLAYER_CREATE;
	title = "Create Game";
	w = 112 + 96;
	h = 118;

	bot_on[0] = true;    // blue
	bot_on[1] = true;    // green
	bot_on[2] = false;   // yellow

	//the orchestrator is the source of truth for spawnable map files: the client
	//only knows campaign display names, not filenames.
	ZMP_ListMaps(map_names);

	SetupLayout1();
}

void GMMMultiplayerCreate::SetupLayout1()
{
	int next_y = GMM_TITLE_MARGIN;

	map_list.SetCoords(GMM_SIDE_MARGIN, next_y);
	map_list.SetDimensions(w - (GMM_SIDE_MARGIN * 2), 84);
	map_list.SetVisibleEntries(6);
	for(size_t i = 0; i < map_names.size(); i++)
		map_list.GetEntryList().push_back(mmlist_entry(map_names[i], (int)i, (int)i));
	map_list.CheckViewI();
	AddWidget(&map_list);
	next_y += map_list.GetHeight() + 3;

	for(int i = 0; i < GMM_MP_CREATE_BOTS; i++)
	{
		bot_button[i].SetType(MMGENERIC_BUTTON);
		bot_button[i].SetCoords(GMM_SIDE_MARGIN, next_y);
		bot_button[i].SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
		AddWidget(&bot_button[i]);
		next_y += GMMWBUTTON_HEIGHT + 1;
	}

	next_y += 2;
	start_button.SetType(MMGENERIC_BUTTON);
	start_button.SetText("Start");
	start_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	start_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&start_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	back_button.SetType(MMGENERIC_BUTTON);
	back_button.SetText("Back");
	back_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	back_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&back_button);
	next_y += GMMWBUTTON_HEIGHT + 2;

	status_label.SetText(map_names.empty() ? "No maps from orchestrator" : "Pick a map + Start");
	status_label.SetCoords(GMM_SIDE_MARGIN, next_y);
	AddWidget(&status_label);
	next_y += 12;

	h = next_y + 1 + GMM_BOTTOM_MARGIN;
	UpdateDimensions();
}

// The highlighted map, defaulting to the first entry if nothing's clicked yet.
string GMMMultiplayerCreate::SelectedMap()
{
	if(map_list.GetEntryList().empty()) return "";

	int sel = map_list.GetFirstSelected();
	if(sel == -1) sel = 0;

	int idx = map_list.GetEntryList()[sel].ref_id;
	if(idx < 0 || idx >= (int)map_names.size()) return "";
	return map_names[idx];
}

void GMMMultiplayerCreate::Process()
{
	for(int i = 0; i < GMM_MP_CREATE_BOTS; i++)
	{
		bot_button[i].SetText(string("Bot ") + bot_team_names[i] + (bot_on[i] ? ": on" : ": off"));
		bot_button[i].SetGreen(bot_on[i]);
	}

	ProcessWidgets();
}

void GMMMultiplayerCreate::HandleWidgetEvent(int event_type, ZGMMWidget *event_widget)
{
	if(!event_widget) return;

	int w_ref_id = event_widget->GetRefID();

	if(event_type == GMM_CLICK_EVENT)
	{
		//single-select: keep only the just-clicked map highlighted
		if(w_ref_id == map_list.GetRefID())
			if(map_list.GetGMMWFlags().mmlist_entry_selected != -1)
				map_list.UnSelectAll(map_list.GetGMMWFlags().mmlist_entry_selected);
		return;
	}

	if(event_type != GMM_UNCLICK_EVENT) return;

	for(int i = 0; i < GMM_MP_CREATE_BOTS; i++)
		if(w_ref_id == bot_button[i].GetRefID())
		{
			bot_on[i] = !bot_on[i];
			return;
		}

	if(w_ref_id == back_button.GetRefID())
	{
		gmm_flags.open_main_menu = true;
		gmm_flags.open_main_menu_type = GMM_MULTIPLAYER;
		return;
	}

	if(w_ref_id == start_button.GetRefID())
	{
		string map = SelectedMap();
		if(map.empty())
		{
			status_label.SetText("Pick a map first");
			return;
		}

		vector<string> bots;
		for(int i = 0; i < GMM_MP_CREATE_BOTS; i++)
			if(bot_on[i]) bots.push_back(bot_team_names[i]);

		status_label.SetText("Creating...");

		MatchInfo created;
		if(ZMP_CreateMatch(map, bots, created))
		{
			gmm_flags.join_match = true;
			gmm_flags.join_host = created.host;
			gmm_flags.join_port = created.port;
		}
		else
		{
			status_label.SetText("Create failed (" + ZMP_OrchestratorAddress() + ")");
		}
	}
}
