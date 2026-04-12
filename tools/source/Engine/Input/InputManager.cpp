#include <stdafx.h>
#include "InputManager.h"

InputManager& InputManager::Get()
{
	static InputManager instance;
	return instance;
}

void InputManager::Update()
{
	memcpy(myPreviousKeys, myCurrentKeys, sizeof(myCurrentKeys));

	for (int i = 0; i < 256; i++)
	{
		myCurrentKeys[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
	}

	myPreviousMouseX = myMouseX;
	myPreviousMouseY = myMouseY;

	POINT p;
	GetCursorPos(&p);
	myMouseX = p.x;
	myMouseY = p.y;

	myMouseDeltaX = static_cast<float>(myMouseX - myPreviousMouseX);
	myMouseDeltaY = static_cast<float>(myMouseY - myPreviousMouseY);
}

bool InputManager::IsKeyDown(int aVirtualKey) const
{
	if (aVirtualKey < 0 || aVirtualKey >= 256) return false;
	return myCurrentKeys[aVirtualKey];
}

bool InputManager::IsKeyPressed(int aVirtualKey) const
{
	if (aVirtualKey < 0 || aVirtualKey >= 256) return false;
	return myCurrentKeys[aVirtualKey] && !myPreviousKeys[aVirtualKey];
}

bool InputManager::IsKeyReleased(int aVirtualKey) const
{
	if (aVirtualKey < 0 || aVirtualKey >= 256) return false;
	return !myCurrentKeys[aVirtualKey] && myPreviousKeys[aVirtualKey];
}

bool InputManager::IsMouseButtonDown(int aButton) const
{
	static const int buttons[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
	if (aButton < 0 || aButton > 2) return false;
	return myCurrentKeys[buttons[aButton]];
}

bool InputManager::IsMouseButtonPressed(int aButton) const
{
	static const int buttons[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
	if (aButton < 0 || aButton > 2) return false;
	return myCurrentKeys[buttons[aButton]] && !myPreviousKeys[buttons[aButton]];
}

bool InputManager::IsMouseButtonReleased(int aButton) const
{
	static const int buttons[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
	if (aButton < 0 || aButton > 2) return false;
	return !myCurrentKeys[buttons[aButton]] && myPreviousKeys[buttons[aButton]];
}
