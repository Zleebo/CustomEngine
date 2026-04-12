#pragma once
#include "AbstractCommand.h"

#include <string>

class ModelInstance;

class ChangeTextureCommand : public AbstractCommand
{
public:
	ChangeTextureCommand(
		ModelInstance* aModelInstance, 
		const char *aFrom, const char *aTo
	);
	~ChangeTextureCommand() override;

	bool Execute() override;
	bool Undo() override;

private:
	ModelInstance* myModelInstance;
	const std::string myFrom;
	const std::string myTo;
};