#pragma once
#include <Windows.h>
#include <Core/EngineSettings.h>

extern LRESULT CALLBACK WinProc(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam);

class WindowHandler
{
public:
	WindowHandler();
	virtual ~WindowHandler();

	bool Init(Settings::WindowData someWindowData);

	FORCEINLINE HWND GetWindowHandle() { return myWindowHandle; }
	FORCEINLINE float GetWidth() const { return myWidth; }
	FORCEINLINE float GetHeight() const { return myHeight; }

	void SetWidth(float aWidth) { myWidth = aWidth; }
	void setHeight(float aHeight) { myHeight = aHeight; }

private:
	HWND myWindowHandle;
	float myWidth;
	float myHeight;
};

