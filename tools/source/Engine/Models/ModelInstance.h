#pragma once

#include <Math/Math.hpp>
#include <Math/Transform.h>
#include <d3d11.h>

#include <uuid/UUIDManager.h>
#include <Components/Component.h>

#include <memory>
#include <vector>

class Model;
class DX_Texture2D;

class ModelInstance
{
public:
	ModelInstance(Model* model, const char *aName = nullptr, const std::string *uuid=nullptr);

	Model* GetModel() const;
	const char* GetTexture() const;
	ID3D11ShaderResourceView* GetTextureResource();

	__forceinline const Transform& GetTransform() const { return myTransform; }
	__forceinline const Transform CopyTransform() const { return myTransform; }

	const Vector3f GetLocation() const;

	void SetTransform(const Transform& someTransform);
	void SetRotation(const Rotator &someRotation);
	void SetLocation(const Vector3f &someLocation);
	void SetScale(const Vector3f &someScale);

	void Translate(const Vector3f& anOffset)
	{
		myTransform.Translate(anOffset);
	}
	void Rotate(const Rotator& aRotation)
	{
		myTransform.AddRotation(aRotation);
	}

public:
	const char* GetName() const { 
		return myName.c_str();
	}
	void SetName(const char* aName) { myName = aName; }
	void SetTexture(const char* path);

	const uint32_t GetID() const;
	const char* GetUUID() const { return myUUID.c_str(); }
	void SetUUID(const char* uuid) { UUIDManager::GetIDForUUID(uuid); }

	void AddComponent(std::unique_ptr<Component> aComponent);
	void RemoveComponent(int index);
	void UpdateComponents(float aDeltaTime);
	void LateUpdateComponents(float aDeltaTime);
	const std::vector<std::unique_ptr<Component>>& GetAllComponents() const { return myComponents; }

	ModelInstance* GetParent() const { return myParent; }
	const std::vector<ModelInstance*>& GetChildren() const { return myChildren; }
	void SetParent(ModelInstance* aParent);
	bool IsDescendantOf(const ModelInstance* aPotentialAncestor) const;

	template<typename T>
	T* GetComponent()
	{
		for (auto& c : myComponents)
		{
			if (T* typed = dynamic_cast<T*>(c.get()))
			{
				return typed;
			}
		}
		return nullptr;
	}

private:

	Model* myModel{};
	std::string myTexture;
	std::string myName;
	Transform myTransform{};
	std::string myUUID;
	std::vector<std::unique_ptr<Component>> myComponents;
	ModelInstance* myParent = nullptr;
	std::vector<ModelInstance*> myChildren;
};

