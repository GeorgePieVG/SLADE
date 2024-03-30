#pragma once

namespace slade
{
class GfxCanvas;
class ColourBox;

class GfxColouriseDialog : public wxDialog
{
public:
	GfxColouriseDialog(wxWindow* parent, ArchiveEntry* entry, const Palette& pal);

	ColRGBA colour() const;
	void    setColour(const wxString& col) const;

private:
	GfxCanvas*          gfx_preview_ = nullptr;
	ArchiveEntry*       entry_       = nullptr;
	unique_ptr<Palette> palette_;
	ColourBox*          cb_colour_ = nullptr;

	// Events
	void onColourChanged(wxEvent& e);
	void onResize(wxSizeEvent& e);
};
} // namespace slade
