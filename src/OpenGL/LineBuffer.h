#pragma once

#include "OpenGL.h"

namespace slade::gl
{
class Shader;
class View;

class LineBuffer
{
public:
	struct Line
	{
		glm::vec4 v1_pos_width;
		glm::vec4 v1_colour;
		glm::vec4 v2_pos_width;
		glm::vec4 v2_colour;
	};

	LineBuffer() = default;

	~LineBuffer()
	{
		gl::deleteVBO(vbo_);
		gl::deleteVAO(vao_);
	}

	// Vector-like access to lines
	unsigned    size() const { return lines_.size(); }
	void        clear() { lines_.clear(); }
	bool        empty() const { return lines_.empty(); }
	const Line& operator[](unsigned index) const { return lines_[index]; }
	Line&       operator[](unsigned index)
	{
		lines_updated_ = true;
		return lines_[index];
	}

	float     widthMult() const { return width_mult_; }
	glm::vec2 aaRadius() const { return aa_radius_; }

	void setWidthMult(float width) { width_mult_ = width; }
	void setAaRadius(float x, float y) { aa_radius_ = { x, y }; }
	void setDashed(bool dashed, float dash_size = 6.0f, float gap_size = 6.0f)
	{
		dashed_        = dashed;
		dash_size_     = dash_size;
		dash_gap_size_ = gap_size;
	}

	void add(const Line& line);
	void add(const vector<Line>& lines);
	void add2d(float x1, float y1, float x2, float y2, glm::vec4 colour, float width = 1.0f)
	{
		add(Line{ { x1, y1, 0.0f, width }, colour, { x2, y2, 0.0f, width }, colour });
	}

	void draw(
		const View*      view   = nullptr,
		const glm::vec4& colour = glm::vec4{ 1.0f },
		const glm::mat4& model  = glm::mat4{ 1.0f }) const;

	static const Shader& shader();

private:
	float     width_mult_    = 1.0f;
	glm::vec2 aa_radius_     = { 2.0f, 2.0f };
	bool      dashed_        = false;
	float     dash_size_     = 6.0f;
	float     dash_gap_size_ = 6.0f;

	vector<Line>     lines_;
	mutable unsigned vao_           = 0;
	mutable unsigned vbo_           = 0;
	mutable bool     lines_updated_ = false;

	void initVAO() const;
	void updateVBO() const;
};
} // namespace slade::gl
