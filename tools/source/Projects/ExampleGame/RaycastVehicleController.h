#pragma once
#include <Engine.h>
#include <Physics/PhysicsWorld.h>

class RigidbodyComponent;
class ModelInstance;

class RaycastVehicleController : public ScriptComponent, public IPhysicsVehicleController
{
public:
	struct WheelConfig

{
		Vector3f anchorLocal = {};
		float    maxTravel   = 0.35f;
		float    restLength  = 0.14f;
		float    wheelRadius = 0.42f;
		float    springK     = 0.f;
		float    springC     = 0.f;
		bool     isFront     = false;
		bool     isDriven    = false;
	};

	struct RayWheelState

{
		// Persistent across frames
		float    omega      = 0.f;
		float    spinAngle  = 0.f;
		float    loadedTime = 0.f;
		// Refreshed each frame
		bool     grounded      = false;
		Vector3f contactPoint  = {};
		Vector3f contactNormal = { 0.f, 1.f, 0.f };
		Vector3f wheelForward  = { 0.f, 0.f, 1.f };
		Vector3f wheelRight    = { 1.f, 0.f, 0.f };
		float    L_spring      = 0.f;
		float    dL            = 0.f;
		float    F_z           = 0.f;
		float    kappa         = 0.f;
		float    alpha         = 0.f;
		float    F_x           = 0.f;
		float    F_y           = 0.f;
	};

	RaycastVehicleController();
	~RaycastVehicleController() override;

	void OnStart()  override;
	void Update(float dt) override;
	void LateUpdate(float dt) override;
	void ApplyVehicleForces(float dt) override;

	void SetWheels(ModelInstance* aFL, ModelInstance* aFR,
	               ModelInstance* aBL, ModelInstance* aBR);

	// Inspector-exposed tuning
	float myAcceleration          = 32.f;
	float myReverseAccel          = 22.f;
	float myMaxSpeed              = 42.f;
	float myBrakeStrength         = 8.f;
	float myLongitudinalGrip      = 14.f;
	float myLateralGrip           = 14.f;
	float myTireFriction          = 1.6f;
	float myRollingResistance     = 0.02f;
	float myWheelRadius           = 0.42f;
	float myRestLength            = 0.14f;
	float myMaxLength             = 0.35f;
	float mySuspensionFrequencyHz = 3.f;
	float mySuspensionDampingRatio = 0.9f;
	float myAntiRoll              = 800.f;
	float myUprightAssist         = 40.f;

private:
	RigidbodyComponent* myRigidbody = nullptr;
	ModelInstance*      myWheels[4] = {};
	WheelConfig         myConfig[4] = {};
	RayWheelState       myState[4]  = {};

	float mySpringK[4]    = {};
	float mySpringC[4]    = {};
	float mySprungMass[4] = {};

	float myCachedThrottle    = 0.f;
	float myCachedSteer       = 0.f;
	bool  myCachedBrake       = false;
	float mySteerVisualAngle  = 0.f;
	float myAirTime           = 0.f;
	bool  myIsGrounded        = false;
	bool  myConfigured        = false;

	void ConfigureSpring();
};
