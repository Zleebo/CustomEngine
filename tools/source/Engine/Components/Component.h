#pragma once
#include <string>
#include <vector>

class ModelInstance;

// Describes a variable exposed to the editor inspector.
struct ExposedProperty
{
	enum class Type { Float, Int, Bool, Vec3 };
	std::string label;
	Type        type  = Type::Float;
	void*       ptr   = nullptr;
	float       speed = 0.1f;
	float       min   = 0.0f;
	float       max   = 0.0f;
};

class Component
{
public:
	virtual ~Component() = default;

	virtual void OnStart() {}
	virtual void Update(float aDeltaTime) {}
	virtual void LateUpdate(float aDeltaTime) { (void)aDeltaTime; }

	// Called by the editor for components that do not use EXPOSE macros.
	virtual void DrawInspector() {}

	ModelInstance*              myOwner = nullptr;
	std::vector<ExposedProperty> myExposedProperties;
};

// ---------------------------------------------------------------------------
// Expose macros, call these inside your component's constructor to make
// member variables appear as editable fields in the editor inspector.
//
//   EXPOSE_FLOAT("Speed",       mySpeed)
//   EXPOSE_FLOAT_EX("Speed",    mySpeed, /*speed*/0.1f, /*min*/0.f, /*max*/100.f)
//   EXPOSE_INT("Count",         myCount)
//   EXPOSE_BOOL("Active",       myActive)
//   EXPOSE_VEC3("Half Extents", myHalfExtents)   // expects a Vector3<T>
// ---------------------------------------------------------------------------
#define EXPOSE_FLOAT(label, var) \
	myExposedProperties.push_back({label, ExposedProperty::Type::Float, (void*)&(var), 0.1f, 0.0f, 0.0f})

#define EXPOSE_FLOAT_EX(label, var, speed, minVal, maxVal) \
	myExposedProperties.push_back({label, ExposedProperty::Type::Float, (void*)&(var), (speed), (minVal), (maxVal)})

#define EXPOSE_INT(label, var) \
	myExposedProperties.push_back({label, ExposedProperty::Type::Int, (void*)&(var), 1.0f, 0.0f, 0.0f})

#define EXPOSE_BOOL(label, var) \
	myExposedProperties.push_back({label, ExposedProperty::Type::Bool, (void*)&(var), 0.0f, 0.0f, 0.0f})

// Vector3<T> stores floats in myValues[3] (union with X/Y/Z), which is
// layout-compatible with float[3] expected by ImGui::DragFloat3.
#define EXPOSE_VEC3(label, var) \
	myExposedProperties.push_back({label, ExposedProperty::Type::Vec3, (void*)&(var).myValues[0], 0.1f, 0.0f, 0.0f})
