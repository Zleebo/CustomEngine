#include "TransformCommand.h"

#include <Math/Matrix4x4.h>
#include <Selection.h>

TransformCommand::TransformCommand(
	const std::vector<Matrix4x4f> someFrom,
	const std::vector<Matrix4x4f> someTo,
	const std::vector<ModelInstance *> someModelInstances
)
	: myFrom(someFrom)
	, myTo(someTo)
	, mySceneModels(someModelInstances)
{
	assert(myFrom.size() == myTo.size() && myFrom.size() == mySceneModels.size());
}

TransformCommand::~TransformCommand()
{
}

bool TransformCommand::Execute()
{
	if (mySceneModels.size() <= 0)
	{
		return false;
	}
	for (std::size_t i = 0; i < myTo.size(); ++i)
	{
		Vector3f pos, rot, scale;
		myTo[i].DecomposeMatrix(pos, rot, scale);

		mySceneModels[i]->SetRotation(rot);
		mySceneModels[i]->SetLocation(pos);
		mySceneModels[i]->SetScale(scale);
	}



	return true;
}

bool TransformCommand::Undo()
{
	for (std::size_t i = 0; i < myFrom.size(); ++i)
	{
		Vector3f pos, rot, scale;
		myFrom[i].DecomposeMatrix(pos, rot, scale);

		mySceneModels[i]->SetRotation(rot);
		mySceneModels[i]->SetLocation(pos);
		mySceneModels[i]->SetScale(scale);
	}


	return true;
}
