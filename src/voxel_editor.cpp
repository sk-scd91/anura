#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include <assert.h>

#include <algorithm>
#include <numeric>
#include <map>
#include <vector>

#include "SDL.h"

#include "button.hpp"
#include "camera.hpp"
#include "color_utils.hpp"
#include "dialog.hpp"
#include "formatter.hpp"
#include "gles2.hpp"
#include "grid_widget.hpp"
#include "label.hpp"
#include "level_runner.hpp"
#include "preferences.hpp"
#include "unit_test.hpp"

#ifdef USE_GLES2

#define EXT_CALL(call) call
#define EXT_MACRO(macro) macro

namespace {
typedef boost::array<int, 3> VoxelPos;
struct Voxel {
	graphics::color color;
};

typedef std::map<VoxelPos, Voxel> VoxelMap;
typedef std::pair<VoxelPos, Voxel> VoxelPair;

struct Layer {
	std::string name;
	VoxelMap map;
};

class voxel_editor : public gui::dialog
{
public:
	explicit voxel_editor(const rect& r);
	~voxel_editor();
	void init();

	const VoxelMap& voxels() const { return voxels_; }

	void set_voxel(const VoxelPos& pos, const Voxel& voxel);
	bool set_cursor(const VoxelPos& pos);

	const VoxelPos* get_cursor() const { return cursor_.get(); }

	VoxelPos get_selected_voxel(const VoxelPos& pos, int facing, bool reverse);
private:
	bool handle_event(const SDL_Event& event, bool claimed);

	Layer& layer() { return layers_[current_layer_]; }
	const Layer& layer() const { return layers_[current_layer_]; }

	void build_voxels();

	rect area_;

	int current_layer_;
	std::vector<Layer> layers_;
	VoxelMap voxels_;

	boost::scoped_ptr<VoxelPos> cursor_;

	gui::label_ptr pos_label_;
};

voxel_editor* g_voxel_editor;

voxel_editor& get_editor() {
	assert(g_voxel_editor);
	return *g_voxel_editor;
}

using namespace gui;

class iso_renderer : public gui::widget
{
public:
	explicit iso_renderer(const rect& area);
	void handle_draw() const;
private:
	void handle_process();
	bool handle_event(const SDL_Event& event, bool claimed);

	void render_fbo();
	boost::intrusive_ptr<camera_callable> camera_;
	GLfloat camera_hangle_, camera_vangle_, camera_distance_;

	void calculate_camera();

	graphics::texture fbo_;

	boost::array<GLfloat, 3> vector_;

};

iso_renderer::iso_renderer(const rect& area)
  : camera_(new camera_callable),
    camera_hangle_(0.12), camera_vangle_(1.25), camera_distance_(20.0)
{
	set_loc(area.x(), area.y());
	set_dim(area.w(), area.h());
	vector_[0] = 1.0;
	vector_[1] = 1.0;
	vector_[2] = 1.0;

	calculate_camera();
}

void iso_renderer::calculate_camera()
{
	const GLfloat hdist = sin(camera_vangle_)*camera_distance_;
	const GLfloat ydist = cos(camera_vangle_)*camera_distance_;

	const GLfloat xdist = sin(camera_hangle_)*hdist;
	const GLfloat zdist = cos(camera_hangle_)*hdist;

	std::cerr << "LOOK AT: " << xdist << ", " << ydist << ", " << zdist << "\n";

	camera_->look_at(glm::vec3(xdist, ydist, zdist), glm::vec3(0,0,0), glm::vec3(0.0, 1.0, 0.0));
}

void iso_renderer::handle_draw() const
{
	gles2::manager gles2_manager(gles2::get_tex_shader());

	//we get the fbo upside down, so flip it when blitting.
	graphics::blit_texture(fbo_, x(), y(), width(), -height(), 0.0, 0.0, 0.0, 1.0, 1.0);
}

void iso_renderer::handle_process()
{
	render_fbo();
}

bool iso_renderer::handle_event(const SDL_Event& event, bool claimed)
{
	switch(event.type) {
	case SDL_MOUSEMOTION: {
		const SDL_MouseMotionEvent& motion = event.motion;
		if(motion.x >= x() && motion.y >= y() &&
		   motion.x <= x() + width() && motion.y <= y() + height()) {
			Uint8 button_state = SDL_GetMouseState(NULL, NULL);
			if(button_state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
				if(motion.xrel) {
					camera_hangle_ += motion.xrel*0.02;
				}

				if(motion.yrel) {
					camera_vangle_ += motion.yrel*0.02;
				}

				std::cerr << "ANGLE: " << camera_hangle_ << ", " << camera_vangle_ << "\n";

				calculate_camera();
			}
		}
		break;
	}
	}

	return widget::handle_event(event, claimed);
}

void iso_renderer::render_fbo()
{
	const rect area(0, 0, width(), height());

	const int tex_width = graphics::texture::allows_npot() ? area.w() : graphics::texture::next_power_of_2(area.w());
	const int tex_height = graphics::texture::allows_npot() ? area.h() : graphics::texture::next_power_of_2(area.h());
	GLint video_framebuffer_id = 0;
	glGetIntegerv(EXT_MACRO(GL_FRAMEBUFFER_BINDING), &video_framebuffer_id);

	GLuint texture_id = 0, depth_id = 0;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glGenRenderbuffers(1, &depth_id);
	glBindRenderbuffer(GL_RENDERBUFFER, depth_id);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, tex_width, tex_height);


	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	GLuint framebuffer_id = 0;
	EXT_CALL(glGenFramebuffers)(1, &framebuffer_id);
	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), framebuffer_id);

	// attach the texture to FBO color attachment point
	EXT_CALL(glFramebufferTexture2D)(EXT_MACRO(GL_FRAMEBUFFER), EXT_MACRO(GL_COLOR_ATTACHMENT0),
                          GL_TEXTURE_2D, texture_id, 0);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_id);
	

	// check FBO status
	GLenum status = EXT_CALL(glCheckFramebufferStatus)(EXT_MACRO(GL_FRAMEBUFFER));
	ASSERT_NE(status, EXT_MACRO(GL_FRAMEBUFFER_UNSUPPORTED));
	ASSERT_EQ(status, EXT_MACRO(GL_FRAMEBUFFER_COMPLETE));

	//set up the raster projection.
	glViewport(0, 0, area.w(), area.h());
	glEnable(GL_LIGHTING);
	glShadeModel(GL_SMOOTH);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glColor4f(1.0, 1.0, 1.0, 1.0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);

	//start drawing here.
	gles2::shader_program_ptr shader_program(gles2::shader_program::get_global("iso_color_line"));
	gles2::program_ptr shader = shader_program->shader();
	gles2::actives_map_iterator mvp_uniform_itor = shader->get_uniform_reference("mvp_matrix");

	gles2::manager gles2_manager(shader_program);

	glm::mat4 model_matrix(1.0f);

	glm::mat4 mvp = camera_->projection_mat() * camera_->view_mat() * model_matrix;

	shader->set_uniform(mvp_uniform_itor, 1, glm::value_ptr(mvp));

	std::vector<GLfloat> varray, carray;

	const GLfloat axes_vertex[] = {
		0.0, 0.0, 0.0,
		0.0, 0.0, 10.0,
		0.0, 0.0, 0.0,
		0.0, 10.0, 0.0,
		0.0, 0.0, 0.0,
		10.0, 0.0, 0.0,
	};

	for(int n = 0; n != sizeof(axes_vertex)/sizeof(*axes_vertex); ++n) {
		varray.push_back(axes_vertex[n]);
		if(n%3 == 0) {
			carray.push_back(1.0);
			carray.push_back(1.0);
			carray.push_back(1.0);
			carray.push_back(1.0);
		}
	}

	if(get_editor().get_cursor()) {
		const VoxelPos& cursor = *get_editor().get_cursor();
		const GLfloat cursor_vertex[] = {
			cursor[0], cursor[1], cursor[2],
			cursor[0]+1.0, cursor[1], cursor[2],
			cursor[0]+1.0, cursor[1], cursor[2],
			cursor[0]+1.0, cursor[1]+1.0, cursor[2],
			cursor[0]+1.0, cursor[1]+1.0, cursor[2],
			cursor[0], cursor[1]+1.0, cursor[2],
			cursor[0], cursor[1]+1.0, cursor[2],
			cursor[0], cursor[1], cursor[2],

			cursor[0], cursor[1], cursor[2]+1.0,
			cursor[0]+1.0, cursor[1], cursor[2]+1.0,
			cursor[0]+1.0, cursor[1], cursor[2]+1.0,
			cursor[0]+1.0, cursor[1]+1.0, cursor[2]+1.0,
			cursor[0]+1.0, cursor[1]+1.0, cursor[2]+1.0,
			cursor[0], cursor[1]+1.0, cursor[2]+1.0,
			cursor[0], cursor[1]+1.0, cursor[2]+1.0,
			cursor[0], cursor[1], cursor[2]+1.0,

			cursor[0], cursor[1], cursor[2],
			cursor[0], cursor[1], cursor[2]+1.0,
			cursor[0]+1.0, cursor[1], cursor[2],
			cursor[0]+1.0, cursor[1], cursor[2]+1.0,
			cursor[0]+1.0, cursor[1]+1.0, cursor[2],
			cursor[0]+1.0, cursor[1]+1.0, cursor[2]+1.0,
			cursor[0], cursor[1]+1.0, cursor[2],
			cursor[0], cursor[1]+1.0, cursor[2]+1.0,
		};

		for(int n = 0; n != sizeof(cursor_vertex)/sizeof(*cursor_vertex); ++n) {
			varray.push_back(cursor_vertex[n]);
			if(n%3 == 0) {
				carray.push_back(1.0);
				carray.push_back(1.0);
				carray.push_back(0.0);
				carray.push_back(1.0);
			}
		}
	}

	gles2::active_shader()->shader()->vertex_array(3, GL_FLOAT, 0, 0, &varray[0]);
	gles2::active_shader()->shader()->color_array(4, GL_FLOAT, 0, 0, &carray[0]);
	glDrawArrays(GL_LINES, 0, varray.size()/3);

	varray.clear();
	carray.clear();

	for(const VoxelPair& p : get_editor().voxels()) {
		const VoxelPos& pos = p.first;

		const GLfloat vertex[] = {
			0, 0, 0,
			1, 0, 0,
			1, 1, 0,

			0, 0, 0,
			0, 1, 0,
			1, 1, 0,

			0, 0, 1,
			1, 0, 1,
			1, 1, 1,

			0, 0, 1,
			0, 1, 1,
			1, 1, 1,

			0, 0, 0,
			0, 1, 0,
			0, 1, 1,

			0, 0, 0,
			0, 0, 1,
			0, 1, 1,

			1, 0, 0,
			1, 1, 0,
			1, 1, 1,

			1, 0, 0,
			1, 0, 1,
			1, 1, 1,

			0, 0, 0,
			1, 0, 0,
			1, 0, 1,

			0, 0, 0,
			0, 0, 1,
			1, 0, 1,

			0, 1, 0,
			1, 1, 0,
			1, 1, 1,

			0, 1, 0,
			0, 1, 1,
			1, 1, 1,
		};

		graphics::color color = p.second.color;
		const bool is_selected = get_editor().get_cursor() && *get_editor().get_cursor() == pos;
		if(is_selected) {
			const int delta = sin(SDL_GetTicks()*0.01)*64;
			graphics::color_transform transform(delta, delta, delta, 0);
			graphics::color_transform new_color = graphics::color_transform(color) + transform;
			color = new_color.to_color();
		}

		int face = 0;

		for(int n = 0; n != sizeof(vertex)/sizeof(*vertex); ++n) {
			varray.push_back(pos[n%3]+vertex[n]);
			if(n%3 == 0) {
				//our shoddy way of doing shading right now
				//until Kristina does it properly.
				GLfloat mul = 1.0;
				switch(face/6) {
				case 0: mul = 1.0; break;
				case 1: mul = 1.0; break;
				case 2: mul = 0.8; break;
				case 3: mul = 0.8; break;
				case 4: mul = 0.6; break;
				case 5: mul = 0.6; break;
				}

				carray.push_back(mul*color.r()/255.0);
				carray.push_back(mul*color.g()/255.0);
				carray.push_back(mul*color.b()/255.0);
				carray.push_back(color.a()/255.0);
				++face;
			}
		}
	}

	if(!varray.empty()) {
		gles2::active_shader()->shader()->vertex_array(3, GL_FLOAT, 0, 0, &varray[0]);
		gles2::active_shader()->shader()->color_array(4, GL_FLOAT, 0, 0, &carray[0]);
		glDrawArrays(GL_TRIANGLES, 0, varray.size()/3);
	}

	EXT_CALL(glBindFramebuffer)(EXT_MACRO(GL_FRAMEBUFFER), video_framebuffer_id);

	glViewport(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height());

	fbo_ = graphics::texture(texture_id, tex_width, tex_height);
	glDisable(GL_DEPTH_TEST);

	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glDeleteRenderbuffers(1, &depth_id);

	glDeleteFramebuffers(1, &framebuffer_id);
}

class perspective_renderer : public gui::widget
{
public:
	perspective_renderer(int xdir, int ydir, int zdir);
	void handle_draw() const;

	void zoom_in();
	void zoom_out();

	//converts given pos to [x,y,0]
	VoxelPos normalize_pos(const VoxelPos& pos) const;
private:
	VoxelPos get_mouse_pos(int mousex, int mousey) const;
	bool handle_event(const SDL_Event& event, bool claimed);
	bool calculate_cursor(int mousex, int mousey);
	void pencil_voxel();

	bool is_flipped() const { return vector_[0] + vector_[1] + vector_[2] < 0; }
	int vector_[3];
	int facing_; //0=x, 1=y, 2=z
	int voxel_width_;

	int last_select_x_, last_select_y_;

	int invert_y_;

	bool drawing_on_;
};

perspective_renderer::perspective_renderer(int xdir, int ydir, int zdir)
  : voxel_width_(20), last_select_x_(INT_MIN), last_select_y_(INT_MIN),
    invert_y_(1), drawing_on_(false)
{
	vector_[0] = xdir;
	vector_[1] = ydir;
	vector_[2] = zdir;

	for(int n = 0; n != 3; ++n) {
		if(vector_[n]) {
			facing_ = n;
			break;
		}
	}

	if(facing_ != 1) {
		invert_y_ *= -1;
	}
};

void perspective_renderer::zoom_in()
{
	if(voxel_width_ < 80) {
		voxel_width_ *= 2;
	}
}

void perspective_renderer::zoom_out()
{
	if(voxel_width_ > 5) {
		voxel_width_ /= 2;
	}
}

VoxelPos perspective_renderer::normalize_pos(const VoxelPos& pos) const
{
	VoxelPos result;
	result[2] = 0;
	int* out = &result[0];

	int dimensions[3] = {0, 2, 1};
	for(int n = 0; n != 3; ++n) {
		if(dimensions[n] != facing_) {
			*out++ = pos[dimensions[n]];
		}
	}

	return result;
}

VoxelPos perspective_renderer::get_mouse_pos(int mousex, int mousey) const
{
	int xpos = mousex - (x() + width()/2);
	int ypos = mousey - (y() + height()/2);

	if(xpos < 0) {
		xpos -= voxel_width_;
	}

	if(ypos > 0) {
		ypos += voxel_width_;
	}

	const int xselect = xpos/voxel_width_;
	const int yselect = ypos/voxel_width_;
	VoxelPos result;
	result[0] = xselect;
	result[1] = yselect*invert_y_;
	result[2] = 0;
	return result;
}

void perspective_renderer::pencil_voxel()
{
	if(get_editor().get_cursor()) {
		Voxel voxel = { graphics::color(SDL_GetModState()&KMOD_SHIFT ? "blue" : "green") };
		get_editor().set_voxel(*get_editor().get_cursor(), voxel);
	}
}

bool perspective_renderer::calculate_cursor(int mousex, int mousey)
{
	if(mousex == INT_MIN) {
		return false;
	}

	const VoxelPos pos2d = get_mouse_pos(mousex, mousey);

	const int* p = &pos2d[0];

	VoxelPos pos;
	int dimensions[3] = {0,2,1};
	for(int n = 0; n != 3; ++n) {
		if(dimensions[n] != facing_) {
			pos[dimensions[n]] = *p++;
		} else {
			pos[dimensions[n]] = 0;
		}
	}

	VoxelPos cursor = get_editor().get_selected_voxel(pos, facing_, vector_[facing_] < 0);
	if(SDL_GetModState()&KMOD_CTRL && get_editor().voxels().count(cursor)) {
		for(int n = 0; n != 3; ++n) {
			cursor[n] += vector_[n];
		}
	}

	return get_editor().set_cursor(cursor);

}

bool perspective_renderer::handle_event(const SDL_Event& event, bool claimed)
{
	switch(event.type) {
	case SDL_KEYUP:
	case SDL_KEYDOWN: {
		calculate_cursor(last_select_x_, last_select_y_);
		break;
	}

	case SDL_MOUSEBUTTONUP: {
		drawing_on_ = false;
		break;
	}

	case SDL_MOUSEBUTTONDOWN: {
		const SDL_MouseButtonEvent& e = event.button;
		if(e.x >= x() && e.y >= y() &&
		   e.x <= x() + width() && e.y <= y() + height()) {
			pencil_voxel();
			calculate_cursor(last_select_x_, last_select_y_);

			drawing_on_ = true;
		} else {
			drawing_on_ = false;
		}
		break;
	}

	case SDL_MOUSEMOTION: {
		const SDL_MouseMotionEvent& motion = event.motion;
		if(motion.x >= x() && motion.y >= y() &&
		   motion.x <= x() + width() && motion.y <= y() + height()) {
			const bool is_cursor_set = calculate_cursor(motion.x, motion.y);
			last_select_x_ = motion.x;
			last_select_y_ = motion.y;

			if(is_cursor_set) {
				Uint8 button_state = SDL_GetMouseState(NULL, NULL);
				if(button_state & SDL_BUTTON(SDL_BUTTON_LEFT) && drawing_on_) {
					pencil_voxel();
					calculate_cursor(motion.x, motion.y);
				}
			}

			break;
		} else {
			last_select_x_ = last_select_y_ = INT_MIN;
		}
	}
	}
	return widget::handle_event(event, claimed);
}

void perspective_renderer::handle_draw() const
{
	const SDL_Rect clip_area = { x(), y(), width(), height() };
	const graphics::clip_scope clipping_scope(clip_area);

	gles2::manager gles2_manager(gles2::get_simple_col_shader());

	std::vector<GLfloat> varray, carray;

	const int cells_h = width()/voxel_width_ + 1;
	const int cells_v = height()/voxel_width_ + 1;
	for(int xpos = -cells_h/2; xpos <= cells_h/2; ++xpos) {
		const int left_side = x() + width()/2 + xpos*voxel_width_;
		if(left_side < x() || left_side + voxel_width_ > x() + width()) {
			continue;
		}

		varray.push_back(left_side);
		varray.push_back(y());
		varray.push_back(left_side);
		varray.push_back(y() + height());

		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(xpos == 0 ? 1.0 : 0.3);

		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(xpos == 0 ? 1.0 : 0.3);
	}

	for(int ypos = -cells_v/2; ypos <= cells_v/2; ++ypos) {
		const int top_side = y() + height()/2 + ypos*voxel_width_;
		if(top_side < y() || top_side + voxel_width_ > y() + height()) {
			continue;
		}

		varray.push_back(x());
		varray.push_back(top_side);
		varray.push_back(x() + width());
		varray.push_back(top_side);

		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(ypos == 0 ? 1.0 : 0.3);

		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(1.0);
		carray.push_back(ypos == 0 ? 1.0 : 0.3);
	}

	if(get_editor().get_cursor()) {
		const VoxelPos cursor = normalize_pos(*get_editor().get_cursor());

		const int x1 = x() + width()/2 + cursor[0]*voxel_width_;
		const int y1 = y() + height()/2 + cursor[1]*voxel_width_*invert_y_;

		const int x2 = x1 + voxel_width_;
		const int y2 = y1 - voxel_width_;

		int vertexes[] = { x1, y1, x1, y2,
		                   x2, y1, x2, y2,
						   x1, y1, x2, y1,
						   x1, y2, x2, y2, };
		for(int n = 0; n != sizeof(vertexes)/sizeof(*vertexes); ++n) {
			varray.push_back(vertexes[n]);
			if(n%2 == 0) {
				carray.push_back(1.0);
				carray.push_back(0.0);
				carray.push_back(0.0);
				carray.push_back(1.0);
			}
		}
	}

	gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray[0]);
	gles2::active_shader()->shader()->color_array(4, GL_FLOAT, 0, 0, &carray[0]);
	glDrawArrays(GL_LINES, 0, varray.size()/2);

	varray.clear();
	carray.clear();

	std::vector<VoxelPair> voxels(get_editor().voxels().begin(), get_editor().voxels().end());
	if(is_flipped()) {
		std::reverse(voxels.begin(), voxels.end());
	}

	for(const VoxelPair& p : voxels) {
		const VoxelPos pos = normalize_pos(p.first);

		const int x1 = x() + width()/2 + pos[0]*voxel_width_;
		const int y1 = y() + height()/2 + pos[1]*voxel_width_*invert_y_;

		const int x2 = x1 + voxel_width_;
		const int y2 = y1 - voxel_width_;

		int vertexes[] = { x1, y1,
		                   x1, y1, x1, y2,
		                   x2, y1, x2, y2,
						   x1, y1, x2, y1,
						   x1, y2, x2, y2,
						   x2, y2, };
		for(int n = 0; n != sizeof(vertexes)/sizeof(*vertexes); ++n) {
			varray.push_back(vertexes[n]);
			if(n%2 == 0) {
				carray.push_back(p.second.color.r()/255.0);
				carray.push_back(p.second.color.g()/255.0);
				carray.push_back(p.second.color.b()/255.0);
				carray.push_back(p.second.color.a()/255.0);
			}
		}
	}

	gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray[0]);
	gles2::active_shader()->shader()->color_array(4, GL_FLOAT, 0, 0, &carray[0]);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, varray.size()/2);

	varray.clear();
	carray.clear();

	//When voxels are adjacent but different height to each other from our
	//perspective, we represent the height difference by drawing black lines
	//between the voxels.
	for(const VoxelPair& p : voxels) {
		const VoxelPos pos = normalize_pos(p.first);

		const int x1 = x() + width()/2 + pos[0]*voxel_width_;
		const int y1 = y() + height()/2 + pos[1]*voxel_width_*invert_y_;

		const int x2 = x1 + voxel_width_;
		const int y2 = y1 - voxel_width_;

		VoxelPos actual_pos = get_editor().get_selected_voxel(p.first, facing_, vector_[facing_] < 0);
		if(actual_pos != p.first) {
			continue;
		}

		VoxelPos down = p.first;
		VoxelPos right = p.first;

		switch(facing_) {
			case 0: down[1]--; right[2]++; break;
			case 1: down[2]++; right[0]++; break;
			case 2: down[1]--; right[0]++; break;
		}

		if(get_editor().get_selected_voxel(down, facing_, vector_[facing_] < 0) != down) {
			varray.push_back(x1);
			varray.push_back(y1);
			varray.push_back(x2);
			varray.push_back(y1);

			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(1);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(1);
		}

		if(get_editor().get_selected_voxel(right, facing_, vector_[facing_] < 0) != right) {
			varray.push_back(x2);
			varray.push_back(y1);
			varray.push_back(x2);
			varray.push_back(y2);

			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(1);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(0);
			carray.push_back(1);
		}
	}

	gles2::active_shader()->shader()->vertex_array(2, GL_FLOAT, 0, 0, &varray[0]);
	gles2::active_shader()->shader()->color_array(4, GL_FLOAT, 0, 0, &carray[0]);
	glDrawArrays(GL_LINES, 0, varray.size()/2);
}

class perspective_widget : public gui::dialog
{
public:
	perspective_widget(const rect& area, int xdir, int ydir, int zdir);
	void init();
private:
	void flip();
	int xdir_, ydir_, zdir_;
	bool flipped_;

	boost::intrusive_ptr<perspective_renderer> renderer_;
	gui::label_ptr description_label_;
};

perspective_widget::perspective_widget(const rect& area, int xdir, int ydir, int zdir)
  : dialog(area.x(), area.y(), area.w(), area.h()),
    xdir_(xdir), ydir_(ydir), zdir_(zdir), flipped_(false)
{
	init();
}

void perspective_widget::init()
{
	clear();

	renderer_.reset(new perspective_renderer(xdir_, ydir_, zdir_));

	grid_ptr toolbar(new grid(4));

	std::string description;
	if(xdir_) { description = flipped_ ? "Reverse" : "Side"; }
	else if(ydir_) { description = flipped_ ? "Bottom" : "Top"; }
	else if(zdir_) { description = flipped_ ? "Back" : "Front"; }

	description_label_.reset(new label(description, 12));
	toolbar->add_col(description_label_);
	toolbar->add_col(new button("Flip", boost::bind(&perspective_widget::flip, this)));
	toolbar->add_col(new button("+", boost::bind(&perspective_renderer::zoom_in, renderer_.get())));
	toolbar->add_col(new button("-", boost::bind(&perspective_renderer::zoom_out, renderer_.get())));
	add_widget(toolbar);

	add_widget(renderer_);
	renderer_->set_dim(width(), height() - renderer_->y());
};

void perspective_widget::flip()
{
	flipped_ = !flipped_;
	xdir_ *= -1;
	ydir_ *= -1;
	zdir_ *= -1;
	init();
}

voxel_editor::voxel_editor(const rect& r)
  : dialog(r.x(), r.y(), r.w(), r.h()), area_(r), current_layer_(0)
{
	layers_.push_back(Layer());

	g_voxel_editor = this;
	init();
}

voxel_editor::~voxel_editor()
{
	if(g_voxel_editor == this) {
		g_voxel_editor = NULL;
	}
}

void voxel_editor::init()
{
	clear();

	const int sidebar_padding = 200;
	const int between_padding = 10;
	const int widget_width = (area_.w() - sidebar_padding - between_padding)/2;
	const int widget_height = (area_.h() - between_padding)/2;
	widget_ptr w;

	w.reset(new perspective_widget(rect(area_.x(), area_.y(), widget_width, widget_height), 1, 0, 0));
	add_widget(w, w->x(), w->y());

	w.reset(new perspective_widget(rect(area_.x() + widget_width + between_padding, area_.y(), widget_width, widget_height), 0, 1, 0));
	add_widget(w, w->x(), w->y());

	w.reset(new perspective_widget(rect(area_.x(), area_.y() + widget_height + between_padding, widget_width, widget_height), 0, 0, 1));
	add_widget(w, w->x(), w->y());

	w.reset(new iso_renderer(rect(area_.x() + widget_width + between_padding, area_.y() + widget_height + between_padding, widget_width, widget_height)));
	add_widget(w, w->x(), w->y());

	pos_label_.reset(new label("", 12));
	add_widget(pos_label_, area_.x() + area_.w() - pos_label_->width() - 100,
	                       area_.y() + area_.h() - pos_label_->height() - 30 );
}

void voxel_editor::set_voxel(const VoxelPos& pos, const Voxel& voxel)
{
	layer().map[pos] = voxel;
	build_voxels();
}

bool voxel_editor::set_cursor(const VoxelPos& pos)
{
	if(cursor_ && *cursor_ == pos) {
		return false;
	}

	cursor_.reset(new VoxelPos(pos));
	if(pos_label_) {
		pos_label_->set_text(formatter() << "(" << pos[0] << "," << pos[1] << "," << pos[2] << ")");
		pos_label_->set_loc(area_.x() + area_.w() - pos_label_->width() - 8,
		                    area_.y() + area_.h() - pos_label_->height() - 4);
	}

	return true;
}

VoxelPos voxel_editor::get_selected_voxel(const VoxelPos& pos, int facing, bool reverse)
{
	const int flip = reverse ? -1 : 1;
	VoxelPos result = pos;
	bool found = false;
	int best_value = 0;
	for(const VoxelPair& p : voxels_) {
		bool is_equal = true;
		for(int n = 0; n != 3; ++n) {
			if(n != facing && pos[n] != p.first[n]) {
				is_equal = false;
				break;
			}
		}

		if(!is_equal) {
			continue;
		}

		const int value = flip*p.first[facing];
		if(found == false || value >= best_value) {
			best_value = value;
			result = p.first;
			found = true;
		}
	}

	return result;
}

bool voxel_editor::handle_event(const SDL_Event& event, bool claimed)
{
	if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
		video_resize(event);
		set_dim(preferences::actual_screen_width(), preferences::actual_screen_height());
		init();
		return true;
	}
	return dialog::handle_event(event, claimed);
}

void voxel_editor::build_voxels()
{
	voxels_.clear();
	for(const Layer& layer : layers_) {
		for(const VoxelPair& p : layer.map) {
			voxels_.insert(p);
		}
	}
}

}

UTILITY(voxel_editor)
{
	boost::intrusive_ptr<voxel_editor> editor(new voxel_editor(rect(0, 0, preferences::actual_screen_width(), preferences::actual_screen_height())));
	editor->show_modal();
}

#endif //USE_GLES2