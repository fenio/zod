#ifndef _ZGMM_LOBBY_H_
#define _ZGMM_LOBBY_H_

#include "zgui_main_menu_base.h"

// Pre-match waiting room. You land here after creating or joining a match (which
// is spawned paused). It lists who's connected and their teams, lets you pick a
// team and add bots (via the existing in-game menus), and Start un-pauses the
// match for everyone. The game stays paused - "ready, waiting" - until Start.
class GMMLobby : public ZGuiMainMenuBase
{
public:
	GMMLobby();

	void Process();
private:
	GMMWLabel status_label;
	GMMWList player_list;
	GMMWButton team_button;
	GMMWButton bots_button;
	GMMWButton start_button;
	GMMWButton leave_button;

	void SetupLayout1();
	void HandleWidgetEvent(int event_type, ZGMMWidget *event_widget);
	void RebuildPlayers();
};

#endif
