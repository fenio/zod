#ifndef _ZGMM_MULTIPLAYER_CREATE_H_
#define _ZGMM_MULTIPLAYER_CREATE_H_

#include "zgui_main_menu_base.h"
#include "zmpclient.h"
#include <vector>

// Create-a-match form: pick a map from a scrollable list, toggle bot teams,
// Start. Start POSTs to the orchestrator and, on success, sets
// gmm_flags.join_match so ZPlayer connects to the freshly spawned server. Back
// returns to the match browser. The map list comes from the orchestrator (the
// client only knows campaign display names, not the .map filenames it accepts).
#define GMM_MP_CREATE_BOTS 3   // blue / green / yellow (red is the joining human)

class GMMMultiplayerCreate : public ZGuiMainMenuBase
{
public:
	GMMMultiplayerCreate();

	void Process();
private:
	GMMWList map_list;
	GMMWButton bot_button[GMM_MP_CREATE_BOTS];
	GMMWButton start_button;
	GMMWButton back_button;
	GMMWLabel status_label;

	bool bot_on[GMM_MP_CREATE_BOTS];
	std::vector<MapMeta> map_names;   //maps from the orchestrator (filename + player slots)

	void SetupLayout1();
	void HandleWidgetEvent(int event_type, ZGMMWidget *event_widget);
	string SelectedMap();
};

#endif
