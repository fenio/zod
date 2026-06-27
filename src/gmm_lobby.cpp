#include "gmm_lobby.h"
#include <cstdio>

GMMLobby::GMMLobby() : ZGuiMainMenuBase()
{
	menu_type = GMM_LOBBY;
	title = "Lobby";
	w = 112 + 96;
	h = 118;

	saw_paused = false;

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

	ready_button.SetType(MMGENERIC_BUTTON);
	ready_button.SetText("I'm Ready");
	ready_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	ready_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&ready_button);
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

// My own player record, matched by the client p_id the host pushes onto the menu.
p_info *GMMLobby::LocalPlayer()
{
	if(!player_info || !local_p_id) return NULL;

	for(vector<p_info>::iterator i = player_info->begin(); i != player_info->end(); ++i)
		if(i->p_id == *local_p_id) return &(*i);

	return NULL;
}

void GMMLobby::RebuildPlayers()
{
	if(!player_info) return;

	player_list.GetEntryList().clear();

	int humans = 0, ready = 0;
	for(vector<p_info>::iterator i = player_info->begin(); i != player_info->end(); ++i)
	{
		if(i->mode == PLAYER_MODE)
		{
			string mark = i->ready ? "[R] " : "[ ] ";
			string you = (local_p_id && i->p_id == *local_p_id) ? " (you)" : "";
			player_list.GetEntryList().push_back(
				mmlist_entry(mark + team_type_string[i->team] + ": " + i->name + you, i->p_id, i->team));
			humans++;
			if(i->ready) ready++;
		}
		else if(i->mode == BOT_MODE && !i->ignored)
		{
			player_list.GetEntryList().push_back(
				mmlist_entry("    " + team_type_string[i->team] + ": (bot)", i->p_id, i->team + MAX_TEAM_TYPES));
		}
	}

	char buf[80];
	if(humans < 2)
		snprintf(buf, sizeof(buf), "Waiting for another player... (%d here)", humans);
	else if(ready < humans)
		snprintf(buf, sizeof(buf), "Ready %d/%d - waiting...", ready, humans);
	else
		snprintf(buf, sizeof(buf), "All ready - starting!");
	status_label.SetText(buf);

	// Reflect my own ready state on the toggle.
	p_info *me = LocalPlayer();
	bool iam = me && me->ready;
	ready_button.SetText(iam ? "Not Ready" : "I'm Ready");
	ready_button.SetGreen(iam);

	player_list.CheckViewI();
}

void GMMLobby::Process()
{
	// The server auto-starts (un-pauses) once everyone's ready. When that happens,
	// close the lobby so the game shows. Guard on saw_paused so we don't close in
	// the brief window before the initial paused state has arrived.
	if(ztime)
	{
		if(ztime->IsPaused()) saw_paused = true;
		else if(saw_paused) { killme = true; return; }
	}

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
	else if(w_ref_id == ready_button.GetRefID())
	{
		p_info *me = LocalPlayer();
		bool cur = me && me->ready;
		gmm_flags.toggle_ready = true;       //ZPlayer sends it; the server starts when all ready
		gmm_flags.ready_value = !cur;
	}
	else if(w_ref_id == leave_button.GetRefID())
	{
		gmm_flags.open_main_menu = true;
		gmm_flags.open_main_menu_type = GMM_MAIN_MAIN;
	}
}
