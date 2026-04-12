#include <stdafx.h>

//#include <UUIDManager.h>

#include "ModelInstance.h"
#include "Model.h"

#include <GraphicsEngine/Texture/Texture2D.h>
#include <GraphicsEngine/Material/Material.h>
#include <algorithm>

ModelInstance::ModelInstance(Model* model, const char* aName, const std::string *uuid)
	: myModel(model)
	, myName(aName == nullptr ? model->GetPath() : aName)
{
	if (uuid != nullptr)
	{
		myUUID = UUIDManager::GetIDForUUID(uuid->c_str());
	}
}

const uint32_t ModelInstance::GetID() const
{
	return UUIDManager::GetIDForUUID(myUUID.c_str());
	/*std::string uuid;
	UuidToStringA(&myUUID, (RPC_CSTR*)uuid.data());
	return uuid;*/
}

Model* ModelInstance::GetModel() const
{
	return myModel;
}

void ModelInstance::SetTransform(const Transform& transform)
{
	myTransform = transform;
}

void ModelInstance::SetScale(const Vector3f &scale)
{
	myTransform.SetScale(scale);
}

void ModelInstance::SetRotation(const Rotator &rotation)
{
	// Really should unroll rotations as well somewhere
	// so we can use -180 to 180 instead of 0 to 360.
	myTransform.SetRotation(rotation);
}

void ModelInstance::SetLocation(const Vector3f &location)
{
	myTransform.SetPosition(location);
}

const Vector3f ModelInstance::GetLocation() const
{
	return std::move(myTransform.GetPosition());
}

void ModelInstance::SetTexture(const char* path)
{
	myTexture = path;
}

const char* ModelInstance::GetTexture() const
{
	return myTexture.c_str();
}

ID3D11ShaderResourceView* ModelInstance::GetTextureResource()
{
	return DX_Texture2D::GetTexture(myTexture.c_str());
}

void ModelInstance::AddComponent(std::unique_ptr<Component> aComponent)
{
	aComponent->myOwner = this;
	aComponent->OnStart();
	myComponents.push_back(std::move(aComponent));
}

void ModelInstance::RemoveComponent(int index)
{
	if (index >= 0 && index < (int)myComponents.size())
		myComponents.erase(myComponents.begin() + index);
}

void ModelInstance::UpdateComponents(float aDeltaTime)
{
	for (auto& c : myComponents)
	{
		c->Update(aDeltaTime);
	}
}

void ModelInstance::LateUpdateComponents(float aDeltaTime)
{
	for (auto& c : myComponents)
	{
		c->LateUpdate(aDeltaTime);
	}
}

void ModelInstance::SetParent(ModelInstance* aParent)
{
	if (aParent == this)
	{
		return;
	}

	// Prevent cyclic parenting (cannot parent to own descendant).
	if (aParent && aParent->IsDescendantOf(this))
	{
		return;
	}

	if (myParent == aParent)
	{
		return;
	}

	if (myParent)
	{
		auto& siblings = myParent->myChildren;
		siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
	}

	myParent = aParent;
	if (myParent)
	{
		auto& children = myParent->myChildren;
		if (std::find(children.begin(), children.end(), this) == children.end())
		{
			children.push_back(this);
		}
	}
}

bool ModelInstance::IsDescendantOf(const ModelInstance* aPotentialAncestor) const
{
	if (!aPotentialAncestor)
	{
		return false;
	}
	const ModelInstance* cursor = myParent;
	while (cursor)
	{
		if (cursor == aPotentialAncestor)
		{
			return true;
		}
		cursor = cursor->myParent;
	}
	return false;
}
