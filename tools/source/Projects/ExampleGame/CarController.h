#pragma once
#include <Engine.h>
#include <Physics/PhysicsWorld.h>
#include <memory>
#include "CarAutotest.h"

class RigidbodyComponent;
class ModelInstance;

class CarController : public ScriptComponent, public IPhysicsVehicleController
{
public:
	struct WheelState
	{
		float omega            = 0.0f;
		float spinAngle        = 0.0f;
		float loadedTime       = 0.0f;

		bool     grounded      = false;
		Vector3f contactPoint  = {};
		Vector3f contactNormal = { 0.f, 1.f, 0.f };
		Vector3f wheelForward  = { 0.f, 0.f, 1.f };
		Vector3f wheelRight    = { 1.f, 0.f, 0.f };
		float    L_spring      = 0.0f;
		float    dL            = 0.0f;
		float    F_z           = 0.0f;
		float    kappa         = 0.0f;
		float    alpha         = 0.0f;
		float    F_x           = 0.0f;
		float    F_y           = 0.0f;
	};

	struct WheelContact
	{
		Vector3f worldPos          = {};
		Vector3f normal            = { 0.f, 1.f, 0.f };
		Vector3f contactPoint      = {};
		float    gap               = 0.f;
		float    compression       = 0.f;
		float    wheelLoad         = 0.f;
		float    wheelAngularSpeed = 0.f;
		float    slipRatio         = 0.f;
		float    slipAngle         = 0.f;
		bool     grounded          = false;
	};

	CarController();
	~CarController() override;

	void OnStart() override;
	void Update(float aDeltaTime) override;
	void LateUpdate(float aDeltaTime) override;
	void ApplyVehicleForces(float aDeltaTime) override;

	void SetWheels(ModelInstance* aFL, ModelInstance* aFR, ModelInstance* aBL, ModelInstance* aBR);

	bool HasWheel(int i) const { return i >= 0 && i < 4 && myWheels[i]; }
	ModelInstance* GetWheel(int i) const { return (i >= 0 && i < 4) ? myWheels[i] : nullptr; }
	const WheelContact& GetWheelContact(int i) const { return myWheelContacts[i]; }
	const WheelState& GetWheelState(int i) const { return myWheelState[i]; }
	float GetWheelRadius() const { return myWheelRadius; }

	float myAcceleration              = 120.0f;
	float myReverseAccel              = 60.0f;
	float myMaxSpeed                  = 42.0f;
	float myBrakeStrength             = 8.0f;
	float mySteerSpeed                = 120.0f;
	float myLateralFriction           = 8.0f;
	float myWheelRadius               = 0.42f;
	float myRestLength                = 0.26f;
	float myMaxLength                 = 0.60f;
	float mySuspensionFrequencyHz     = 2.0f;
	float mySuspensionDampingRatio    = 0.9f;

private:
	RigidbodyComponent* myRigidbody = nullptr;
	ModelInstance* myWheels[4] = {};

	WheelState myWheelState[4] = {};
	mutable WheelContact myWheelContacts[4] = {};

	Vector3f myChassisAnchorLocal[4] = {};
	Vector3f mySuspensionAxisLocal = { 0.f, -1.f, 0.f };

	float mySpringK[4] = {};
	float mySpringC[4] = {};
	float mySprungMass[4] = {};

	float myCachedThrottleInput = 0.f;
	float myCachedSteerInput = 0.f;
	bool  myCachedBrakeInput = false;
	float mySteerVisualAngle = 0.f;
	float myAirTime = 0.f;
	bool  myIsGrounded = false;
	bool  mySpringConfigured = false;

	std::unique_ptr<CarAutotest> myAutoTest;

	void ConfigureSpringParameters();
	void BuildWheelTelemetry();
};