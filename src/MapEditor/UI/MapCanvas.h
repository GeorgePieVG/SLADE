#pragma once

#include "General/KeyBind.h"
#include "UI/Canvas/GLCanvas.h"

namespace slade
{
class MapEditContext;

class MapCanvas : public GLCanvas, public KeyBindHandler
{
public:
	MapCanvas(wxWindow* parent, MapEditContext* context);
	~MapCanvas() override = default;

	// Drawing
	void draw() override;

	// Mouse
	void mouseToCenter();
	void lockMouse(bool lock);
	void mouseLook3d();

	// Keybind handling
	void onKeyBindPress(string_view name) override;

private:
	MapEditContext* context_    = nullptr;
	bool            mouse_warp_ = false;
	vector<int>     fps_avg_;
	sf::Clock       sf_clock_;
	wxTimer         timer_;
	long            last_time_ = 0;

	void update();

	// Events
	void onSize(wxSizeEvent& e);
	void onKeyDown(wxKeyEvent& e);
	void onKeyUp(wxKeyEvent& e);
	void onMouseDown(wxMouseEvent& e);
	void onMouseUp(wxMouseEvent& e);
	void onMouseMotion(wxMouseEvent& e);
	void onMouseWheel(wxMouseEvent& e);
	void onMouseLeave(wxMouseEvent& e);
	void onMouseEnter(wxMouseEvent& e);
	void onIdle(wxIdleEvent& e);
	void onRTimer(wxTimerEvent& e);
	void onFocus(wxFocusEvent& e);
};
} // namespace slade
