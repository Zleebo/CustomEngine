#pragma once
#include "Component.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>

class ComponentFactory
{
public:
	static ComponentFactory& Get();

	using CreatorFn = std::function<std::unique_ptr<Component>()>;

	void Register(const std::string& aTypeName, CreatorFn aCreator);
	std::unique_ptr<Component> Create(const std::string& aTypeName) const;
	bool Has(const std::string& aTypeName) const;
	std::vector<std::string> GetRegisteredTypes() const;

private:
	ComponentFactory() = default;
	std::unordered_map<std::string, CreatorFn> myCreators;
};

#define REGISTER_COMPONENT(TypeName) \
	ComponentFactory::Get().Register(#TypeName, []() { return std::make_unique<TypeName>(); })
