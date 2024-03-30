
// -----------------------------------------------------------------------------
// SLADE - It's a Doom Editor
// Copyright(C) 2008 - 2024 Simon Judd
//
// Email:       sirjuddington@gmail.com
// Web:         https://slade.mancubus.net
// Filename:    ActionSpecialPanel.cpp
// Description: UI for selecting an action special (and/or generalised special)
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
//
// Includes
//
// -----------------------------------------------------------------------------
#include "Main.h"
#include "ActionSpecialPanel.h"
#include "ArgsPanel.h"
#include "Dialogs/SpecialPresetDialog.h"
#include "Game/ActionSpecial.h"
#include "Game/Configuration.h"
#include "Game/SpecialPreset.h"
#include "Game/UDMFProperty.h"
#include "GenLineSpecialPanel.h"
#include "General/UI.h"
#include "MapEditor/MapEditContext.h"
#include "MapEditor/MapEditor.h"
#include "SLADEMap/MapObject/MapLine.h"
#include "UI/Controls/NumberTextCtrl.h"
#include "UI/WxUtils.h"

using namespace slade;


// -----------------------------------------------------------------------------
// ActionSpecialtreeView Class
//
// A wxDataViewTreeCtrl specialisation showing the action specials and groups in
// a tree structure
// -----------------------------------------------------------------------------
namespace slade
{
class ActionSpecialTreeView : public wxDataViewTreeCtrl
{
public:
	ActionSpecialTreeView(wxWindow* parent);
	~ActionSpecialTreeView() override = default;

	void setParentDialog(wxDialog* dlg) { parent_dialog_ = dlg; }

	int  specialNumber(wxDataViewItem item) const;
	void showSpecial(int special, bool focus = true);
	int  selectedSpecial() const;

private:
	wxDataViewItem root_;
	wxDataViewItem item_none_;
	wxDialog*      parent_dialog_ = nullptr;

	struct ASTVGroup
	{
		wxString       name;
		wxDataViewItem item;
		ASTVGroup(wxDataViewItem i, const wxString& name) : name{ name }, item{ i } {}
	};
	vector<ASTVGroup> groups_;

	wxDataViewItem getGroup(const wxString& group_name);
};
} // namespace slade

// -----------------------------------------------------------------------------
// ActionSpecialTreeView class constructor
// -----------------------------------------------------------------------------
ActionSpecialTreeView::ActionSpecialTreeView(wxWindow* parent) : wxDataViewTreeCtrl{ parent, -1 }, root_{ nullptr }
{
	// Add 'None'
	item_none_ = AppendItem(root_, "0: None");

	// Computing the minimum width of the tree is slightly complicated, since
	// wx doesn't expose it to us directly
	wxClientDC dc(this);
	dc.SetFont(GetFont());
	wxSize textsize;

	// Populate tree
	for (auto& i : game::configuration().allActionSpecials())
	{
		if (!i.second.defined())
			continue;

		wxString label = wxString::Format("%d: %s", i.second.number(), i.second.name());
		AppendItem(getGroup(i.second.group()), label);
		textsize.IncTo(dc.GetTextExtent(label));
	}
	Expand(root_);

	// Bind events
	Bind(wxEVT_DATAVIEW_ITEM_START_EDITING, [&](wxDataViewEvent& e) { e.Veto(); });
	Bind(
		wxEVT_DATAVIEW_ITEM_ACTIVATED,
		[&](wxDataViewEvent&)
		{
			if (parent_dialog_)
				parent_dialog_->EndModal(wxID_OK);
		});

	// 64 is an arbitrary fudge factor -- should be at least the width of a
	// scrollbar plus the expand icons plus any extra padding
	int min_width = textsize.GetWidth() + GetIndent() + ui::scalePx(64);
	wxWindowBase::SetMinSize({ min_width, ui::scalePx(200) });
}

// -----------------------------------------------------------------------------
// Returns the action special value for [item]
// -----------------------------------------------------------------------------
int ActionSpecialTreeView::specialNumber(wxDataViewItem item) const
{
	wxString num = GetItemText(item).BeforeFirst(':');
	long     s;
	if (!num.ToLong(&s))
		s = -1;

	return s;
}

// -----------------------------------------------------------------------------
// Finds the item for [special], selects it and ensures it is shown
// -----------------------------------------------------------------------------
void ActionSpecialTreeView::showSpecial(int special, bool focus)
{
	if (special == 0)
	{
		EnsureVisible(item_none_);
		Select(item_none_);
		if (focus)
			SetFocus();
		return;
	}

	// Go through item groups
	for (auto& group : groups_)
	{
		// Go through group items
		for (int b = 0; b < GetChildCount(group.item); b++)
		{
			auto item = GetNthChild(group.item, b);

			// Select+show if match
			if (specialNumber(item) == special)
			{
				EnsureVisible(item);
				Select(item);
				if (focus)
					SetFocus();
				return;
			}
		}
	}
}

// -----------------------------------------------------------------------------
// Returns the currently selected action special value
// -----------------------------------------------------------------------------
int ActionSpecialTreeView::selectedSpecial() const
{
	auto item = GetSelection();
	if (item.IsOk())
		return specialNumber(item);
	else
		return -1;
}

// -----------------------------------------------------------------------------
// Returns the parent wxDataViewItem representing action special group [group]
// -----------------------------------------------------------------------------
wxDataViewItem ActionSpecialTreeView::getGroup(const wxString& group_name)
{
	// Check if group was already made
	for (auto& group : groups_)
	{
		if (group_name == group.name)
			return group.item;
	}

	// Split group into subgroups
	auto path = wxSplit(group_name, '/');

	// Create group needed
	auto     current  = root_;
	wxString fullpath = "";
	for (unsigned p = 0; p < path.size(); p++)
	{
		if (p > 0)
			fullpath += "/";
		fullpath += path[p];

		bool found = false;
		for (auto& group : groups_)
		{
			if (group.name == fullpath)
			{
				current = group.item;
				found   = true;
				break;
			}
		}

		if (!found)
		{
			current = AppendContainer(current, path[p], -1, 1);
			groups_.emplace_back(current, fullpath);
		}
	}

	return current;
}


// -----------------------------------------------------------------------------
//
// ActionSpecialPanel Class Functions
//
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// ActionSpecialPanel class constructor
// -----------------------------------------------------------------------------
ActionSpecialPanel::ActionSpecialPanel(wxWindow* parent, bool trigger) : wxPanel(parent, -1)
{
	panel_args_     = nullptr;
	choice_trigger_ = nullptr;
	show_trigger_   = trigger;

	// Setup layout
	auto sizer = new wxBoxSizer(wxVERTICAL);

	if (game::configuration().featureSupported(game::Feature::Boom))
	{
		// Action Special radio button
		auto hbox = new wxBoxSizer(wxHORIZONTAL);
		sizer->Add(hbox, wxutil::sfWithBorder(0, wxBOTTOM).Expand());
		rb_special_ = new wxRadioButton(this, -1, "Action Special", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
		hbox->Add(rb_special_, wxutil::sfWithBorder(0, wxRIGHT).Expand());

		// Generalised Special radio button
		rb_generalised_ = new wxRadioButton(this, -1, "Generalised Special");
		hbox->Add(rb_generalised_, wxSizerFlags().Expand());

		// Boom generalised line special panel
		panel_gen_specials_ = new GenLineSpecialPanel(this);
		panel_gen_specials_->Show(false);

		// Bind events
		rb_special_->Bind(wxEVT_RADIOBUTTON, &ActionSpecialPanel::onRadioButtonChanged, this);
		rb_generalised_->Bind(wxEVT_RADIOBUTTON, &ActionSpecialPanel::onRadioButtonChanged, this);
	}

	// Action specials tree
	setupSpecialPanel();
	sizer->Add(panel_action_special_, 1, wxEXPAND);

	SetSizerAndFit(sizer);

	// Bind events
	tree_specials_->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &ActionSpecialPanel::onSpecialSelectionChanged, this);
	tree_specials_->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &ActionSpecialPanel::onSpecialItemActivated, this);
}

// -----------------------------------------------------------------------------
// Creates and sets up the action special panel
// -----------------------------------------------------------------------------
void ActionSpecialPanel::setupSpecialPanel()
{
	// Create panel
	panel_action_special_ = new wxPanel(this, -1);
	auto sizer            = new wxBoxSizer(wxVERTICAL);

	// Special box
	text_special_ = new NumberTextCtrl(panel_action_special_);
	sizer->Add(text_special_, wxutil::sfWithBorder(0, wxBOTTOM).Expand());
	text_special_->Bind(
		wxEVT_TEXT, [&](wxCommandEvent& e) { tree_specials_->showSpecial(text_special_->number(), false); });

	// Action specials tree
	tree_specials_ = new ActionSpecialTreeView(panel_action_special_);
	sizer->Add(tree_specials_, wxSizerFlags(1).Expand());

	if (show_trigger_)
	{
		// UDMF Triggers
		if (mapeditor::editContext().mapDesc().format == MapFormat::UDMF)
		{
			// Get all UDMF properties
			auto& props = game::configuration().allUDMFProperties(MapObject::Type::Line);

			// Get all UDMF trigger properties
			std::map<wxString, wxFlexGridSizer*> named_flexgrids;
			for (auto& i : props)
			{
				if (!i.second.isTrigger())
					continue;

				wxString group       = i.second.group();
				auto     frame_sizer = named_flexgrids[group];
				if (!frame_sizer)
				{
					auto frame_triggers = new wxStaticBox(panel_action_special_, -1, group);
					auto sizer_triggers = new wxStaticBoxSizer(frame_triggers, wxVERTICAL);
					sizer->Add(sizer_triggers, wxutil::sfWithBorder(0, wxTOP).Expand());

					frame_sizer = new wxFlexGridSizer(3, ui::pad() / 2, ui::pad());
					frame_sizer->AddGrowableCol(0, 1);
					frame_sizer->AddGrowableCol(1, 1);
					frame_sizer->AddGrowableCol(2, 1);
					sizer_triggers->Add(frame_sizer, wxutil::sfWithBorder(1).Expand());

					named_flexgrids.find(group)->second = frame_sizer;
				}

				auto cb_trigger = new wxCheckBox(
					panel_action_special_, -1, i.second.name(), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE);
				frame_sizer->Add(cb_trigger, wxSizerFlags().Expand());

				flags_.push_back({ cb_trigger, -1, i.second.propName() });
			}
		}

		// Hexen trigger
		else if (mapeditor::editContext().mapDesc().format == MapFormat::Hexen)
		{
			auto frame_trigger = new wxStaticBox(panel_action_special_, -1, "Special Trigger");
			auto sizer_trigger = new wxStaticBoxSizer(frame_trigger, wxVERTICAL);
			sizer->Add(sizer_trigger, wxutil::sfWithBorder().Expand());

			// Add triggers dropdown
			auto spac_triggers = wxutil::arrayStringStd(game::configuration().allSpacTriggers());
			choice_trigger_ = new wxChoice(panel_action_special_, -1, wxDefaultPosition, wxDefaultSize, spac_triggers);
			sizer_trigger->Add(choice_trigger_, wxutil::sfWithBorder().Expand());

			// Add activation-related flags
			auto fg_sizer = new wxFlexGridSizer(3, ui::pad() / 2, ui::pad());
			fg_sizer->AddGrowableCol(0, 1);
			fg_sizer->AddGrowableCol(1, 1);
			fg_sizer->AddGrowableCol(2, 1);
			sizer_trigger->Add(fg_sizer, wxutil::sfWithBorder().Expand());
			for (unsigned a = 0; a < game::configuration().nLineFlags(); a++)
			{
				if (game::configuration().lineFlag(a).activation)
				{
					flags_.push_back(
						{ new wxCheckBox(panel_action_special_, -1, game::configuration().lineFlag(a).name),
						  static_cast<int>(a),
						  game::configuration().lineFlag(a).udmf });
					fg_sizer->Add(flags_.back().check_box, 0, wxEXPAND);
				}
			}
		}

		// Preset button
		btn_preset_ = new wxButton(panel_action_special_, -1, "Preset...");
		sizer->Add(btn_preset_, wxutil::sfWithBorder(0, wxTOP).Right());
		btn_preset_->Bind(wxEVT_BUTTON, &ActionSpecialPanel::onSpecialPresetClicked, this);
	}

	panel_action_special_->SetSizerAndFit(sizer);
}

// -----------------------------------------------------------------------------
// Selects the item for special [special] in the specials tree
// -----------------------------------------------------------------------------
void ActionSpecialPanel::setSpecial(int special)
{
	// Check for boom generalised special
	if (game::configuration().featureSupported(game::Feature::Boom))
	{
		if (panel_gen_specials_->loadSpecial(special))
		{
			rb_generalised_->SetValue(true);
			showGeneralised(true);
			panel_gen_specials_->SetFocus();
			return;
		}
		else
			rb_special_->SetValue(true);
	}

	// Regular action special
	showGeneralised(false);
	tree_specials_->showSpecial(special, false);
	text_special_->SetValue(wxString::Format("%d", special));

	// Setup args if any
	if (panel_args_)
	{
		auto& args = game::configuration().actionSpecial(selectedSpecial()).argSpec();
		panel_args_->setup(args, (mapeditor::editContext().mapDesc().format == MapFormat::UDMF));
	}
}

// -----------------------------------------------------------------------------
// Sets the action special trigger (hexen or udmf)
// -----------------------------------------------------------------------------
void ActionSpecialPanel::setTrigger(int index) const
{
	if (!show_trigger_)
		return;

	// Hexen trigger
	if (choice_trigger_)
		choice_trigger_->SetSelection(index);

	// UDMF Trigger
	else if (index < static_cast<int>(flags_.size()))
		flags_[index].check_box->SetValue(true);
}

// -----------------------------------------------------------------------------
// Sets the action special trigger from a udmf trigger name (hexen or udmf)
// -----------------------------------------------------------------------------
void ActionSpecialPanel::setTrigger(const wxString& trigger) const
{
	if (!show_trigger_)
		return;

	// Hexen trigger
	if (choice_trigger_)
	{
		for (unsigned a = 0; a < choice_trigger_->GetCount(); a++)
			if (game::configuration().spacTriggerUDMFName(a) == trigger)
			{
				choice_trigger_->SetSelection(a);
				break;
			}
	}

	// UDMF Trigger or Hexen Flag
	for (auto& flag : flags_)
		if (flag.udmf == trigger)
		{
			flag.check_box->SetValue(true);
			break;
		}
}

// -----------------------------------------------------------------------------
// Deselects all triggers (or resets to 'player cross' in hexen format)
// -----------------------------------------------------------------------------
void ActionSpecialPanel::clearTrigger() const
{
	// UDMF Triggers and Flags
	for (auto& flag : flags_)
		flag.check_box->SetValue(false);

	// Hexen trigger
	if (choice_trigger_)
		choice_trigger_->SetSelection(0);
}

// -----------------------------------------------------------------------------
// Returns the currently selected action special
// -----------------------------------------------------------------------------
int ActionSpecialPanel::selectedSpecial() const
{
	if (game::configuration().featureSupported(game::Feature::Boom))
	{
		if (rb_special_->GetValue())
			return tree_specials_->selectedSpecial();
		else
			return panel_gen_specials_->special();
	}
	else
		return tree_specials_->selectedSpecial();
}

// -----------------------------------------------------------------------------
// If [show] is true, show the generalised special panel, otherwise show the
// action special tree
// -----------------------------------------------------------------------------
void ActionSpecialPanel::showGeneralised(bool show)
{
	if (!game::configuration().featureSupported(game::Feature::Boom))
		return;

	if (show)
	{
		auto sizer = GetSizer();
		sizer->Replace(panel_action_special_, panel_gen_specials_);
		panel_action_special_->Show(false);
		panel_gen_specials_->Show(true);
		Layout();
	}
	else
	{
		auto sizer = GetSizer();
		sizer->Replace(panel_gen_specials_, panel_action_special_);
		panel_action_special_->Show(true);
		panel_gen_specials_->Show(false);
		Layout();
	}
}

// -----------------------------------------------------------------------------
// Applies selected special (if [apply_special] is true), trigger(s) and args
// (if any) to [lines]
// -----------------------------------------------------------------------------
void ActionSpecialPanel::applyTo(const vector<MapObject*>& lines, bool apply_special) const
{
	// Special
	int special = selectedSpecial();
	if (apply_special && special >= 0)
	{
		for (auto& line : lines)
			line->setIntProperty("special", special);
	}

	// Args
	if (panel_args_)
	{
		// Get values
		int args[5];
		args[0] = panel_args_->argValue(0);
		args[1] = panel_args_->argValue(1);
		args[2] = panel_args_->argValue(2);
		args[3] = panel_args_->argValue(3);
		args[4] = panel_args_->argValue(4);

		for (auto& line : lines)
		{
			if (args[0] >= 0)
				line->setIntProperty("arg0", args[0]);
			if (args[1] >= 0)
				line->setIntProperty("arg1", args[1]);
			if (args[2] >= 0)
				line->setIntProperty("arg2", args[2]);
			if (args[3] >= 0)
				line->setIntProperty("arg3", args[3]);
			if (args[4] >= 0)
				line->setIntProperty("arg4", args[4]);
		}
	}

	// Trigger(s)
	if (show_trigger_)
	{
		for (auto line : lines)
		{
			// Hexen
			if (choice_trigger_)
				game::configuration().setLineSpacTrigger(choice_trigger_->GetSelection(), dynamic_cast<MapLine*>(line));

			// UDMF / Flags
			for (auto& flag : flags_)
			{
				if (flag.check_box->Get3StateValue() == wxCHK_UNDETERMINED)
					continue;

				if (choice_trigger_)
					game::configuration().setLineFlag(
						flag.index, dynamic_cast<MapLine*>(line), flag.check_box->GetValue());
				else
					line->setBoolProperty(flag.udmf.ToStdString(), flag.check_box->GetValue());
			}
		}
	}
}

// -----------------------------------------------------------------------------
// Loads special/trigger/arg values from [lines]
// -----------------------------------------------------------------------------
void ActionSpecialPanel::openLines(const vector<MapObject*>& lines)
{
	if (lines.empty())
		return;

	// Special
	int special = lines[0]->intProperty("special");
	MapObject::multiIntProperty(lines, "special", special);
	setSpecial(special);

	// Args
	if (panel_args_)
	{
		int args[5] = { -1, -1, -1, -1, -1 };
		MapObject::multiIntProperty(lines, "arg0", args[0]);
		MapObject::multiIntProperty(lines, "arg1", args[1]);
		MapObject::multiIntProperty(lines, "arg2", args[2]);
		MapObject::multiIntProperty(lines, "arg3", args[3]);
		MapObject::multiIntProperty(lines, "arg4", args[4]);
		panel_args_->setValues(args);
	}

	// Trigger
	if (show_trigger_)
	{
		// Hexen
		if (choice_trigger_)
		{
			int trigger = game::configuration().spacTriggerIndexHexen(dynamic_cast<MapLine*>(lines[0]));
			for (unsigned a = 1; a < lines.size(); a++)
			{
				if (trigger != game::configuration().spacTriggerIndexHexen(dynamic_cast<MapLine*>(lines[a])))
				{
					trigger = -1;
					break;
				}
			}

			if (trigger >= 0)
				choice_trigger_->SetSelection(trigger);

			// Flags
			for (auto& flag : flags_)
			{
				// Set initial flag checked value
				flag.check_box->SetValue(
					game::configuration().lineFlagSet(flag.index, dynamic_cast<MapLine*>(lines[0])));

				// Go through subsequent lines
				for (unsigned b = 1; b < lines.size(); b++)
				{
					// Check for mismatch
					if (flag.check_box->GetValue()
						!= game::configuration().lineFlagSet(flag.index, dynamic_cast<MapLine*>(lines[b])))
					{
						// Set undefined
						flag.check_box->Set3StateValue(wxCHK_UNDETERMINED);
						break;
					}
				}
			}
		}

		// UDMF
		else
		{
			for (auto& flag : flags_)
			{
				bool set;
				if (MapObject::multiBoolProperty(lines, flag.udmf.ToStdString(), set))
					flag.check_box->SetValue(set);
				else
					flag.check_box->Set3StateValue(wxCHK_UNDETERMINED);
			}
		}
	}
}


// -----------------------------------------------------------------------------
//
// ActionSpecialPanel Class Events
//
// -----------------------------------------------------------------------------

// ReSharper disable CppMemberFunctionMayBeConst
// ReSharper disable CppParameterMayBeConstPtrOrRef

// -----------------------------------------------------------------------------
// Called when the radio button selection is changed
// -----------------------------------------------------------------------------
void ActionSpecialPanel::onRadioButtonChanged(wxCommandEvent& e)
{
	// Swap panels
	showGeneralised(rb_generalised_->GetValue());
}

// -----------------------------------------------------------------------------
// Called when the action special selection is changed
// -----------------------------------------------------------------------------
void ActionSpecialPanel::onSpecialSelectionChanged(wxDataViewEvent& e)
{
	if ((game::configuration().featureSupported(game::Feature::Boom) && rb_generalised_->GetValue())
		|| selectedSpecial() < 0)
	{
		e.Skip();
		return;
	}

	// Set special # text box
	text_special_->SetValue(wxString::Format("%d", selectedSpecial()));

	if (panel_args_)
	{
		auto& args = game::configuration().actionSpecial(selectedSpecial()).argSpec();
		panel_args_->setup(args, (mapeditor::editContext().mapDesc().format == MapFormat::UDMF));
	}
}

// -----------------------------------------------------------------------------
// Called when the action special item is activated
// (double-clicked or enter pressed)
// -----------------------------------------------------------------------------
void ActionSpecialPanel::onSpecialItemActivated(wxDataViewEvent& e)
{
	if (tree_specials_->GetChildCount(e.GetItem()) > 0)
	{
		tree_specials_->Expand(e.GetItem());
		e.Skip();
		return;
	}

	// Jump to args tab, if there is one
	if (panel_args_)
	{
		auto& args = game::configuration().actionSpecial(selectedSpecial()).argSpec();
		panel_args_->setup(args, (mapeditor::editContext().mapDesc().format == MapFormat::UDMF));
		panel_args_->SetFocus();
	}
}

// -----------------------------------------------------------------------------
// Called when the special preset button is clicked
// -----------------------------------------------------------------------------
void ActionSpecialPanel::onSpecialPresetClicked(wxCommandEvent& e)
{
	// Open Special Preset Dialog
	SpecialPresetDialog dlg(this);
	dlg.CenterOnParent();
	if (dlg.ShowModal() == wxID_OK)
	{
		auto preset = dlg.selectedPreset();
		if (preset.special > 0)
		{
			// Set Special
			setSpecial(preset.special);

			// Set Args
			if (panel_args_)
				panel_args_->setValues(preset.args);

			// Set Flags
			clearTrigger();
			for (auto& flag : preset.flags)
				setTrigger(flag);
		}
	}
}
