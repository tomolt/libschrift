#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <epoxy/gl.h>
#include <GLFW/glfw3.h>
#include <schrift.h>

#include "../include/utf8_to_utf32.h"

#define LENGTH(array) (sizeof(array)/sizeof*(array))

#define ATLAS_SIZE 512
#define VBO_SIZE 4096

static const char *vertex_shader_source[] = {
	"#version 330 core\n",
	//"uniform mat4 transform;",
	"in vec2 vert_pos;",
	"void main() {",
	"	gl_Position = vec4(vert_pos, 0.0f, 1.0f);",
	"}"
};

static const char *fragment_shader_source[] = {
	"#version 330 core\n",
	"out vec4 frag_color;",
	"void main() {",
	"	frag_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);",
	"}"
};

static GLFWwindow *window;
static GLuint shader;
static GLuint vao;
static GLuint vbo;
static GLuint atlas;
static SFT sft;

void die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

void init_gl(void)
{
	// Open a window & GL context
	if (!glfwInit())
		die("Can't init GLFW");
	window = glfwCreateWindow(640, 480, "libschrift GL demo", NULL, NULL);
	if (!window)
		die("Can't open window");
	glfwMakeContextCurrent(window);
	glClearColor(1.0f, 1.0f, 0.0f, 1.0f);

	// Compile shaders
	GLuint vert, frag;
	GLint status;
	vert = glCreateShader(GL_VERTEX_SHADER);
	frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(vert, LENGTH(vertex_shader_source), vertex_shader_source, NULL);
	glShaderSource(frag, LENGTH(fragment_shader_source), fragment_shader_source, NULL);
	glCompileShader(vert);
	glCompileShader(frag);
	glGetShaderiv(vert, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) die("Could not compile vertex shader");
	glGetShaderiv(frag, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE) die("Could not compile fragment shader");
	shader = glCreateProgram();
	glAttachShader(shader, vert);
	glAttachShader(shader, frag);
	glLinkProgram(shader);
	glGetProgramiv(shader, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) die("Could not link shader program");
	glDetachShader(shader, vert);
	glDetachShader(shader, frag);
	glDeleteShader(vert);
	glDeleteShader(frag);

	// Create VBO & VAO
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, VBO_SIZE, NULL, GL_STREAM_DRAW);
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	
	// Create font atlas texture
	glGenTextures(1, &atlas);
	glBindTexture(GL_TEXTURE_2D, atlas);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void draw_text(const char *text)
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, VBO_SIZE, NULL, GL_STREAM_DRAW); // Orphan old buffer contents
	float *coords = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	
	float *pointer = coords;
	size_t i;
	float penX = 0.0f;

	for (i = 0; text[i]; i++) {

		SFT_Glyph glyph;
		if (sft_lookup(&sft, text[i], &glyph) < 0)
			die("Can't look up glyph id");
		SFT_GMetrics gmtx;
		if (sft_gmetrics(&sft, glyph, &gmtx) < 0)
			die("Can't look up glyph metrics");

		float x1 = penX + gmtx.leftSideBearing / 320.f;
		float y1 = gmtx.yOffset / 240.f;
		float x2 = x1 + gmtx.minWidth / 320.f;
		float y2 = y1 + gmtx.minHeight / 240.f;

		penX += gmtx.advanceWidth / 320.f;

		float glyph_pos[] = {x1, y1, x1, y1, x2, y1, x1, y2, x2, y2, x2, y2};
		memcpy(pointer, glyph_pos, sizeof glyph_pos);
		pointer += LENGTH(glyph_pos);
	}

	glUnmapBuffer(GL_ARRAY_BUFFER);

	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 1, 6 * i - 2);
}

int main()
{
	init_gl();

	sft.font = sft_loadfile("../../resources/fonts/FiraGO-Regular.ttf");
	if (!sft.font)
		die("Can't load font");
	sft.xScale = 16;
	sft.yScale = 16;
	
	SFT_LMetrics lmtx;
	if (sft_lmetrics(&sft, &lmtx) < 0)
		die("Can't query line metrics");

	while (!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(shader);
		draw_text("Lorem ipsum dolor sit amet");
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	
	sft_freefont(sft.font);
	glfwTerminate();
	return 0;
}

