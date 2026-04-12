#include <stdafx.h>
#include "RigidbodyComponent.h"
#include "PhysicsWorld.h"
#include <Models/ModelInstance.h>
#include <algorithm>
#include <fstream>

namespace
{
	Vector3f SafeNormalize(const Vector3f& v, const Vector3f& fallback)
	{
		const float len = v.Length();
		if (len <= 0.0001f) return fallback;
		return v * (1.0f / len);
	}

	bool NearlyEqual(float a, float b, float epsilon = 0.01f)
	{
		return std::fabs(a - b) <= epsilon;
	}

	bool RotatorNearlyEqual(const Rotator& a, const Rotator& b, float epsilon = 0.01f)
	{
		return NearlyEqual(a.Roll, b.Roll, epsilon) &&
			NearlyEqual(a.Pitch, b.Pitch, epsilon) &&
			NearlyEqual(a.Yaw, b.Yaw, epsilon);
	}
}

RigidbodyComponent::~RigidbodyComponent()
{
	PhysicsWorld::Get().RemoveBody(this);
}

void RigidbodyComponent::SyncOrientationFromOwner()
{
	if (!myOwner) return;
	const Rotator rotDeg = myOwner->GetTransform().GetRotation();
	const Vector3f rotRad = rotDeg * Math::DegToRad;
	myOrientation = Quaternionf(rotRad);
	myOrientation.Normalize();
	myOrientationInitialized = true;
	myLastOwnerRotation = rotDeg;
}

void RigidbodyComponent::SyncOwnerRotationFromOrientation()
{
	if (!myOwner) return;
	myOrientation.Normalize();
	const Matrix4x4f rotationMatrix = Matrix4x4f::CreateRotationMatrixFromNormalizedQuaternion(myOrientation);
	Vector3f position;
	Vector3f rotation;
	Vector3f scale;
	rotationMatrix.DecomposeMatrix(position, rotation, scale);
	myOwner->SetRotation(rotation);
	myLastOwnerRotation = rotation;
}

Vector3f RigidbodyComponent::GetBodyInertiaDiagonal() const
{
	if (GetColliderType() == ColliderType::Sphere)
	{
		const float radius = mySphereRadius > 0.0001f ? mySphereRadius : 0.0001f;
		const float mass = myMass > 0.0001f ? myMass : 0.0001f;
		const float inertiaValue = 0.4f * mass * radius * radius;
		return
		{
			(std::max)(inertiaValue * myInertiaScale.X, 0.0001f),
			(std::max)(inertiaValue * myInertiaScale.Y, 0.0001f),
			(std::max)(inertiaValue * myInertiaScale.Z, 0.0001f)
		};
	}

	const float ex = myHalfExtents.X;
	const float ey = myHalfExtents.Y;
	const float ez = myHalfExtents.Z;
	const float mass = myMass > 0.0001f ? myMass : 0.0001f;

	Vector3f inertia;
	inertia.X = (mass / 3.0f) * (ey * ey + ez * ez) * myInertiaScale.X;
	inertia.Y = (mass / 3.0f) * (ex * ex + ez * ez) * myInertiaScale.Y;
	inertia.Z = (mass / 3.0f) * (ex * ex + ey * ey) * myInertiaScale.Z;

	if (inertia.X < 0.0001f) inertia.X = 0.0001f;
	if (inertia.Y < 0.0001f) inertia.Y = 0.0001f;
	if (inertia.Z < 0.0001f) inertia.Z = 0.0001f;
	return inertia;
}

Quaternionf RigidbodyComponent::GetWorldOrientation() const
{
	if (myOrientationInitialized)
	{
		return myOrientation;
	}
	if (myOwner)
	{
		return Quaternionf(myOwner->GetTransform().GetRotation() * Math::DegToRad);
	}
	return Quaternionf();
}

void RigidbodyComponent::OnStart()
{
	SyncOrientationFromOwner();
	PhysicsWorld::Get().AddBody(this);
}

Vector3f RigidbodyComponent::GetCenterOfMassWorld() const
{
	if (!myOwner) return {};
	const Quaternionf q = GetWorldOrientation();
	return myOwner->GetLocation() + Quaternionf::RotateVectorByQuaternion(q, myCenterOfMassLocal);
}

Vector3f RigidbodyComponent::GetColliderCenterWorld() const
{
	if (!myOwner) return {};
	const Quaternionf q = GetWorldOrientation();
	return myOwner->GetLocation() + Quaternionf::RotateVectorByQuaternion(q, myColliderCenterLocal);
}

Vector3f RigidbodyComponent::InertiaMultiplyWorld(const Vector3f& aWorldVector) const
{
	const Quaternionf q = GetWorldOrientation();
	const Vector3f axisX = SafeNormalize(q.GetRight(), { 1.0f, 0.0f, 0.0f });
	const Vector3f axisY = SafeNormalize(q.GetUp(), { 0.0f, 1.0f, 0.0f });
	const Vector3f axisZ = SafeNormalize(q.GetForward(), { 0.0f, 0.0f, 1.0f });
	const Vector3f inertia = GetBodyInertiaDiagonal();

	return
		axisX * (axisX.Dot(aWorldVector) * inertia.X) +
		axisY * (axisY.Dot(aWorldVector) * inertia.Y) +
		axisZ * (axisZ.Dot(aWorldVector) * inertia.Z);
}

Vector3f RigidbodyComponent::InverseInertiaMultiplyWorld(const Vector3f& aWorldVector) const
{
	const Quaternionf q = GetWorldOrientation();
	const Vector3f axisX = SafeNormalize(q.GetRight(), { 1.0f, 0.0f, 0.0f });
	const Vector3f axisY = SafeNormalize(q.GetUp(), { 0.0f, 1.0f, 0.0f });
	const Vector3f axisZ = SafeNormalize(q.GetForward(), { 0.0f, 0.0f, 1.0f });
	const Vector3f inertia = GetBodyInertiaDiagonal();

	return
		axisX * (axisX.Dot(aWorldVector) / inertia.X) +
		axisY * (axisY.Dot(aWorldVector) / inertia.Y) +
		axisZ * (axisZ.Dot(aWorldVector) / inertia.Z);
}

Vector3f RigidbodyComponent::GetVelocityAtPoint(const Vector3f& aWorldPoint) const
{
	const Vector3f r = aWorldPoint - GetCenterOfMassWorld();
	return myVelocity + myAngularVelocity.Cross(r);
}

Vector3f RigidbodyComponent::GetAverageContactNormal() const
{
	if (myContactCount <= 0)
	{
		return { 0.0f, 1.0f, 0.0f };
	}
	return SafeNormalize(myAccumulatedContactNormal, { 0.0f, 1.0f, 0.0f });
}

Vector3f RigidbodyComponent::GetAverageContactPoint() const
{
	if (myContactCount <= 0)
	{
		return GetColliderCenterWorld();
	}
	return myAccumulatedContactPoint * (1.0f / static_cast<float>(myContactCount));
}

Vector3f RigidbodyComponent::GetAverageTerrainContactNormal() const
{
	if (myTerrainContactCount <= 0)
	{
		return { 0.0f, 1.0f, 0.0f };
	}
	return SafeNormalize(myAccumulatedTerrainContactNormal, { 0.0f, 1.0f, 0.0f });
}

Vector3f RigidbodyComponent::GetAverageTerrainContactPoint() const
{
	if (myTerrainContactCount <= 0)
	{
		return GetColliderCenterWorld();
	}
	return myAccumulatedTerrainContactPoint * (1.0f / static_cast<float>(myTerrainContactCount));
}

float RigidbodyComponent::GetTerrainLoadEstimate() const
{
	if (myLastPhysicsStepDeltaTime <= 0.0001f)
	{
		return 0.0f;
	}
	return myAccumulatedTerrainNormalImpulse / myLastPhysicsStepDeltaTime;
}

void RigidbodyComponent::AddForceAccumulatedAtPoint(const Vector3f& aForce, const Vector3f& aWorldPoint)
{
	AddForceAccumulated(aForce);
	const Vector3f r = aWorldPoint - GetCenterOfMassWorld();
	AddTorqueAccumulated(r.Cross(aForce));
}

void RigidbodyComponent::BeginPhysicsStep()
{
	myAccumulatedContactNormal = { 0.0f, 0.0f, 0.0f };
	myAccumulatedContactPoint = { 0.0f, 0.0f, 0.0f };
	myAccumulatedTerrainContactNormal = { 0.0f, 0.0f, 0.0f };
	myAccumulatedTerrainContactPoint = { 0.0f, 0.0f, 0.0f };
	myAccumulatedTerrainNormalImpulse = 0.0f;
	myContactCount = 0;
	myTerrainContactCount = 0;
}

void RigidbodyComponent::RegisterContact(const Vector3f& aNormal, const Vector3f& aPoint, bool aIsTerrainContact, float aNormalImpulse)
{
	myAccumulatedContactNormal += SafeNormalize(aNormal, { 0.0f, 1.0f, 0.0f });
	myAccumulatedContactPoint += aPoint;
	++myContactCount;
	if (aIsTerrainContact)
	{
		myAccumulatedTerrainContactNormal += SafeNormalize(aNormal, { 0.0f, 1.0f, 0.0f });
		myAccumulatedTerrainContactPoint += aPoint;
		myAccumulatedTerrainNormalImpulse += aNormalImpulse > 0.0f ? aNormalImpulse : 0.0f;
		++myTerrainContactCount;
	}
}

void RigidbodyComponent::AddImpulseAtPoint(const Vector3f& anImpulse, const Vector3f& aWorldPoint)
{
	if (myIsStatic || myMass <= 0.0001f) return;
	myVelocity += anImpulse * (1.0f / myMass);
	const Vector3f r = aWorldPoint - GetCenterOfMassWorld();
	myAngularVelocity += InverseInertiaMultiplyWorld(r.Cross(anImpulse));
}

void RigidbodyComponent::ApplyPositionImpulseAtPoint(const Vector3f& anImpulse, const Vector3f& aWorldPoint)
{
	if (myIsStatic || myMass <= 0.0001f || !myOwner) return;
	if (!myOrientationInitialized)
	{
		SyncOrientationFromOwner();
	}

	myOwner->SetLocation(myOwner->GetLocation() + (anImpulse * (1.0f / myMass)));

	const Vector3f r = aWorldPoint - GetCenterOfMassWorld();
	const Vector3f angularStep = InverseInertiaMultiplyWorld(r.Cross(anImpulse));
	const float angularStepLength = angularStep.Length();
	if (angularStepLength > 0.0001f)
	{
		const Vector3f angularAxis = angularStep * (1.0f / angularStepLength);
		const Quaternionf delta(angularAxis, angularStepLength);
		myOrientation = (delta * myOrientation).GetNormalized();
		SyncOwnerRotationFromOrientation();
	}
}

void RigidbodyComponent::Update(float aDeltaTime)
{
	(void)aDeltaTime;
	if (!myOwner)
	{
		return;
	}

	const Rotator ownerRotation = myOwner->GetTransform().GetRotation();
	if (!myOrientationInitialized || !RotatorNearlyEqual(ownerRotation, myLastOwnerRotation))
	{
		SyncOrientationFromOwner();
	}
}

void RigidbodyComponent::Simulate(float aDeltaTime)
{
	if (myIsStatic || myOwner == nullptr) return;
	if (!myOrientationInitialized)
	{
		SyncOrientationFromOwner();
	}
	myLastPhysicsStepDeltaTime = aDeltaTime;

	// diagnostic log (sSimDiagEnabled)
	static bool          sSimDiagEnabled = false;
	static std::ofstream sSimLog;
	static int           sSimFrame = 0;
	static bool          sSimHeaderWritten = false;
	if (sSimDiagEnabled && !sSimHeaderWritten)
	{
		sSimLog.open("sim_diag.csv");
		if (sSimLog.is_open())
			sSimLog <<
			    "frame,"
			    "vy_pre,"
			    "accum_fy,accum_torque_x,accum_torque_y,accum_torque_z,"
			    "vy_post_force,delta_force,"
			    "ow_pre_x,ow_pre_y,ow_pre_z,"
			    "ow_post_x,ow_post_y,ow_post_z,delta_ow_x,delta_ow_y,delta_ow_z,"
			    "gravity_delta_vy,vy_post_gravity,"
			    "drag_factor,vy_post_drag,delta_drag,"
			    "vy_final\n";
		sSimHeaderWritten = true;
	}
	const bool doLog = sSimDiagEnabled && sSimLog.is_open() && sSimFrame < 2000;

	const float vy_pre          = myVelocity.Y;
	const Vector3f accum_f      = myAccumulatedForce;
	const Vector3f accum_t      = myAccumulatedTorque;
	const Vector3f ow_pre       = myAngularVelocity;

	if (myAccumulatedForce.X != 0.f || myAccumulatedForce.Y != 0.f || myAccumulatedForce.Z != 0.f)
	{
		myVelocity = myVelocity + (myAccumulatedForce * (aDeltaTime / myMass));
		myAccumulatedForce = {};
	}

	const float vy_post_force = myVelocity.Y;

	if (myAccumulatedTorque.X != 0.f || myAccumulatedTorque.Y != 0.f || myAccumulatedTorque.Z != 0.f)
	{
		const Vector3f angularMomentum = InertiaMultiplyWorld(myAngularVelocity);
		const Vector3f gyroscopic = myAngularVelocity.Cross(angularMomentum);
		const Vector3f angularAcceleration = InverseInertiaMultiplyWorld(myAccumulatedTorque - gyroscopic);
		myAngularVelocity += angularAcceleration * aDeltaTime;
		myAccumulatedTorque = {};
	}

	const Vector3f ow_post = myAngularVelocity;

	const float gravity_delta = myUseGravity ? ourGravity * aDeltaTime : 0.f;
	if (myUseGravity)
	{
		myVelocity.Y += ourGravity * aDeltaTime;
	}

	const float vy_post_gravity = myVelocity.Y;
	const float linearDrag = 1.0f - myLinearDrag * aDeltaTime;
	const float dragFactor = linearDrag > 0.0f ? linearDrag : 0.0f;
	myVelocity = myVelocity * dragFactor;

	const float vy_post_drag = myVelocity.Y;

	const float angularDrag = 1.0f - myAngularDrag * aDeltaTime;
	myAngularVelocity = myAngularVelocity * (angularDrag > 0.0f ? angularDrag : 0.0f);

	// ── Write sim log row ─────────────────────────────────────────────────────
	if (doLog)
	{
		sSimLog
		    << sSimFrame << ','
		    << vy_pre << ','
		    << accum_f.Y << ',' << accum_t.X << ',' << accum_t.Y << ',' << accum_t.Z << ','
		    << vy_post_force << ',' << (vy_post_force - vy_pre) << ','
		    << ow_pre.X  << ',' << ow_pre.Y  << ',' << ow_pre.Z  << ','
		    << ow_post.X << ',' << ow_post.Y << ',' << ow_post.Z << ','
		    << (ow_post.X - ow_pre.X) << ',' << (ow_post.Y - ow_pre.Y) << ',' << (ow_post.Z - ow_pre.Z) << ','
		    << gravity_delta << ',' << vy_post_gravity << ','
		    << dragFactor << ',' << vy_post_drag << ',' << (vy_post_drag - vy_post_gravity) << ','
		    << myVelocity.Y << '\n';
		sSimLog.flush();
		++sSimFrame;
	}
	// ── End sim log write ─────────────────────────────────────────────────────

	Vector3f pos = myOwner->GetLocation();
	pos = pos + (myVelocity * aDeltaTime);
	myOwner->SetLocation(pos);

	const float angularSpeed = myAngularVelocity.Length();
	if (angularSpeed > 0.0001f)
	{
		const Vector3f axis = myAngularVelocity * (1.0f / angularSpeed);
		const Quaternionf delta(axis, angularSpeed * aDeltaTime);
		myOrientation = (delta * myOrientation).GetNormalized();
		SyncOwnerRotationFromOrientation();
	}
}

AABB RigidbodyComponent::GetAABB() const
{
	if (myOwner == nullptr) return {};

	if (GetColliderType() == ColliderType::Sphere)
	{
		const Sphere sphere = GetSphere();
		const Vector3f radiusVec = { sphere.myRadius, sphere.myRadius, sphere.myRadius };
		return
		{
			sphere.myCenter - radiusVec,
			sphere.myCenter + radiusVec
		};
	}

	const OBB obb = GetOBB();
	Vector3f absAxis0 = { std::fabs(obb.myAxis[0].X), std::fabs(obb.myAxis[0].Y), std::fabs(obb.myAxis[0].Z) };
	Vector3f absAxis1 = { std::fabs(obb.myAxis[1].X), std::fabs(obb.myAxis[1].Y), std::fabs(obb.myAxis[1].Z) };
	Vector3f absAxis2 = { std::fabs(obb.myAxis[2].X), std::fabs(obb.myAxis[2].Y), std::fabs(obb.myAxis[2].Z) };

	Vector3f worldExtents =
		absAxis0 * obb.myExtents.X +
		absAxis1 * obb.myExtents.Y +
		absAxis2 * obb.myExtents.Z;

	return
	{
		obb.myCenter - worldExtents,
		obb.myCenter + worldExtents
	};
}

OBB RigidbodyComponent::GetOBB() const
{
	if (myOwner == nullptr) return {};

	const Quaternionf q = GetWorldOrientation();
	OBB obb;
	obb.myCenter = GetColliderCenterWorld();
	obb.myAxis[0] = SafeNormalize(q.GetRight(), { 1.0f, 0.0f, 0.0f });
	obb.myAxis[1] = SafeNormalize(q.GetUp(), { 0.0f, 1.0f, 0.0f });
	obb.myAxis[2] = SafeNormalize(q.GetForward(), { 0.0f, 0.0f, 1.0f });
	obb.myExtents = myHalfExtents;
	return obb;
}

Sphere RigidbodyComponent::GetSphere() const
{
	Sphere sphere;
	sphere.myCenter = GetColliderCenterWorld();
	sphere.myRadius = mySphereRadius;
	return sphere;
}
