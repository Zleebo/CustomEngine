#pragma once

#include <Windows.h>
#include <memory>

class GraphicsEngine;
class AudioEngine;

class Application
{
public:
	Application();
	virtual ~Application();

	void Run();

	virtual void BeginFrame();
	virtual void Render();
	virtual void EndFrame();

protected:
	std::shared_ptr<GraphicsEngine> myGraphicsEngine;
	float myDeltaTime = 0.0f;
};