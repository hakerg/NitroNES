#pragma once
#include <cstdint>
#include <iostream>
#include <GL/glew.h>

#include "NESConst.h"

// Enkapsuluje inicjalizacje GLEW, teksture OpenGL i renderowanie klatki PPU.
// Nie zalezy od SDL ani windows.h – moze byc uzywany z dowolnym kontekstem GL.
class OpenGLRenderer {
public:
	OpenGLRenderer() = default;
	~OpenGLRenderer() { shutdown(); }

	OpenGLRenderer(const OpenGLRenderer&) = delete;
	OpenGLRenderer& operator=(const OpenGLRenderer&) = delete;

	// Musi byc wywolane po stworzeniu/aktywowaniu kontekstu GL.
	bool init() {
		glewExperimental = GL_TRUE;
		GLenum err = glewInit();
		if (err != GLEW_OK) {
			std::cerr << "[GL] glewInit nieudane: "
					  << (const char*)glewGetErrorString(err) << "\n";
			return false;
		}

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, NES::SCREEN_WIDTH, NES::SCREEN_HEIGHT, 0,
			GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, nullptr);
		return true;
	}

	void shutdown() {
		if (texture) { glDeleteTextures(1, &texture); texture = 0; }
	}

	// Uploaduje framebuffer PPU i renderuje go na caly viewport (winW x winH).
	// fb musi wskazywac na tablice NES::SCREEN_WIDTH * NES::SCREEN_HEIGHT pikseli BGRA uint32_t.
	void renderFrame(const uint32_t* fb, int winW, int winH) {
		if (!texture || winW <= 0 || winH <= 0) return;

		// Upload pikseli.
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, NES::SCREEN_WIDTH);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, NES::SCREEN_WIDTH, NES::SCREEN_HEIGHT,
			GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, fb);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		// Czarne tlo.
		glViewport(0, 0, winW, winH);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// Cel: zachowanie PAR i overscan.
		const float targetAspect =
			(float)(NES::SCREEN_WIDTH * NES::PAR_NUM) / (float)(NES::VISIBLE_H * NES::PAR_DEN);
		float dstW = (float)winW;
		float dstH = dstW / targetAspect;
		if (dstH > (float)winH) { dstH = (float)winH; dstW = dstH * targetAspect; }
		float dstX = ((float)winW - dstW) * 0.5f;
		float dstY = ((float)winH - dstH) * 0.5f;

		// NDC.
		float x0 =  (dstX            / (float)winW) * 2.0f - 1.0f;
		float x1 = ((dstX + dstW)    / (float)winW) * 2.0f - 1.0f;
		float y1 = 1.0f - ( dstY            / (float)winH) * 2.0f;
		float y0 = 1.0f - ((dstY + dstH)    / (float)winH) * 2.0f;

		// Wspolrzedne tekstury: pomijamy OVERSCAN_TOP/BOTTOM.
		float u0 = 0.0f;
		float u1 = 1.0f;
		float v0 = (float)NES::OVERSCAN_TOP / (float)NES::SCREEN_HEIGHT;
		float v1 = (float)(NES::OVERSCAN_TOP + NES::VISIBLE_H) / (float)NES::SCREEN_HEIGHT;

		glMatrixMode(GL_PROJECTION); glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);  glLoadIdentity();

		glEnable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		glBegin(GL_TRIANGLE_STRIP);
			glTexCoord2f(u0, v1); glVertex2f(x0, y0);
			glTexCoord2f(u1, v1); glVertex2f(x1, y0);
			glTexCoord2f(u0, v0); glVertex2f(x0, y1);
			glTexCoord2f(u1, v0); glVertex2f(x1, y1);
		glEnd();

		glDisable(GL_TEXTURE_2D);

		// Single buffer: glFlush wymusza dotarcie polecen do front buffera.
		glFlush();
	}

private:
	GLuint texture = 0;
};
