#include "gmm_multiplayer.h"
#include <cstdio>

GMMMultiplayer::GMMMultiplayer() : ZGuiMainMenuBase()
{
	menu_type = GMM_MULTIPLAYER;
	title = "Multiplayer";
	w = 112 + 144;
	h = 118;

	SetupLayout1();
	Refresh();   //initial fetch (blocking, localhost - fast)
}

void GMMMultiplayer::SetupLayout1()
{
	int next_y = GMM_TITLE_MARGIN;

	match_list.SetCoords(GMM_SIDE_MARGIN, next_y);
	match_list.SetDimensions(w - (GMM_SIDE_MARGIN * 2), 70);
	match_list.SetVisibleEntries(5);
	AddWidget(&match_list);
	next_y += match_list.GetHeight() + 2;

	status_label.SetText("");
	status_label.SetCoords(GMM_SIDE_MARGIN, next_y);
	AddWidget(&status_label);
	next_y += 12;

	join_button.SetType(MMGENERIC_BUTTON);
	join_button.SetText("Join");
	join_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	join_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&join_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	create_button.SetType(MMGENERIC_BUTTON);
	create_button.SetText("Create Game");
	create_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	create_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&create_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	refresh_button.SetType(MMGENERIC_BUTTON);
	refresh_button.SetText("Refresh");
	refresh_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	refresh_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&refresh_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	back_button.SetType(MMGENERIC_BUTTON);
	back_button.SetText("Back");
	back_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	back_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&back_button);
	next_y += GMMWBUTTON_HEIGHT;

	h = next_y + 1 + GMM_BOTTOM_MARGIN;
	UpdateDimensions();
}

void GMMMultiplayer::Refresh()
{
	bool ok = ZMP_ListMatches(matches);

	match_list.GetEntryList().clear();

	if(!ok)
	{
		status_label.SetText("Orchestrator unreachable (" + ZMP_OrchestratorAddress() + ")");
	}
	else if(matches.empty())
	{
		status_label.SetText("No matches - create one");
	}
	else
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "%d match(es)", (int)matches.size());
		status_label.SetText(buf);

		for(size_t i = 0; i < matches.size(); i++)
		{
			//drop the ".map" extension and show players as "N/4" (4 teams max).
			string mapdisp = matches[i].map;
			size_t dot = mapdisp.rfind(".map");
			if(dot != string::npos) mapdisp.erase(dot);

			int players = matches[i].players;

			char line[192];
			if(players >= 0)
				snprintf(line, sizeof(line), "%s  -  %s  (%d/4)",
					matches[i].name.c_str(), mapdisp.c_str(), players);
			else
				snprintf(line, sizeof(line), "%s  -  %s",
					matches[i].name.c_str(), mapdisp.c_str());

			match_list.GetEntryList().push_back(mmlist_entry(line, (int)i, (int)i));
		}
	}

	match_list.CheckViewI();
}

void GMMMultiplayer::Process()
{
	ProcessWidgets();
}

void GMMMultiplayer::HandleWidgetEvent(int event_type, ZGMMWidget *event_widget)
{
	if(!event_widget) return;

	int w_ref_id = event_widget->GetRefID();

	switch(event_type)
	{
	case GMM_CLICK_EVENT:
		if(w_ref_id == match_list.GetRefID())
		{
			if(match_list.GetGMMWFlags().mmlist_entry_selected != -1)
				match_list.UnSelectAll(match_list.GetGMMWFlags().mmlist_entry_selected);
		}
		break;
	case GMM_UNCLICK_EVENT:
		if(w_ref_id == refresh_button.GetRefID())
		{
			Refresh();
		}
		else if(w_ref_id == create_button.GetRefID())
		{
			gmm_flags.open_main_menu = true;
			gmm_flags.open_main_menu_type = GMM_MULTIPLAYER_CREATE;
		}
		else if(w_ref_id == back_button.GetRefID())
		{
			gmm_flags.open_main_menu = true;
			gmm_flags.open_main_menu_type = GMM_MAIN_MAIN;
		}
		else if(w_ref_id == join_button.GetRefID())
		{
			int sel = match_list.GetFirstSelected();
			if(sel != -1)
			{
				int idx = match_list.GetEntryList()[sel].ref_id;
				if(idx >= 0 && idx < (int)matches.size())
				{
					gmm_flags.join_match = true;
					gmm_flags.join_host = matches[idx].host;
					gmm_flags.join_port = matches[idx].port;
				}
			}
		}
		break;
	}
}
