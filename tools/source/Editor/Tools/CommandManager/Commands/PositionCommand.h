#pragma once
#include "AbstractCommand.h"

#include <Math/Vector3.h>

class ModelInstance;

class PositionCommand : public AbstractCommand
{
public:
	PositionCommand(
		ModelInstance* aModelInstance, 
		Vector3f aFrom, Vector3f aTo
	);
	~PositionCommand() override;

	bool Execute() override;
	bool Undo() override;

private:
	ModelInstance* myModelInstance;
	const Vector3f myFrom;
	const Vector3f myTo;
};