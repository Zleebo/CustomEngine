#pragma once
#include "Component.h"

class ScriptComponent : public Component
{
public:
	virtual void OnStart() override {}
	virtual void Update(float aDeltaTime) override {}
};
