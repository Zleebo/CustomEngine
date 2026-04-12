#pragma once
#include <ToolsInterface/ToolsInterface.h>
#include <memory>

class GraphicsEngine;

class TerrainTool : public ToolsInterface
{
public:
	TerrainTool(GraphicsEngine* aEngine);
	void Draw() override;

private:
	GraphicsEngine* myEngine;
};
