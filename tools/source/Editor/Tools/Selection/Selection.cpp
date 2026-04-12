#include "Selection.h"

#include <Engine.h>
#include <algorithm>

#include <Math/Vector.h>
#include <Models/ModelInstance.h>
#include <Math/Transform.h>
#include <Math/Math.hpp>

std::vector<ModelInstance *> Selection::mySelection;
ParentToChildMapping Selection::myParentToChildMap;
ChildToParentMapping Selection::myChildToParentMap;

void Selection::TraverseChildBranch(ModelInstance* parent, std::vector<ModelInstance*> &aChildList)
{
	if (!parent) return;
	for (auto* child : parent->GetChildren())
	{
		aChildList.push_back(child);
		TraverseChildBranch(child, aChildList);
	}
}

std::vector<ModelInstance*> Selection::GetChildrenInBranch(ModelInstance* parent)
{
	std::vector<ModelInstance*> result;
	TraverseChildBranch(parent, result);
	return result;
}

void Selection::SetSelection(const std::vector<ModelInstance*>& aNewSelection)
{
	mySelection = aNewSelection;
}

void Selection::AddToSelection(ModelInstance* aModel)
{
	mySelection.push_back(aModel);
}

void Selection::RemoveFromSelection(ModelInstance* aModel)
{
	mySelection.erase(std::remove(mySelection.begin(), mySelection.end(), aModel), mySelection.end());
}

void Selection::ClearSelection()
{
	mySelection.clear();
}

bool Selection::Contains(const ModelInstance* anInstance)
{
	return std::find(mySelection.begin(), mySelection.end(), anInstance) != mySelection.end();
}

void Selection::ToggleSelect(ModelInstance* anInstance)
{
	if (std::find(mySelection.begin(), mySelection.end(), anInstance) != mySelection.end())
	{
		RemoveFromSelection(anInstance);
	}
	else
	{
		AddToSelection(anInstance);
	}
}

Matrix4x4f Selection::GetCentroid()
{
	if (mySelection.empty())
		return Matrix4x4f::CreateIdentityMatrix();

	Vector3f centroid_position;
	Rotator centroid_orientation;
	for (const auto& mi : mySelection)
	{
		centroid_position    += mi->GetLocation();
		centroid_orientation += mi->GetTransform().GetRotation();
	}
	centroid_position    /= static_cast<float>(mySelection.size());
	centroid_orientation /= static_cast<float>(mySelection.size());
	return Matrix4x4f::CreateRollPitchYawMatrix(centroid_orientation)
	     * Matrix4x4f::CreateTranslationMatrix(centroid_position);
}
