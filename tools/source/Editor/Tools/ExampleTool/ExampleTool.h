#pragma once

#include <ToolsInterface/ToolsInterface.h>

#include <memory>

//#include <Engine.h>
class GraphicsEngine;

class ExampleTool : public ToolsInterface
{
public:
	ExampleTool(std::shared_ptr<GraphicsEngine> aFramework);

	virtual void Draw();

private:
	std::shared_ptr<GraphicsEngine> myGraphicsEngine;
};