#ifndef _ZGMM_MULTIPLAYER_H_
#define _ZGMM_MULTIPLAYER_H_

#include "zgui_main_menu_base.h"
#include "zmpclient.h"

// Multiplayer front door (matchmaking).
//  - "Play with someone": ask the orchestrator (POST /matchmake) for the shared
//    open match and join it -> lobby, where you wait for a fellow and ready up.
//  - "Practice vs Bots": the create form (pick a map + bots) for solo testing.
// The old server browser is retired; all orchestrator HTTP lives behind zmpclient.
class GMMMultiplayer : public ZGuiMainMenuBase
{
public:
	GMMMultiplayer();

	void Process();
private:
	GMMWButton play_button;
	GMMWButton practice_button;
	GMMWButton back_button;
	GMMWLabel status_label;

	void SetupLayout1();
	void HandleWidgetEvent(int event_type, ZGMMWidget *event_widget);
};

#endif
