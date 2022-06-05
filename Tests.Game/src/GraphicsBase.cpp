#include "stdafx.h"
#include "GraphicsBase.hpp"

GraphicsTest::~GraphicsTest()
{
	if (m_gl)
		delete m_gl;
	if (m_window)
		delete m_window;
}

bool GraphicsTest::Run()
{
	m_window = new Graphics::Window();

	m_gl = new OpenGL();
	if (!m_gl->Init(*m_window, 0))
	{
		delete m_gl;
		m_gl = nullptr;
		return false;
	}
	m_window->Show();

	Timer t;
	while (m_window->Update())
	{
		float dt = t.SecondsAsFloat();
		t.Restart();
		Render(dt);
	}

	return true;
}
