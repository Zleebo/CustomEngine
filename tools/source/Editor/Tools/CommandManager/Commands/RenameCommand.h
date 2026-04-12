#pragma once
#include <AbstractCommand.h>
#include <Models/ModelInstance.h>
#include <string>

class RenameCommand : public AbstractCommand
{
public:
	RenameCommand(ModelInstance* aInstance, const char* aOldName, const char* aNewName)
		: myInstance(aInstance)
		, myOldName(aOldName)
		, myNewName(aNewName)
	{
	}

	bool Execute() override
	{
		myInstance->SetName(myNewName.c_str());
		return true;
	}

	bool Undo() override
	{
		myInstance->SetName(myOldName.c_str());
		return true;
	}

private:
	ModelInstance* myInstance;
	std::string myOldName;
	std::string myNewName;
};
