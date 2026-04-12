#include "CarController.h"

#include <Engine.h>
#include <Physics/RigidbodyComponent.h>

#include <cmath>

namespace
{
	constexpr float kPi               = 3.14159265358979323846f;
	constexpr float kTwoPi            = 6.28318530717958647692f;
	constexpr float kDegToRad         = kPi / 180.0f;
	constexpr float kRadToDeg         = 180.0f / kPi;
	constexpr float kSweepTolerance   = 0.02f;

	float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
	float Clamp01(float v) { return Clamp(v, 0.f, 1.f); }
	float AbsF(float v) { return v < 0.f ? -v : v; }
	float SignOrZero(float v, float eps = 0.0001f)
	{
		return v > eps ? 1.f : (v < -eps ? -1.f : 0.f);
	}

	Vector3f NormalizeOr(const Vector3f& v, const Vector3f& fallback)
	{
		const float len = v.Length();
		return len > 0.0001f ? v * (1.f / len) : fallback;
	}

	Transform MakeNoScaleTransform(const Transform& t)
	{
		Transform r = t;
		r.SetScale({ 1.f, 1.f, 1.f });
		return r;
	}

	Vector3f TransformPointNoScale(const Transform& t, const Vector3f& p)
	{
		return MakeNoScaleTransform(t).TransformPosition(const_cast<Vector3f&>(p));
	}

	Vector3f InverseTransformPointNoScale(const Transform& t, const Vector3f& p)
	{
		return MakeNoScaleTransform(t).InverseTransformPosition(const_cast<Vector3f&>(p));
	}

	Vector3f TransformVectorNoScale(const Transform& t, const Vector3f& v)
	{
		const Matrix4x4f m = MakeNoScaleTransform(t).GetMatrix(true);
		return m.GetRight() * v.X + m.GetUp() * v.Y + m.GetForward() * v.Z;
	}

	ModelInstance* FindChildByName(ModelInstance* parent, const char* name)
	{
		if (!parent || !name) return nullptr;

		for (ModelInstance* child : parent->GetChildren())
		{
			if (!child) continue;
			const char* childName = child->GetName();
			if (childName && std::string(childName) == name)
				return child;
		}
		return nullptr;
	}
}

CarController::CarController()
{
	EXPOSE_FLOAT_EX("Acceleration",      myAcceleration,    0.1f,   0.f, 400.f);
	EXPOSE_FLOAT_EX("Reverse Accel",     myReverseAccel,    0.1f,   0.f, 400.f);
	EXPOSE_FLOAT_EX("Max Speed",         myMaxSpeed,        0.1f,   1.f, 200.f);
	EXPOSE_FLOAT_EX("Brake Strength",    myBrakeStrength,   0.1f,   0.f,  50.f);
	EXPOSE_FLOAT_EX("Steer Speed",       mySteerSpeed,      1.0f,  10.f, 360.f);
	EXPOSE_FLOAT_EX("Lateral Friction",  myLateralFriction, 0.1f,   0.f,  50.f);
	EXPOSE_FLOAT_EX("Wheel Radius",      myWheelRadius,    0.01f, 0.05f,   2.f);
	EXPOSE_FLOAT_EX("Rest Length",         myRestLength,            0.01f,  0.f,   1.f);
	EXPOSE_FLOAT_EX("Max Length",          myMaxLength,             0.01f, 0.01f,  2.f);
	EXPOSE_FLOAT_EX("Suspension Hz",       mySuspensionFrequencyHz, 0.05f,  0.1f, 20.f);
	EXPOSE_FLOAT_EX("Suspension Damping",  mySuspensionDampingRatio,0.01f, 0.05f,  3.f);
}

CarController::~CarController()
{
	PhysicsWorld::Get().RemoveVehicleController(this);
}

void CarController::OnStart()
{
	myRigidbody = myOwner ? myOwner->GetComponent<RigidbodyComponent>() : nullptr;
	if (myRigidbody)
	{
		myRigidbody->myBounciness = 0.0f;
	}

	myAutoTest = std::make_unique<CarAutotest>();
	myAutoTest->Initialize(myOwner, myRigidbody);

	if (myOwner)
	{
		SetWheels(
			FindChildByName(myOwner, "WheelFL"),
			FindChildByName(myOwner, "WheelFR"),
			FindChildByName(myOwner, "WheelBL"),
			FindChildByName(myOwner, "WheelBR"));
	}

	if (!myWheels[0] || !myWheels[1] || !myWheels[2] || !myWheels[3])
	{
		const Vector3f fallbackOffsets[4] = {
			{ -0.92f, -0.08f,  1.42f },
			{  0.92f, -0.08f,  1.42f },
			{ -0.92f, -0.08f, -1.42f },
			{  0.92f, -0.08f, -1.42f }
		};

		for (int i = 0; i < 4; ++i)
		{
			myChassisAnchorLocal[i] = {
				fallbackOffsets[i].X,
				fallbackOffsets[i].Y + myRestLength,
				fallbackOffsets[i].Z
			};
			myWheelState[i] = {};
		}
	}

	PhysicsWorld::Get().AddVehicleController(this);
	ConfigureSpringParameters();
}

void CarController::SetWheels(ModelInstance* aFL, ModelInstance* aFR,
	ModelInstance* aBL, ModelInstance* aBR)
{
	myWheels[0] = aFL;
	myWheels[1] = aFR;
	myWheels[2] = aBL;
	myWheels[3] = aBR;

	if (!myOwner) return;
	const Transform ownerTransform = myOwner->GetTransform();

	for (int i = 0; i < 4; ++i)
	{
		if (!myWheels[i]) continue;

		const Vector3f wheelWorld = myWheels[i]->GetLocation();
		const Vector3f wheelLocal = InverseTransformPointNoScale(ownerTransform, wheelWorld);

		myChassisAnchorLocal[i] = {
			wheelLocal.X,
			wheelLocal.Y + myRestLength,
			wheelLocal.Z
		};
		myWheelState[i] = {};
	}

	ConfigureSpringParameters();
}

void CarController::ConfigureSpringParameters()
{
	if (!myRigidbody) return;

	float leftX = myChassisAnchorLocal[0].X, rightX = leftX;
	float rearZ = myChassisAnchorLocal[0].Z, frontZ = rearZ;

	for (int i = 1; i < 4; ++i)
	{
		if (myChassisAnchorLocal[i].X < leftX)  leftX  = myChassisAnchorLocal[i].X;
		if (myChassisAnchorLocal[i].X > rightX) rightX = myChassisAnchorLocal[i].X;
		if (myChassisAnchorLocal[i].Z < rearZ)  rearZ  = myChassisAnchorLocal[i].Z;
		if (myChassisAnchorLocal[i].Z > frontZ) frontZ = myChassisAnchorLocal[i].Z;
	}

	const Vector3f com = myRigidbody->myCenterOfMassLocal;
	const float spanX = (rightX - leftX) > 0.0001f ? (rightX - leftX) : 1.f;
	const float spanZ = (frontZ - rearZ) > 0.0001f ? (frontZ - rearZ) : 1.f;

	const float rightW = Clamp01((com.X - leftX) / spanX);
	const float frontW = Clamp01((com.Z - rearZ) / spanZ);
	const float leftW  = 1.f - rightW;
	const float rearW  = 1.f - frontW;

	float weights[4] = { frontW * leftW, frontW * rightW, rearW * leftW, rearW * rightW };
	float totalW = weights[0] + weights[1] + weights[2] + weights[3];
	if (totalW < 0.0001f)
	{
		totalW = 1.f;
		for (float& w : weights) w = 0.25f;
	}

	const float omegaN = kTwoPi * (mySuspensionFrequencyHz > 0.1f ? mySuspensionFrequencyHz : 0.1f);
	const float zeta   = mySuspensionDampingRatio > 0.05f ? mySuspensionDampingRatio : 0.05f;

	for (int i = 0; i < 4; ++i)
	{
		mySprungMass[i] = myRigidbody->myMass * (weights[i] / totalW);
		mySpringK[i]    = mySprungMass[i] * omegaN * omegaN;
		mySpringC[i]    = 2.f * zeta * mySprungMass[i] * omegaN;
	}

	mySpringConfigured = true;
}

void CarController::Update(float aDeltaTime)
{
	(void)aDeltaTime;

	if (!myRigidbody || !myOwner)
	{
		if (myAutoTest && myAutoTest->IsEnabled() && !myAutoTest->IsFinished())
			myAutoTest->FailMissingRuntime();
		return;
	}

	if (!mySpringConfigured)
		ConfigureSpringParameters();

	if (myAutoTest)
	{
		myAutoTest->NoteWheelBindingState(
			myOwner,
			myRigidbody,
			myWheels[0] && myWheels[1] && myWheels[2] && myWheels[3]);
	}

	InputManager& input = InputManager::Get();
	myCachedThrottleInput = 0.f;
	if (input.IsKeyDown('W')) myCachedThrottleInput += 1.f;
	if (input.IsKeyDown('S')) myCachedThrottleInput -= 1.f;

	myCachedSteerInput = 0.f;
	if (input.IsKeyDown('A')) myCachedSteerInput -= 1.f;
	if (input.IsKeyDown('D')) myCachedSteerInput += 1.f;

	myCachedBrakeInput = input.IsKeyDown(VK_SPACE);

	if (myAutoTest)
		myAutoTest->ApplyScript(aDeltaTime, myCachedThrottleInput, myCachedSteerInput, myCachedBrakeInput);
}

void CarController::ApplyVehicleForces(float aDeltaTime)
{
	if (!myRigidbody || !myOwner || aDeltaTime <= 0.0001f) return;

	const Transform   chassisTransform = myOwner->GetTransform();
	const Quaternionf q                = myRigidbody->GetWorldOrientation();

	// read from physics quaternion directly
	const Vector3f bodyForward = NormalizeOr(q.GetForward(), { 0.f, 0.f, 1.f });
	const Vector3f bodyRight   = NormalizeOr(q.GetRight(),   { 1.f, 0.f, 0.f });
	const Vector3f axisWorld   = NormalizeOr(q.GetUp() * -1.f, { 0.f, -1.f, 0.f });

	const float safeR        = myWheelRadius > 0.01f ? myWheelRadius : 0.01f;
	const float forwardSpeed = myRigidbody->myVelocity.Dot(bodyForward);
	int groundedCount        = 0;

	// Suspension
	for (int i = 0; i < 4; ++i)
	{
		WheelState&    ws          = myWheelState[i];
		const Vector3f anchorWorld = TransformPointNoScale(chassisTransform, myChassisAnchorLocal[i]);

		PhysicsWorld::TerrainSweepHit hit = {};
		ws.grounded = PhysicsWorld::Get().SweepSphereAgainstTerrain(
			anchorWorld, anchorWorld + axisWorld * myMaxLength, safeR, kSweepTolerance, hit);

		if (ws.grounded)
		{
			ws.contactPoint  = hit.point;
			ws.contactNormal = NormalizeOr(hit.normal, { 0.f, 1.f, 0.f });
		}
		else
		{
			ws.contactPoint  = anchorWorld + axisWorld * myMaxLength;
			ws.contactNormal = { 0.f, 1.f, 0.f };
		}

		float L_unclamped = myMaxLength;
		if (ws.grounded)
		{
			const Vector3f supportCenter = ws.contactPoint + ws.contactNormal * safeR;
			L_unclamped = (supportCenter - anchorWorld).Dot(axisWorld);
		}
		ws.L_spring = Clamp(L_unclamped, 0.f, myMaxLength);
		ws.dL       = -myRigidbody->GetVelocityAtPoint(anchorWorld).Dot(axisWorld);

		const float displacement = ws.L_spring - myRestLength;
		const float F_z_raw      = (displacement < 0.f)
			? (-mySpringK[i] * displacement - mySpringC[i] * ws.dL) : 0.f;
		ws.F_z = F_z_raw > 0.f ? F_z_raw : 0.f;

		if (ws.grounded && ws.F_z > 0.f)
		{
			myRigidbody->AddForceAccumulatedAtPoint(axisWorld * (-ws.F_z), anchorWorld);
			++groundedCount;
		}

		// Wheel spin, visual only
		ws.omega = forwardSpeed / safeR;

		if (ws.F_z > 1.f) ws.loadedTime += aDeltaTime;
		else               ws.loadedTime  = 0.f;
	}

	// Drive, steer, friction
	if (groundedCount > 0)
	{
		// Throttle / reverse
		if (AbsF(myCachedThrottleInput) > 0.01f)
		{
			const float accel = myCachedThrottleInput > 0.f ? myAcceleration : myReverseAccel;
			const float cap   = myCachedThrottleInput > 0.f ? myMaxSpeed : myMaxSpeed * 0.5f;
			if (myCachedThrottleInput * forwardSpeed < cap)
				myRigidbody->AddForceAccumulated(
					bodyForward * (myCachedThrottleInput * accel * myRigidbody->myMass));
		}

		// Braking (Space)
		if (myCachedBrakeInput && AbsF(forwardSpeed) > 0.05f)
			myRigidbody->AddForceAccumulated(
				bodyForward * (-SignOrZero(forwardSpeed) * myBrakeStrength * myRigidbody->myMass));

		// Lateral friction
		const float lateralSpeed = myRigidbody->myVelocity.Dot(bodyRight);
		const float killFraction = 1.f - std::exp(-myLateralFriction * aDeltaTime);
		myRigidbody->myVelocity  = myRigidbody->myVelocity - bodyRight * (lateralSpeed * killFraction);

		// Steering
		const float targetYaw = myCachedSteerInput * mySteerSpeed * kDegToRad;
		myRigidbody->myAngularVelocity.Y = targetYaw;
	}

	myIsGrounded = groundedCount > 0;
	if (myIsGrounded) myAirTime = 0.f;
	else              myAirTime += aDeltaTime;
}

void CarController::LateUpdate(float aDeltaTime)
{
	if (!myRigidbody || !myOwner) return;

	BuildWheelTelemetry();

	if (myAutoTest)
	{
		CarAutotest::WheelTelemetry telemetry[4];
		for (int i = 0; i < 4; ++i)
		{
			telemetry[i].worldPos          = myWheelContacts[i].worldPos;
			telemetry[i].normal            = myWheelContacts[i].normal;
			telemetry[i].supportPoint      = myWheelContacts[i].contactPoint + myWheelContacts[i].normal * myWheelRadius;
			telemetry[i].gap               = myWheelContacts[i].gap;
			telemetry[i].compression       = myWheelContacts[i].compression;
			telemetry[i].grounded          = myWheelContacts[i].grounded;
			telemetry[i].wheelLoad         = myWheelContacts[i].wheelLoad;
			telemetry[i].wheelAngularSpeed = myWheelContacts[i].wheelAngularSpeed;
			telemetry[i].slipRatio         = myWheelContacts[i].slipRatio;
			telemetry[i].slipAngle         = myWheelContacts[i].slipAngle;
		}

		int groundedCount = 0;
		int rearCount = 0;
		float totalDriveForce = 0.f;
		float totalLateralForce = 0.f;
		float totalBrakeForce = 0.f;

		for (int i = 0; i < 4; ++i)
		{
			if (myWheelContacts[i].grounded) { ++groundedCount; if (i >= 2) ++rearCount; }
			totalDriveForce   += AbsF(myWheelState[i].F_x);
			totalLateralForce += AbsF(myWheelState[i].F_y);
			if (myCachedBrakeInput)
				totalBrakeForce += myWheelState[i].F_z;
		}

		myAutoTest->NoteFrame(
			myOwner, myRigidbody, telemetry, 4,
			myWheelRadius, 0.25f * groundedCount, 0.5f * rearCount,
			groundedCount, totalDriveForce, totalLateralForce, totalBrakeForce,
			myIsGrounded, myAirTime);
	}

	const float steerTarget = myCachedSteerInput * 30.f;
	mySteerVisualAngle += (steerTarget - mySteerVisualAngle) * (1.f - std::exp(-12.f * aDeltaTime));

	const Transform chassisTransform = myOwner->GetTransform();
	const Vector3f axisWorld = NormalizeOr(
		TransformVectorNoScale(chassisTransform, mySuspensionAxisLocal),
		{ 0.f, -1.f, 0.f });

	for (int i = 0; i < 4; ++i)
	{
		if (!myWheels[i]) continue;

		const Vector3f chassisAnchorWorld = TransformPointNoScale(chassisTransform, myChassisAnchorLocal[i]);
		const Vector3f wheelWorldPos = chassisAnchorWorld + axisWorld * myWheelState[i].L_spring;
		myWheels[i]->SetLocation(wheelWorldPos);

		myWheelState[i].spinAngle += myWheelState[i].omega * kRadToDeg * aDeltaTime;

		const bool isRight = (i == 1 || i == 3);
		Rotator rot = myOwner->GetTransform().GetRotation();
		rot.Pitch += isRight ? -myWheelState[i].spinAngle : myWheelState[i].spinAngle;
		if (isRight) rot.Yaw += 180.f;
		if (i < 2)   rot.Yaw += mySteerVisualAngle;
		myWheels[i]->SetRotation(rot);
	}
}

void CarController::BuildWheelTelemetry()
{
	const Transform chassisTransform = myOwner->GetTransform();
	const Vector3f axisWorld = NormalizeOr(
		TransformVectorNoScale(chassisTransform, mySuspensionAxisLocal),
		{ 0.f, -1.f, 0.f });

	const float safeR = myWheelRadius > 0.01f ? myWheelRadius : 0.01f;

	for (int i = 0; i < 4; ++i)
	{
		const WheelState& ws = myWheelState[i];
		WheelContact& c = myWheelContacts[i];

		const Vector3f chassisAnchorWorld = TransformPointNoScale(chassisTransform, myChassisAnchorLocal[i]);
		c.worldPos = chassisAnchorWorld + axisWorld * ws.L_spring;
		c.normal = ws.contactNormal;
		c.contactPoint = ws.contactPoint;
		c.gap = ws.grounded ? (c.worldPos - ws.contactPoint).Dot(ws.contactNormal) - safeR : 0.f;
		c.compression = myMaxLength > 0.0001f ? Clamp01(1.f - ws.L_spring / myMaxLength) : 0.f;
		c.wheelLoad = ws.F_z;
		c.wheelAngularSpeed = ws.omega;
		c.slipRatio = ws.kappa;
		c.slipAngle = ws.alpha;
		c.grounded = ws.grounded;
	}
}