#ifndef _ZGMM_MULTIPLAYER_H_
#define _ZGMM_MULTIPLAYER_H_

#include "zgui_main_menu_base.h"
#include "zmpclient.h"

// Multiplayer match browser. Lists the orchestrator's live matches with their map
// and player count (N/capacity, capacity = the map's slots), and lets you Join one
// or Create a new one (pick the map + size). All orchestrator HTTP lives behind
// zmpclient; this is pure UI that, on Join/Create, sets gmm_flags.join_match so
// ZPlayer connects to the match server and lands in the lobby.
class GMMMultiplayer : public ZGuiMainMenuBase
{
public:
	GMMMultiplayer();

	void Process();
private:
	GMMWList match_list;
	GMMWLabel status_label;
	GMMWButton join_button;
	GMMWButton create_button;
	GMMWButton refresh_button;
	GMMWButton back_button;

	vector<MatchInfo> matches;

	void SetupLayout1();
	void HandleWidgetEvent(int event_type, ZGMMWidget *event_widget);
	void Refresh();
};

#endif
