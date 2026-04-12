#include "DeleteCommand.h"
#include <Scene/Scene.h>
#include <Selection.h>

DeleteCommand::DeleteCommand(std::shared_ptr<Scene> aScene, std::vector<ModelInstance*> aInstances)
	: myScene(aScene), myInstances(aInstances)
	{
}

DeleteCommand::~DeleteCommand()
{
	if (myOwnsInstances)
		for (auto* mi : myInstances) delete mi;
}

bool DeleteCommand::Execute()
{
	for (auto* mi : myInstances)
	{
		myScene->RemoveModelInstance(mi);
		Selection::RemoveFromSelection(mi);
	}
	myOwnsInstances = true;
	return true;
}

bool DeleteCommand::Undo()
{
	for (auto* mi : myInstances)
		myScene->AddModelInstance(mi);
	myOwnsInstances = false;
	return true;
}
