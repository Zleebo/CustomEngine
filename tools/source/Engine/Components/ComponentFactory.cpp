#include <stdafx.h>
#include "ComponentFactory.h"

ComponentFactory& ComponentFactory::Get()
{
	static ComponentFactory instance;
	return instance;
}

void ComponentFactory::Register(const std::string& aTypeName, CreatorFn aCreator)
{
	myCreators[aTypeName] = std::move(aCreator);
}

std::unique_ptr<Component> ComponentFactory::Create(const std::string& aTypeName) const
{
	auto it = myCreators.find(aTypeName);
	if (it != myCreators.end())
	{
		return it->second();
	}
	return nullptr;
}

bool ComponentFactory::Has(const std::string& aTypeName) const
{
	return myCreators.count(aTypeName) > 0;
}

std::vector<std::string> ComponentFactory::GetRegisteredTypes() const
{
	std::vector<std::string> types;
	types.reserve(myCreators.size());
	for (const auto& [name, _] : myCreators)
		types.push_back(name);
	return types;
}
