#include "gmm_lobby.h"
#include <cstdio>

GMMLobby::GMMLobby() : ZGuiMainMenuBase()
{
	menu_type = GMM_LOBBY;
	title = "Lobby";
	w = 112 + 96;
	h = 118;

	SetupLayout1();
}

void GMMLobby::SetupLayout1()
{
	int next_y = GMM_TITLE_MARGIN;

	status_label.SetText("Waiting for players...");
	status_label.SetCoords(GMM_SIDE_MARGIN, next_y);
	AddWidget(&status_label);
	next_y += MMLABEL_HEIGHT + 2;

	player_list.SetCoords(GMM_SIDE_MARGIN, next_y);
	player_list.SetDimensions(w - (GMM_SIDE_MARGIN * 2), 70);
	player_list.SetVisibleEntries(5);
	AddWidget(&player_list);
	next_y += player_list.GetHeight() + 3;

	team_button.SetType(MMGENERIC_BUTTON);
	team_button.SetText("Change Team");
	team_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	team_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&team_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	bots_button.SetType(MMGENERIC_BUTTON);
	bots_button.SetText("Add Bots");
	bots_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	bots_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&bots_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	start_button.SetType(MMGENERIC_BUTTON);
	start_button.SetText("Start Game");
	start_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	start_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&start_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	leave_button.SetType(MMGENERIC_BUTTON);
	leave_button.SetText("Leave");
	leave_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	leave_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&leave_button);
	next_y += GMMWBUTTON_HEIGHT;

	h = next_y + 1 + GMM_BOTTOM_MARGIN;
	UpdateDimensions();
}

void GMMLobby::RebuildPlayers()
{
	if(!player_info) return;

	player_list.GetEntryList().clear();

	int humans = 0;
	for(vector<p_info>::iterator i = player_info->begin(); i != player_info->end(); ++i)
	{
		if(i->mode == PLAYER_MODE)
		{
			player_list.GetEntryList().push_back(
				mmlist_entry(team_type_string[i->team] + ": " + i->name, i->p_id, i->team));
			humans++;
		}
		else if(i->mode == BOT_MODE && !i->ignored)
		{
			player_list.GetEntryList().push_back(
				mmlist_entry(team_type_string[i->team] + ": (bot)", i->p_id, i->team + MAX_TEAM_TYPES));
		}
	}

	char buf[64];
	snprintf(buf, sizeof(buf), "Waiting for players... (%d here)", humans);
	status_label.SetText(buf);

	player_list.CheckViewI();
}

void GMMLobby::Process()
{
	RebuildPlayers();
	ProcessWidgets();
}

void GMMLobby::HandleWidgetEvent(int event_type, ZGMMWidget *event_widget)
{
	if(!event_widget) return;
	if(event_type != GMM_UNCLICK_EVENT) return;

	int w_ref_id = event_widget->GetRefID();

	if(w_ref_id == team_button.GetRefID())
	{
		gmm_flags.open_main_menu = true;
		gmm_flags.open_main_menu_type = GMM_CHANGE_TEAMS;
	}
	else if(w_ref_id == bots_button.GetRefID())
	{
		gmm_flags.open_main_menu = true;
		gmm_flags.open_main_menu_type = GMM_MANAGE_BOTS;
	}
	else if(w_ref_id == start_button.GetRefID())
	{
		gmm_flags.start_match = true;   //ZPlayer un-pauses the match
		killme = true;                  //close the lobby so the game shows
	}
	else if(w_ref_id == leave_button.GetRefID())
	{
		gmm_flags.open_main_menu = true;
		gmm_flags.open_main_menu_type = GMM_MAIN_MAIN;
	}
}
