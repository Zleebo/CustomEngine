#include "ChangeTextureCommand.h"

ChangeTextureCommand::ChangeTextureCommand(ModelInstance *aModelInstance, const char *aFrom, const char *aTo)
	: myModelInstance(aModelInstance)
	, myFrom(aFrom)
	, myTo(aTo)
{

}

ChangeTextureCommand::~ChangeTextureCommand()
{

}

bool ChangeTextureCommand::Execute()
{
	myModelInstance->SetTexture(myTo.c_str());
	return true;
}

bool ChangeTextureCommand::Undo()
{
	myModelInstance->SetTexture(myFrom.c_str());
	return true;
}