#pragma once
#include <AbstractCommand.h>
#include <Models/ModelInstance.h>

class SetParentCommand : public AbstractCommand
{
public:
	SetParentCommand(ModelInstance* aChild, ModelInstance* aOldParent, ModelInstance* aNewParent)
		: myChild(aChild)
		, myOldParent(aOldParent)
		, myNewParent(aNewParent)
	{
	}

	bool Execute() override
	{
		myChild->SetParent(myNewParent);
		return true;
	}

	bool Undo() override
	{
		myChild->SetParent(myOldParent);
		return true;
	}

private:
	ModelInstance* myChild;
	ModelInstance* myOldParent;
	ModelInstance* myNewParent;
};
