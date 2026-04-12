#pragma once
#include "AbstractCommand.h"

class TransformCommand : public AbstractCommand
{
public:
	TransformCommand(
		const std::vector<Matrix4x4f> someFrom,
		const std::vector<Matrix4x4f> someTo,
		const std::vector<ModelInstance*> someModelInstances
	);

	~TransformCommand() override;

	bool Execute() override;
	bool Undo() override;

private:
	const std::vector<Matrix4x4f> myFrom;
	const std::vector<Matrix4x4f> myTo;
	const std::vector<ModelInstance*> mySceneModels;
};
