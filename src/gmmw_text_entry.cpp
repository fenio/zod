#include "zgui_main_menu_widgets.h"

//#246: single-line editable text field (used for the player name in Options).
//Mirrors GMMWLabel's text rendering; edit logic mirrors the old ZGuiTextBox.

GMMWTextEntry::GMMWTextEntry() : ZGMMWidget()
{
	widget_type = MMTEXTENTRY_WIDGET;

	rerender_text = false;
	selected = false;
	changed = false;
	max_text = 16;
	h = GMMWTEXTENTRY_HEIGHT;
}

bool GMMWTextEntry::Click(int x_, int y_)
{
	if(!active) return false;

	bool hit = WithinDimensions(x_, y_);

	//clicking the field focuses it; a click anywhere else in the menu drops focus
	//(every widget's Click() is called, so we deselect when the hit isn't ours).
	SetSelected(hit);

	return hit;
}

bool GMMWTextEntry::KeyPress(int c)
{
	if(!active) return false;
	if(!selected) return false;   //only the focused field consumes typing

	if(c == 8 || c == 127)   //backspace / delete
	{
		if(text.length())
		{
			text.erase(text.length() - 1, 1);
			rerender_text = true;
			changed = true;
		}
	}
	else if(c == 13 || c == 9)   //enter / tab: commit + drop focus
	{
		SetSelected(false);
	}
	else
	{
		if((int)text.length() >= max_text) return true;
		if(!good_user_char(c)) return true;

		text += (char)c;
		rerender_text = true;
		changed = true;
	}

	return true;
}

void GMMWTextEntry::DoRender(ZMap &the_map, SDL_Surface *dest, int tx, int ty)
{
	if(!active) return;

	tx += x;
	ty += y;

	MakeTextImage();

	if(text_img.GetBaseSurface())
		text_img.BlitSurface(NULL, tx, ty);
}

void GMMWTextEntry::MakeTextImage()
{
	if(!rerender_text) return;

	//show a trailing caret while focused (mirrors the old text box's '{' marker)
	string disp = label + text + (selected ? "{" : "");

	if(disp.length())
		text_img.LoadBaseImage(ZFontEngine::GetFont(YELLOW_MENU_FONT).Render(disp.c_str()));
	else
		text_img.Unload();

	rerender_text = false;
}
