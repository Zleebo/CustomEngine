#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include <Math/Matrix4x4.h>

class ModelInstance;

using ParentToChildMapping = std::unordered_map< ModelInstance*, std::vector<ModelInstance*>>;
using ChildToParentMapping = std::unordered_map< ModelInstance*, ModelInstance*>;

class Selection
{
public:
	static std::vector<ModelInstance*> GetSelection() { return mySelection; }
	
	static void SetSelection(const std::vector<ModelInstance*>& aNewSelection);
	static void AddToSelection(ModelInstance *aModel);

	static void RemoveFromSelection(ModelInstance* aModel);
	static void ClearSelection();

	static bool Contains(const ModelInstance* anInstance);
	static void ToggleSelect(ModelInstance* anInstance);

	static Matrix4x4f GetCentroid();

	static std::vector<ModelInstance*> GetChildrenInBranch(ModelInstance* parent);

public:
	static ParentToChildMapping myParentToChildMap;
	static ChildToParentMapping myChildToParentMap;

private:
	static std::vector<ModelInstance *> mySelection;

private:
	static void TraverseChildBranch(ModelInstance* parent, std::vector<ModelInstance*>& aChildList);
};