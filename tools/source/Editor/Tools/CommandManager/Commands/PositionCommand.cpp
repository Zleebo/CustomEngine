#include "PositionCommand.h"

PositionCommand::PositionCommand(ModelInstance *aModelInstance, Vector3f aFrom, Vector3f aTo)
	: myModelInstance(aModelInstance)
	, myFrom(aFrom)
	, myTo(aTo)
{

}

PositionCommand::~PositionCommand()
{

}

bool PositionCommand::Execute()
{
	myModelInstance->SetLocation(myTo);
	return true;
}

bool PositionCommand::Undo()
{
	myModelInstance->SetLocation(myFrom);
	return true;
}