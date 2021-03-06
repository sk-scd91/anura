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
#ifndef SCROLLBAR_WIDGET_HPP_INCLUDED
#define SCROLLBAR_WIDGET_HPP_INCLUDED

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#include "widget.hpp"

namespace gui
{

class scrollbar_widget : public widget
{
public:
	explicit scrollbar_widget(boost::function<void(int)> handler);
	explicit scrollbar_widget(const variant& v, game_logic::formula_callable* e);

	void set_range(int total_height, int window_height);
	void set_loc(int x, int y);
	void set_dim(int w, int h);
	void set_window_pos(int pos) { window_pos_ = pos; }
	void set_step(int step) { step_ = step; }
	int window_pos() const { return window_pos_; }
protected:
	virtual void set_value(const std::string& key, const variant& v);
	virtual variant get_value(const std::string& key) const;
private:
	void handle_draw() const;
	bool handle_event(const SDL_Event& event, bool claimed);

	void down_button_pressed();
	void up_button_pressed();

	void clip_window_position();

	boost::function<void(int)> handler_;
	widget_ptr up_arrow_, down_arrow_, handle_, handle_bot_, handle_top_, background_;
	int window_pos_, window_size_, range_, step_;

	bool dragging_handle_;
	int drag_start_;
	int drag_anchor_y_;

	game_logic::formula_ptr ffl_handler_;
	void handler_delegate(int);
};

typedef boost::intrusive_ptr<scrollbar_widget> scrollbar_widget_ptr;

}

#endif
