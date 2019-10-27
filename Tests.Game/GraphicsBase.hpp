#pragma once

#include <Graphics/Graphics.hpp>

class GraphicsTest : public Unique
{
public:
	~GraphicsTest();
	virtual void Render(float deltaTime) = 0;
	bool Run();

protected:
	Graphics::Window* m_window = nullptr;
	Graphics::OpenGL* m_gl = nullptr;
};