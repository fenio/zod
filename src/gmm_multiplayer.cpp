#include "gmm_multiplayer.h"
#include <cstdio>

GMMMultiplayer::GMMMultiplayer() : ZGuiMainMenuBase()
{
	menu_type = GMM_MULTIPLAYER;
	title = "Multiplayer";
	w = 112 + 96;
	h = 118;

	SetupLayout1();
}

void GMMMultiplayer::SetupLayout1()
{
	int next_y = GMM_TITLE_MARGIN;

	play_button.SetType(MMGENERIC_BUTTON);
	play_button.SetText("Play with someone");
	play_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	play_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&play_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	practice_button.SetType(MMGENERIC_BUTTON);
	practice_button.SetText("Practice vs Bots");
	practice_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	practice_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&practice_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	back_button.SetType(MMGENERIC_BUTTON);
	back_button.SetText("Back");
	back_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	back_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&back_button);
	next_y += GMMWBUTTON_HEIGHT + 2;

	status_label.SetText("");
	status_label.SetCoords(GMM_SIDE_MARGIN, next_y);
	AddWidget(&status_label);
	next_y += 12;

	h = next_y + 1 + GMM_BOTTOM_MARGIN;
	UpdateDimensions();
}

void GMMMultiplayer::Process()
{
	ProcessWidgets();
}

void GMMMultiplayer::HandleWidgetEvent(int event_type, ZGMMWidget *event_widget)
{
	if(!event_widget) return;
	if(event_type != GMM_UNCLICK_EVENT) return;

	int w_ref_id = event_widget->GetRefID();

	if(w_ref_id == play_button.GetRefID())
	{
		// Join the shared open match; the orchestrator seeds one if none has room.
		status_label.SetText("Finding a game...");
		MatchInfo mi;
		if(ZMP_Matchmake(mi))
		{
			gmm_flags.join_match = true;
			gmm_flags.join_host = mi.host;
			gmm_flags.join_port = mi.port;
		}
		else
		{
			status_label.SetText("Orchestrator unreachable (" + ZMP_OrchestratorAddress() + ")");
		}
	}
	else if(w_ref_id == practice_button.GetRefID())
	{
		gmm_flags.open_main_menu = true;
		gmm_flags.open_main_menu_type = GMM_MULTIPLAYER_CREATE;
	}
	else if(w_ref_id == back_button.GetRefID())
	{
		gmm_flags.open_main_menu = true;
		gmm_flags.open_main_menu_type = GMM_MAIN_MAIN;
	}
}
