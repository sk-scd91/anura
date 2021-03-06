/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>
	
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef EDITOR_LAYERS_DIALOG_HPP_INCLUDED
#define EDITOR_LAYERS_DIALOG_HPP_INCLUDED
#ifndef NO_EDITOR

#include <vector>

#include "dialog.hpp"
#include "editor.hpp"
#include "image_widget.hpp"

namespace editor_dialogs
{

static const int LAYERS_DIALOG_WIDTH = 40;

class editor_layers_dialog : public gui::dialog
{
public:
	explicit editor_layers_dialog(editor& e);
	void init();
	void process();
private:
	void row_selected(int nrow);
	void row_mouseover(int nrow);
	editor& editor_;

	struct row_data {
		gui::gui_section_widget_ptr checkbox;
		int layer;
		bool hidden;
	};

	std::vector<row_data> rows_;

	bool locked_;

	std::set<int> before_locked_state_;

	void find_classifications();
	std::set<std::string> all_classifications_;

	void classification_selected(int index);
};

typedef boost::intrusive_ptr<editor_layers_dialog> editor_layers_dialog_ptr;

}

#endif // !NO_EDITOR
#endif
