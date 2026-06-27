#ifndef _ZGMM_LOBBY_H_
#define _ZGMM_LOBBY_H_

#include "zgui_main_menu_base.h"

// Pre-match waiting room (matchmaking). You land here after "play with someone"
// or creating/joining a match (which is spawned paused). It lists who's connected
// with their ready state; each player clicks "I'm ready". When every human is
// ready - and there are at least two - the SERVER auto-starts the match for
// everyone, and this lobby closes itself.
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
	GMMWButton ready_button;
	GMMWButton leave_button;

	bool saw_paused;   //don't auto-close until we've actually seen the match paused

	void SetupLayout1();
	void HandleWidgetEvent(int event_type, ZGMMWidget *event_widget);
	void RebuildPlayers();
	p_info *LocalPlayer();   //my own p_info (by local_p_id), or NULL
};

#endif
