#pragma once

#include <Engine.h>
#include <Selection.h>

class AbstractCommand
{
public:
	virtual bool Execute() = 0;
	virtual bool Undo() = 0;
	virtual ~AbstractCommand() {}

	void ApplySelection()
	{
		Selection::ClearSelection();
		Selection::SetSelection(mySelection);
	}

	void SetSelection()
	{
		mySelection = Selection::GetSelection();
	}
protected:
	std::vector<ModelInstance*> mySelection;
};