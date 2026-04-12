#pragma once
#include <Windows.h>

class InputManager
{
public:
	static InputManager& Get();

	void Update();

	bool IsKeyDown(int aVirtualKey) const;
	bool IsKeyPressed(int aVirtualKey) const;
	bool IsKeyReleased(int aVirtualKey) const;

	bool IsMouseButtonDown(int aButton) const;
	bool IsMouseButtonPressed(int aButton) const;
	bool IsMouseButtonReleased(int aButton) const;

	float GetMouseDeltaX() const { return myMouseDeltaX; }
	float GetMouseDeltaY() const { return myMouseDeltaY; }
	int GetMouseX() const { return myMouseX; }
	int GetMouseY() const { return myMouseY; }

private:
	InputManager() = default;

	bool myCurrentKeys[256]{};
	bool myPreviousKeys[256]{};

	int myMouseX{};
	int myMouseY{};
	int myPreviousMouseX{};
	int myPreviousMouseY{};
	float myMouseDeltaX{};
	float myMouseDeltaY{};
};
