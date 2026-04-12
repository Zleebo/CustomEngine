#include "RaycastVehicleController.h"
#include <Engine.h>
#include <Physics/RigidbodyComponent.h>
#include <cmath>
#include <fstream>

namespace
{
	constexpr float kPi             = 3.14159265358979f;
	constexpr float kDegToRad       = kPi / 180.f;
	constexpr float kRadToDeg       = 180.f / kPi;
	constexpr float kSweepTolerance = 0.02f;

	float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
	float AbsF(float v)  { return v < 0.f ? -v : v; }
	float SignOrZero(float v, float eps = 0.0001f)
	{ return v > eps ? 1.f : (v < -eps ? -1.f : 0.f); }

	Vector3f NormalizeOr(const Vector3f& v, const Vector3f& fb)

{
		const float len = v.Length();
		return len > 0.0001f ? v * (1.f / len) : fb;
	}

	Transform MakeNoScaleT(const Transform& t)
	{ Transform r = t; r.SetScale({1.f, 1.f, 1.f}); return r; }

	Vector3f TfmPoint(const Transform& t, const Vector3f& p)
	{ return MakeNoScaleT(t).TransformPosition(const_cast<Vector3f&>(p)); }

	Vector3f InvTfmPoint(const Transform& t, const Vector3f& p)
	{ return MakeNoScaleT(t).InverseTransformPosition(const_cast<Vector3f&>(p)); }

	Vector3f TfmVector(const Transform& t, const Vector3f& v)

{
		const Matrix4x4f m = MakeNoScaleT(t).GetMatrix(true);
		return m.GetRight() * v.X + m.GetUp() * v.Y + m.GetForward() * v.Z;
	}
}


RaycastVehicleController::RaycastVehicleController()
{
	EXPOSE_FLOAT_EX("Acceleration",       myAcceleration,           0.1f,  0.f, 400.f);
	EXPOSE_FLOAT_EX("Reverse Accel",      myReverseAccel,           0.1f,  0.f, 400.f);
	EXPOSE_FLOAT_EX("Max Speed",          myMaxSpeed,               0.1f,  1.f, 200.f);
	EXPOSE_FLOAT_EX("Brake Strength",     myBrakeStrength,          0.1f,  0.f,  50.f);
	EXPOSE_FLOAT_EX("Longitudinal Grip",  myLongitudinalGrip,       0.1f,  0.f,  50.f);
	EXPOSE_FLOAT_EX("Lateral Grip",       myLateralGrip,            0.1f,  0.f,  50.f);
	EXPOSE_FLOAT_EX("Tire Friction",      myTireFriction,          0.01f,  0.1f,  5.f);
	EXPOSE_FLOAT_EX("Rolling Resistance", myRollingResistance,    0.005f,  0.f,  0.2f);
	EXPOSE_FLOAT_EX("Wheel Radius",       myWheelRadius,           0.01f,  0.05f, 2.f);
	EXPOSE_FLOAT_EX("Rest Length",        myRestLength,            0.01f,  0.f,   1.f);
	EXPOSE_FLOAT_EX("Max Length",         myMaxLength,             0.01f,  0.01f, 2.f);
	EXPOSE_FLOAT_EX("Suspension Hz",      mySuspensionFrequencyHz, 0.05f,  0.1f, 20.f);
	EXPOSE_FLOAT_EX("Suspension Damping", mySuspensionDampingRatio,0.01f,  0.05f, 3.f);
	EXPOSE_FLOAT_EX("Anti Roll",          myAntiRoll,              10.f,   0.f, 5000.f);
	EXPOSE_FLOAT_EX("Upright Assist",     myUprightAssist,          1.f,   0.f,  200.f);
}

RaycastVehicleController::~RaycastVehicleController()
{
	PhysicsWorld::Get().RemoveVehicleController(this);
}

void RaycastVehicleController::OnStart()
{
	myRigidbody = myOwner ? myOwner->GetComponent<RigidbodyComponent>() : nullptr;
	if (myRigidbody)

{
		myRigidbody->myBounciness = 0.f;
		myRigidbody->myIgnoreTerrainCollision = true;
	}

	const Vector3f defaultOffsets[4] = {
		{-1.15f, -0.32f,  1.70f},  // FL
		{ 1.15f, -0.32f,  1.70f},  // FR
		{-1.15f, -0.32f, -1.70f},  // BL
		{ 1.15f, -0.32f, -1.70f},  // BR
	};
	for (int i = 0; i < 4; ++i)

{
		myConfig[i].anchorLocal = { defaultOffsets[i].X,
		                            defaultOffsets[i].Y + myRestLength,
		                            defaultOffsets[i].Z };
		myConfig[i].wheelRadius = myWheelRadius;
		myConfig[i].restLength  = myRestLength;
		myConfig[i].maxTravel   = myMaxLength;
		myConfig[i].isFront     = (i < 2);
		myConfig[i].isDriven    = (i >= 2);
		myState[i] = {};
	}

	ConfigureSpring();
	PhysicsWorld::Get().AddVehicleController(this);

	char buf[128];
	snprintf(buf, sizeof(buf),
		"[RaycastVehicle] OnStart — chassis RB: %s, IgnoreTerrain: %d\n",
		myRigidbody ? "ok" : "MISSING",
		myRigidbody ? (int)myRigidbody->myIgnoreTerrainCollision : -1);
	OutputDebugStringA(buf);
}

void RaycastVehicleController::ConfigureSpring()
{
	if (!myRigidbody) return;

	float lx = myConfig[0].anchorLocal.X, rx = lx;
	float rz = myConfig[0].anchorLocal.Z, fz = rz;
	for (int i = 1; i < 4; ++i)

{
		if (myConfig[i].anchorLocal.X < lx) lx = myConfig[i].anchorLocal.X;
		if (myConfig[i].anchorLocal.X > rx) rx = myConfig[i].anchorLocal.X;
		if (myConfig[i].anchorLocal.Z < rz) rz = myConfig[i].anchorLocal.Z;
		if (myConfig[i].anchorLocal.Z > fz) fz = myConfig[i].anchorLocal.Z;
	}
	const Vector3f com   = myRigidbody->myCenterOfMassLocal;
	const float    spanX = (rx - lx) > 0.0001f ? rx - lx : 1.f;
	const float    spanZ = (fz - rz) > 0.0001f ? fz - rz : 1.f;
	const float    rw    = Clamp((com.X - lx) / spanX, 0.f, 1.f);
	const float    fw    = Clamp((com.Z - rz) / spanZ, 0.f, 1.f);
	float w[4] = { fw*(1.f-rw), fw*rw, (1.f-fw)*(1.f-rw), (1.f-fw)*rw };
	float sum  = w[0]+w[1]+w[2]+w[3];
	if (sum < 0.0001f) { sum = 1.f; for (float& x : w) x = 0.25f; }

	const float omegaN = kPi * 2.f * (mySuspensionFrequencyHz > 0.1f ? mySuspensionFrequencyHz : 0.1f);
	const float zeta   = mySuspensionDampingRatio > 0.05f ? mySuspensionDampingRatio : 0.05f;
	for (int i = 0; i < 4; ++i)

{
		mySprungMass[i]     = myRigidbody->myMass * (w[i] / sum);
		mySpringK[i]        = mySprungMass[i] * omegaN * omegaN;
		mySpringC[i]        = 2.f * zeta * mySprungMass[i] * omegaN;
		myConfig[i].springK = mySpringK[i];
		myConfig[i].springC = mySpringC[i];
	}
	myConfigured = true;
}

void RaycastVehicleController::SetWheels(ModelInstance* aFL, ModelInstance* aFR,
                                          ModelInstance* aBL, ModelInstance* aBR)
{
	myWheels[0] = aFL;  myWheels[1] = aFR;
	myWheels[2] = aBL;  myWheels[3] = aBR;
	if (!myOwner) return;
	const Transform ot = myOwner->GetTransform();
	for (int i = 0; i < 4; ++i)

{
		if (!myWheels[i]) continue;
		const Vector3f wl = InvTfmPoint(ot, myWheels[i]->GetLocation());
		myConfig[i].anchorLocal = { wl.X, wl.Y + myRestLength, wl.Z };
		myState[i] = {};
	}
	ConfigureSpring();
}


void RaycastVehicleController::Update(float dt)
{
	(void)dt;
	if (!myRigidbody || !myOwner) return;
	if (!myConfigured) ConfigureSpring();

	auto& input = InputManager::Get();
	myCachedThrottle = 0.f;
	if (input.IsKeyDown('W')) myCachedThrottle += 1.f;
	if (input.IsKeyDown('S')) myCachedThrottle -= 1.f;
	myCachedSteer = 0.f;
	if (input.IsKeyDown('A')) myCachedSteer -= 1.f;
	if (input.IsKeyDown('D')) myCachedSteer += 1.f;
	myCachedBrake = input.IsKeyDown(VK_SPACE);
}


void RaycastVehicleController::ApplyVehicleForces(float dt)
{
	if (!myRigidbody || !myOwner || dt <= 0.0001f) return;

	const Transform ct  = myOwner->GetTransform();
	const Rotator   rot = ct.GetRotation();
	const float     yaw = rot.Yaw * kDegToRad;

	// World-up is the reference for all support forces.
	const Vector3f kWorldUp     = { 0.f, 1.f, 0.f };
	// drive/steer directions from yaw only
	const Vector3f bodyForward  = { std::sinf(yaw), 0.f,  std::cosf(yaw) };
	const Vector3f bodyRight    = { std::cosf(yaw), 0.f, -std::sinf(yaw) };
	// Actual chassis up for upright-assist error measurement.
	const Vector3f bodyUpActual = NormalizeOr(TfmVector(ct, {0.f, 1.f, 0.f}), kWorldUp);
	// Sweep follows chassis so it reaches terrain regardless of pitch/roll.
	const Vector3f sweepDown    = NormalizeOr(TfmVector(ct, {0.f,-1.f,0.f}), {0.f,-1.f,0.f});

	const float safeR      = myWheelRadius > 0.01f ? myWheelRadius : 0.01f;
	const float mass       = myRigidbody->myMass;
	const float chassisSpd = std::sqrtf(
		myRigidbody->myVelocity.X * myRigidbody->myVelocity.X +
		myRigidbody->myVelocity.Z * myRigidbody->myVelocity.Z);


	// Diagnostics
	static std::ofstream sDiag;
	static int  sDiagFrame = 0;
	static bool sDiagHdr   = false;
	if (!sDiagHdr)

{
		sDiag.open("raycar_diag.csv");
		if (sDiag.is_open())
			sDiag << "frame,wheel,grounded,rawLength,currentLength,xDot,"
			         "FzRaw,Fz,Fx,Fy,sumFz,"
			         "chassis_vx,chassis_vy,chassis_vz,"
			         "chassis_ox,chassis_oy,chassis_oz,"
			         "anchor_x,anchor_y,anchor_z\n";
		sDiagHdr = true;
	}

	int   groundedCount = 0;
	float sumFz         = 0.f;
	float compressions[4] = {};

	for (int i = 0; i < 4; ++i)

{
		RayWheelState&     ws  = myState[i];
		const WheelConfig& cfg = myConfig[i];

		// Sphere sweep
		const Vector3f anchor      = TfmPoint(ct, cfg.anchorLocal);
		const Vector3f sweepOrigin = anchor + kWorldUp * safeR;   // start above terrain so sphere at t=0 doesn't already penetrate
		const Vector3f sweepEnd    = anchor + sweepDown * cfg.maxTravel;

		PhysicsWorld::TerrainSweepHit hit = {};
		ws.grounded = PhysicsWorld::Get().SweepSphereAgainstTerrain(
			sweepOrigin, sweepEnd, safeR, kSweepTolerance, hit);

		if (ws.grounded)
	
	{
			ws.contactPoint  = hit.point;
			ws.contactNormal = NormalizeOr(hit.normal, kWorldUp);
		}
		else
	
	{
			ws.contactPoint  = sweepEnd;
			ws.contactNormal = kWorldUp;
		}

		// Compression
		float hitDist = cfg.maxTravel;
		if (ws.grounded)
	
	{
			const float L_raw = (hit.center - anchor).Dot(sweepDown);
			hitDist = std::isfinite(L_raw) ? Clamp(L_raw, 0.f, cfg.maxTravel) : cfg.maxTravel;
		}
		ws.L_spring     = hitDist;
		ws.dL           = 0.f;
		ws.kappa        = 0.f;
		ws.alpha        = 0.f;
		compressions[i] = cfg.restLength - hitDist;

		ws.F_z = 0.f;
		float F_z_raw = 0.f;

		// Suspension force
		// Simple spring-damper, force along world up.
		if (ws.grounded && compressions[i] > 0.f)
	
	{
			const float Fz_spring = cfg.springK * compressions[i];
			// Per-anchor vertical velocity correctly damps roll oscillations.
			const float Fz_damper = -cfg.springC * myRigidbody->GetVelocityAtPoint(anchor).Y;
			F_z_raw = Fz_spring + Fz_damper;

			// ceiling = spring at full travel
			const float maxFz = cfg.springK * cfg.maxTravel;
			ws.F_z = Clamp(F_z_raw, 0.f, maxFz);

			myRigidbody->AddForceAccumulatedAtPoint(kWorldUp * ws.F_z, anchor);
			sumFz += ws.F_z;
			++groundedCount;
		}

		// Tire forces
		ws.F_x = 0.f;
		ws.F_y = 0.f;

		if (ws.F_z > 1.f && compressions[i] > 0.005f)
	
	{
			// Steered direction (front axle only).
			const float    steerAngle = (cfg.isFront ? myCachedSteer * 28.f : 0.f) * kDegToRad;
			const Vector3f wheelFwd   = NormalizeOr(
				bodyForward * std::cosf(steerAngle) + bodyRight * std::sinf(steerAngle), bodyForward);
			// Lateral axis perpendicular to wheel forward in the horizontal plane.
			const Vector3f wheelRight = NormalizeOr(kWorldUp.Cross(wheelFwd), bodyRight);

			const Vector3f vPatch = myRigidbody->GetVelocityAtPoint(ws.contactPoint);
			const float    vLong  = vPatch.Dot(wheelFwd);
			const float    vLat   = vPatch.Dot(wheelRight);

			const float frictionBudget = myTireFriction * ws.F_z;

			// Longitudinal: throttle on driven wheels (evenly split across 2 rear wheels).
			float Fx = 0.f;
			if (cfg.isDriven && AbsF(myCachedThrottle) > 0.01f &&
			    (chassisSpd < myMaxSpeed || myCachedThrottle < 0.f))
		
		{
				const float accel = myCachedThrottle >= 0.f ? myAcceleration : myReverseAccel;
				Fx = myCachedThrottle * accel * mass * 0.5f;
			}

			// Brake.
			if (myCachedBrake)
				Fx += -SignOrZero(vLong, 0.05f) * myBrakeStrength * ws.F_z;

			// Rolling resistance when coasting.
			if (AbsF(myCachedThrottle) <= 0.01f && !myCachedBrake)
				Fx += -SignOrZero(vLong, 0.05f) * myRollingResistance * ws.F_z;

			Fx = Clamp(Fx, -frictionBudget, frictionBudget);

			// Lateral damping
			float Fy = -vLat * myLateralGrip;
			Fy = Clamp(Fy, -frictionBudget, frictionBudget);

			ws.F_x = Fx;
			ws.F_y = Fy;

			myRigidbody->AddForceAccumulatedAtPoint(
				wheelFwd * Fx + wheelRight * Fy, ws.contactPoint);
		}

		// Wheel spin for LateUpdate visuals only.
	
	{
			const float vLong = myRigidbody->GetVelocityAtPoint(ws.contactPoint).Dot(bodyForward);
			ws.omega     = (ws.F_z > 1.f) ? vLong / safeR : ws.omega * std::exp(-2.f * dt);
			ws.spinAngle += ws.omega * kRadToDeg * dt;
		}

		ws.loadedTime = ws.F_z > 1.f ? ws.loadedTime + dt : 0.f;

		// Per-wheel CSV log.
		if (sDiag.is_open() && sDiagFrame < 2000)
	
	{
			sDiag << sDiagFrame << ',' << i << ','
			      << (ws.grounded ? 1 : 0) << ','
			      << hitDist << ',' << ws.L_spring << ',' << ws.dL << ','
			      << F_z_raw << ',' << ws.F_z << ','
			      << ws.F_x << ',' << ws.F_y << ','
			      << sumFz << ','
			      << myRigidbody->myVelocity.X << ','
			      << myRigidbody->myVelocity.Y << ','
			      << myRigidbody->myVelocity.Z << ','
			      << myRigidbody->myAngularVelocity.X << ','
			      << myRigidbody->myAngularVelocity.Y << ','
			      << myRigidbody->myAngularVelocity.Z << ','
			      << anchor.X << ',' << anchor.Y << ',' << anchor.Z << '\n';
			if (i == 3) { sDiag.flush(); ++sDiagFrame; }
		}
	}

	// Anti-roll
	// Resist roll by applying opposing torques when left/right compressions differ.
	const int axlePairs[2][2] = { {0, 1}, {2, 3} };
	for (const auto& pair : axlePairs)

{
		const float diff = compressions[pair[0]] - compressions[pair[1]];
		myRigidbody->AddTorqueAccumulated(bodyForward * (myAntiRoll * diff));
	}

	// Angular damping
	// Always active.  Stronger in the air to prevent airborne tumbling.

{
		const float rate = (groundedCount < 2) ? 5.f : 1.5f;
		myRigidbody->myAngularVelocity =
			myRigidbody->myAngularVelocity * std::exp(-rate * dt);
	}

	// Upright assist
	// Continuously torques the car toward world-up so it behaves like a normal
	// game vehicle and does not flip or tumble from minor disturbances.
	if (myUprightAssist > 0.f)

{
		const Vector3f uprightErr = bodyUpActual.Cross(kWorldUp);
		myRigidbody->AddTorqueAccumulated(uprightErr * myUprightAssist);
	}

	myIsGrounded = groundedCount > 0;
	if (myIsGrounded) myAirTime = 0.f;
	else              myAirTime += dt;
}


void RaycastVehicleController::LateUpdate(float dt)
{
	if (!myRigidbody || !myOwner) return;

	const Transform  ct        = myOwner->GetTransform();
	const Vector3f   sweepDown = NormalizeOr(TfmVector(ct, {0.f,-1.f,0.f}), {0.f,-1.f,0.f});
	const Rotator    bodyRot   = ct.GetRotation();
	const float      steerMax  = 28.f;

	mySteerVisualAngle += (myCachedSteer * steerMax - mySteerVisualAngle)
	                      * Clamp(8.f * dt, 0.f, 1.f);

	for (int i = 0; i < 4; ++i)

{
		if (!myWheels[i]) continue;
		RayWheelState& ws = myState[i];

		const Vector3f anchor   = TfmPoint(ct, myConfig[i].anchorLocal);
		const Vector3f wheelPos = anchor + sweepDown * ws.L_spring;
		myWheels[i]->SetLocation(wheelPos);

		ws.spinAngle += ws.omega * kRadToDeg * dt;

		const bool isRight = (i == 1 || i == 3);
		Rotator wrot = bodyRot;
		wrot.Pitch += isRight ? -ws.spinAngle : ws.spinAngle;
		if (isRight) wrot.Yaw += 180.f;
		if (i < 2)   wrot.Yaw += mySteerVisualAngle;
		myWheels[i]->SetRotation(wrot);
	}
}
