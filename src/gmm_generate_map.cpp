#include "gmm_generate_map.h"

GMMGenerateMap::GMMGenerateMap() : ZGuiMainMenuBase()
{
	menu_type = GMM_GENERATE_MAP;
	title = "Generate Map";
	w = 124;
	h = 118;

	enemies_i = 0;
	size_i = 0;
	terrain_i = 0;
	tech_i = 0;

	SetupLayout1();
}

void GMMGenerateMap::SetupLayout1()
{
	int next_y;

	next_y = GMM_TITLE_MARGIN;

	enemies_button.SetType(MMGENERIC_BUTTON);
	enemies_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	enemies_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&enemies_button);
	next_y += GMMWBUTTON_HEIGHT + 2;

	size_button.SetType(MMGENERIC_BUTTON);
	size_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	size_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&size_button);
	next_y += GMMWBUTTON_HEIGHT + 2;

	terrain_button.SetType(MMGENERIC_BUTTON);
	terrain_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	terrain_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&terrain_button);
	next_y += GMMWBUTTON_HEIGHT + 2;

	tech_button.SetType(MMGENERIC_BUTTON);
	tech_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	tech_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&tech_button);
	next_y += GMMWBUTTON_HEIGHT + 7;

	generate_button.SetType(MMGENERIC_BUTTON);
	generate_button.SetText("Generate & Start");
	generate_button.SetCoords(GMM_SIDE_MARGIN, next_y);
	generate_button.SetDimensions(w - (GMM_SIDE_MARGIN * 2), GMMWBUTTON_HEIGHT);
	AddWidget(&generate_button);
	next_y += GMMWBUTTON_HEIGHT + 1;

	h = next_y + GMM_BOTTOM_MARGIN;

	UpdateButtonText();
	UpdateDimensions();
}

void GMMGenerateMap::UpdateButtonText()
{
	char buf[64];

	sprintf(buf, "Enemies: %d", enemies_i + 1);
	enemies_button.SetText(buf);

	sprintf(buf, "Size: %s", gmmgen_size_name[size_i]);
	size_button.SetText(buf);

	sprintf(buf, "Terrain: %s", gmmgen_terrain_name[terrain_i]);
	terrain_button.SetText(buf);

	sprintf(buf, "Tech: %s", gmmgen_tech_name[tech_i]);
	tech_button.SetText(buf);
}

void GMMGenerateMap::Process()
{
	ProcessWidgets();
}

void GMMGenerateMap::HandleWidgetEvent(int event_type, ZGMMWidget *event_widget)
{
	int w_ref_id;

	if(!event_widget) return;
	if(event_type != GMM_UNCLICK_EVENT) return;

	w_ref_id = event_widget->GetRefID();

	if(w_ref_id == enemies_button.GetRefID())
	{
		enemies_i = (enemies_i + 1) % 3;
		UpdateButtonText();
	}
	else if(w_ref_id == size_button.GetRefID())
	{
		size_i = (size_i + 1) % MAX_GMMGEN_SIZES;
		UpdateButtonText();
	}
	else if(w_ref_id == terrain_button.GetRefID())
	{
		terrain_i = (terrain_i + 1) % MAX_GMMGEN_TERRAINS;
		UpdateButtonText();
	}
	else if(w_ref_id == tech_button.GetRefID())
	{
		tech_i = (tech_i + 1) % MAX_GMMGEN_TECHS;
		UpdateButtonText();
	}
	else if(w_ref_id == generate_button.GetRefID())
	{
		gmm_flags.generate_map = true;
		gmm_flags.gen_enemies = enemies_i + 1;
		gmm_flags.gen_width = gmmgen_size_w[size_i];
		gmm_flags.gen_height = gmmgen_size_h[size_i];
		gmm_flags.gen_terrain = terrain_i;
		gmm_flags.gen_tech = gmmgen_tech_level[tech_i];
	}
}
