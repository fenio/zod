#ifndef _ZGMM_MULTIPLAYER_H_
#define _ZGMM_MULTIPLAYER_H_

#include "zgui_main_menu_base.h"
#include "zmpclient.h"

// Browse the matches the orchestrator knows about and join one. "Create Game"
// opens GMMMultiplayerCreate. All orchestrator HTTP lives behind zmpclient; this
// is pure UI that, on Join, sets gmm_flags.join_match (+ host/port) for ZPlayer
// to act on via ConnectToServer.
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
