#pragma once

#include <memory>
#include <array>

#include <Scene/Scene.h>
#include <GraphicsEngine/GraphicsFrameworks/DX11/DX11FullscreenTexture.h>
#include <GraphicsEngine/Terrain/TerrainRenderer.h>

class DX11Framework;
class ForwardRenderer;
class WindowHandler;
class ParticleRenderer;

class GraphicsEngine
{
public:
	GraphicsEngine();
	~GraphicsEngine();

	bool Init();
	Scene *GetScene() { return &myScene; }

	void BeginFrame();
	void EndFrame();
	void RenderFrame();
	void Update(float dt) { myTotalTime += dt; }

	__forceinline std::shared_ptr<WindowHandler> GetWindowHandler() const { return myWindowHandler; }
	__forceinline std::shared_ptr<DX11Framework> GetFramework() { return myFramework; }

	void Resize(uint32_t w, uint32_t h);

	void * GetRenderedImage() { return myRenderTexture.GetSRV(); }
	static int MouseOver(uint32_t x, uint32_t y);

	TerrainRenderer* GetTerrainRenderer() { return myTerrainRenderer.get(); }

	std::shared_ptr<WindowHandler> myWindowHandler;

	bool myIsRuntime = false;
private:
	std::shared_ptr<DX11Framework> myFramework;
	std::shared_ptr<ForwardRenderer> myForwardRenderer;
	std::unique_ptr<TerrainRenderer> myTerrainRenderer;

	DX11FullscreenTexture myBackbuffer;
	DX11FullscreenTexture myBackbufferDepth;
	static DX11FullscreenTexture myIDBuffer;

	DX11FullscreenTexture myRenderTexture;

	float myTotalTime = 0.f;
	Scene myScene;
};

