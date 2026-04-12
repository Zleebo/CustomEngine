#include <stdafx.h>
#include "SuspensionConstraintComponent.h"

#include "PhysicsWorld.h"
#include "RigidbodyComponent.h"
#include <Models/ModelInstance.h>

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kConstraintSlop = 0.01f;
	constexpr float kConstraintBaumgarte = 0.08f;
	constexpr float kMaxLinearCorrection = 0.10f;
	constexpr float kSupportAxialTolerance = 0.08f;
	constexpr float kSupportLateralTolerance = 0.35f;
	constexpr float kTwoPi = 6.28318530717958647692f;
	constexpr float kFallbackContactTolerance = 0.02f;
	constexpr float kSupportPenetrationSlop = 0.002f;
	constexpr float kSupportVelocityBaumgarte = 0.10f;
	constexpr float kSupportPositionBaumgarte = 0.20f;

	Vector3f NormalizeOr(const Vector3f& value, const Vector3f& fallback)
	{
		const float len = value.Length();
		if (len <= 0.0001f) return fallback;
		return value * (1.0f / len);
	}

	Transform MakeNoScaleTransform(const Transform& transform)
	{
		Transform result = transform;
		result.SetScale({ 1.0f, 1.0f, 1.0f });
		return result;
	}

	Vector3f TransformPointNoScale(const Transform& transform, const Vector3f& localPoint)
	{
		Transform noScale = MakeNoScaleTransform(transform);
		Vector3f point = localPoint;
		return noScale.TransformPosition(point);
	}

	Vector3f InverseTransformPointNoScale(const Transform& transform, const Vector3f& worldPoint)
	{
		Transform noScale = MakeNoScaleTransform(transform);
		Vector3f point = worldPoint;
		return noScale.InverseTransformPosition(point);
	}

	Vector3f TransformVectorNoScale(const Transform& transform, const Vector3f& localVector)
	{
		const Matrix4x4f matrix = MakeNoScaleTransform(transform).GetMatrix(true);
		return
			matrix.GetRight() * localVector.X +
			matrix.GetUp() * localVector.Y +
			matrix.GetForward() * localVector.Z;
	}

	float ComputeConstraintEffectiveMass(
		const RigidbodyComponent* wheelBody,
		const RigidbodyComponent* chassisBody,
		const Vector3f& wheelPoint,
		const Vector3f& chassisPoint,
		const Vector3f& direction)
	{
		const float wheelInvMass = (!wheelBody || wheelBody->myIsStatic || wheelBody->myMass <= 0.0001f) ? 0.0f : (1.0f / wheelBody->myMass);
		const float chassisInvMass = (!chassisBody || chassisBody->myIsStatic || chassisBody->myMass <= 0.0001f) ? 0.0f : (1.0f / chassisBody->myMass);

		float angularTerm = 0.0f;
		if (wheelBody && !wheelBody->myIsStatic)
		{
			const Vector3f rw = wheelPoint - wheelBody->GetCenterOfMassWorld();
			const Vector3f wheelAngular = wheelBody->InverseInertiaMultiplyWorld(rw.Cross(direction)).Cross(rw);
			angularTerm += direction.Dot(wheelAngular);
		}
		if (chassisBody && !chassisBody->myIsStatic)
		{
			const Vector3f rc = chassisPoint - chassisBody->GetCenterOfMassWorld();
			const Vector3f chassisAngular = chassisBody->InverseInertiaMultiplyWorld(rc.Cross(direction)).Cross(rc);
			angularTerm += direction.Dot(chassisAngular);
		}

		return wheelInvMass + chassisInvMass + angularTerm;
	}

	float ClampFloat(float value, float minValue, float maxValue)
	{
		if (value < minValue) return minValue;
		if (value > maxValue) return maxValue;
		return value;
	}
}

SuspensionConstraintComponent::SuspensionConstraintComponent()
{
	EXPOSE_FLOAT_EX("Rest Length", myRestLength, 0.01f, 0.0f, 2.0f);
	EXPOSE_FLOAT_EX("Max Length", myMaxLength, 0.01f, 0.01f, 4.0f);
	EXPOSE_FLOAT_EX("Suspension Frequency", mySuspensionFrequencyHz, 0.05f, 0.1f, 20.0f);
	EXPOSE_FLOAT_EX("Suspension Damping Ratio", mySuspensionDampingRatio, 0.01f, 0.1f, 3.0f);
	EXPOSE_BOOL("Auto Configure", myAutoConfigure);
	EXPOSE_VEC3("Chassis Anchor", myChassisAnchorLocal);
	EXPOSE_VEC3("Wheel Anchor", myWheelAnchorLocal);
	EXPOSE_VEC3("Suspension Axis", mySuspensionAxisLocal);
}

SuspensionConstraintComponent::~SuspensionConstraintComponent()
{
	PhysicsWorld::Get().RemoveSuspensionConstraint(this);
}

void SuspensionConstraintComponent::OnStart()
{
	TryBindBodies();
	UpdateDerivedSuspensionParameters();
	PhysicsWorld::Get().AddSuspensionConstraint(this);
}

void SuspensionConstraintComponent::SetSprungMass(float aSprungMass)
{
	mySprungMass = aSprungMass > 0.05f ? aSprungMass : 0.05f;
	UpdateDerivedSuspensionParameters();
}

void SuspensionConstraintComponent::ResetStepMetrics()
{
	myLastAxialLength = 0.0f;
	myLastClampedLength = 0.0f;
	myLastEffectiveRestLength = 0.0f;
	myLastEffectiveMaxLength = 0.0f;
	myLastSpringForceMagnitude = 0.0f;
	myLastLateralForceMagnitude = 0.0f;
	myLastLimitForceMagnitude = 0.0f;
	myLastUnclampedForceMagnitude = 0.0f;
	myLastTotalForceMagnitude = 0.0f;
	myLastMaxConstraintForce = 0.0f;
	myLastSpringScale = 1.0f;
	myLastLateralScale = 0.0f;
	myLastWheelSupported = false;
	myLastAxialConstraintError = 0.0f;
	myLastLateralConstraintError = 0.0f;
	myHasGroundContact = false;
	myHasSupportContact = false;
	myVelocitySolveSupported = false;
	myHasPredictedGroundContact = false;
	myGroundContactPoint = { 0.0f, 0.0f, 0.0f };
	myGroundContactNormal = { 0.0f, 1.0f, 0.0f };
	myWheelLoad = 0.0f;
	myLastWheelCenter = { 0.0f, 0.0f, 0.0f };
	mySupportedAxialLength = 0.0f;
	myPreviousSupportedAxialLength = 0.0f;
	mySupportedAxialVelocity = 0.0f;
	myLastSupportPenetration = 0.0f;
}

void SuspensionConstraintComponent::UpdateDerivedSuspensionParameters()
{
	const float clampedSprungMass = mySprungMass > 0.05f ? mySprungMass : 0.05f;
	const float clampedFrequency = mySuspensionFrequencyHz > 0.1f ? mySuspensionFrequencyHz : 0.1f;
	const float clampedDampingRatio = mySuspensionDampingRatio > 0.05f ? mySuspensionDampingRatio : 0.05f;
	const float omega = kTwoPi * clampedFrequency;
	myDerivedSpringStrength = clampedSprungMass * omega * omega;
	myDerivedSpringDamping = 2.0f * clampedSprungMass * clampedDampingRatio * omega;
}

void SuspensionConstraintComponent::TryBindBodies()
{
	if (!myOwner)
	{
		return;
	}

	if (!myWheelBody)
	{
		myWheelBody = myOwner->GetComponent<RigidbodyComponent>();
	}

	ModelInstance* parent = myOwner->GetParent();
	if (parent != myBoundParent)
	{
		myBoundParent = parent;
		myChassisBody = nullptr;
		myConfigured = false;
	}

	if (!myChassisBody && parent)
	{
		myChassisBody = parent->GetComponent<RigidbodyComponent>();
	}

	if (myAutoConfigure && !myConfigured && myWheelBody && myChassisBody && parent)
	{
		AutoConfigureFromCurrentPose();
	}
}

void SuspensionConstraintComponent::AutoConfigureFromCurrentPose()
{
	ModelInstance* parent = myOwner ? myOwner->GetParent() : nullptr;
	if (!myOwner || !parent)
	{
		return;
	}

	const Transform parentTransform = parent->GetTransform();
	const Vector3f wheelWorld = myOwner->GetLocation();
	const Vector3f wheelLocalToParent = InverseTransformPointNoScale(parentTransform, wheelWorld);
	const Vector3f axisLocal = NormalizeOr(mySuspensionAxisLocal, { 0.0f, -1.0f, 0.0f });
	myChassisAnchorLocal = wheelLocalToParent - axisLocal * myRestLength;
	myWheelAnchorLocal = { 0.0f, 0.0f, 0.0f };
	myConfigured = true;
}

void SuspensionConstraintComponent::Update(float aDeltaTime)
{
	(void)aDeltaTime;
	TryBindBodies();
	UpdateDerivedSuspensionParameters();
	if (!HasValidBodies() || !myOwner || !myOwner->GetParent())
	{
		ResetStepMetrics();
	}
}

void SuspensionConstraintComponent::RefreshGroundContactState()
{
	myHasGroundContact = false;
	myHasSupportContact = false;
	myHasPredictedGroundContact = false;
	myGroundContactPoint = { 0.0f, 0.0f, 0.0f };
	myGroundContactNormal = { 0.0f, 1.0f, 0.0f };
	myWheelLoad = 0.0f;
	myLastWheelSupported = false;
	myLastWheelCenter = myWheelBody ? myWheelBody->GetColliderCenterWorld() : Vector3f{};
	myLastSupportPenetration = 0.0f;
	myPreviousSupportedAxialLength = mySupportedAxialLength;

	if (!HasValidBodies() || !myOwner || !myOwner->GetParent() || !myWheelBody)
	{
		mySupportedAxialVelocity = 0.0f;
		return;
	}

	const Transform chassisTransform = myOwner->GetParent()->GetTransform();
	const Transform wheelTransform = myOwner->GetTransform();
	const Vector3f axisWorld = NormalizeOr(TransformVectorNoScale(chassisTransform, mySuspensionAxisLocal), { 0.0f, -1.0f, 0.0f });
	const float effectiveRestLength = (std::max)(myRestLength, 0.0f);
	const float effectiveMaxLength = (std::max)(myMaxLength, effectiveRestLength);
	const Vector3f chassisAnchorWorld = TransformPointNoScale(chassisTransform, myChassisAnchorLocal);
	const Vector3f wheelAnchorWorld = TransformPointNoScale(wheelTransform, myWheelAnchorLocal);
	const Vector3f centerOffset = myWheelBody->GetColliderCenterWorld() - wheelAnchorWorld;

	auto StoreSupportedStateFromContact = [&](const Vector3f& contactPoint, const Vector3f& contactNormal)
		{
			myHasGroundContact = true;
			myGroundContactPoint = contactPoint;
			myGroundContactNormal = NormalizeOr(contactNormal, { 0.0f, 1.0f, 0.0f });
			myLastWheelSupported = true;

			const Vector3f supportCenter = myGroundContactPoint + myGroundContactNormal * myWheelBody->mySphereRadius;
			const Vector3f supportAnchor = supportCenter - centerOffset;
			const Vector3f supportDelta = supportAnchor - chassisAnchorWorld;
			mySupportedAxialLength = ClampFloat(supportDelta.Dot(axisWorld), 0.0f, effectiveMaxLength);

			const float signedDistance = (myWheelBody->GetColliderCenterWorld() - myGroundContactPoint).Dot(myGroundContactNormal);
			myLastSupportPenetration = myWheelBody->mySphereRadius - signedDistance;
		};

	if (myWheelBody->HasTerrainContact())
	{
		StoreSupportedStateFromContact(
			myWheelBody->GetAverageTerrainContactPoint(),
			myWheelBody->GetAverageTerrainContactNormal());
	}
	else if (myWheelBody->GetColliderType() == RigidbodyComponent::ColliderType::Sphere)
	{
		const Vector3f startCenter = chassisAnchorWorld + centerOffset;
		const Vector3f endCenter = chassisAnchorWorld + axisWorld * effectiveMaxLength + centerOffset;

		PhysicsWorld::TerrainSweepHit sweepHit = {};
		if (PhysicsWorld::Get().SweepSphereAgainstTerrain(startCenter, endCenter, myWheelBody->mySphereRadius, kFallbackContactTolerance, sweepHit))
		{
			myHasPredictedGroundContact = true;
			StoreSupportedStateFromContact(sweepHit.point, sweepHit.normal);
		}
	}

	mySupportedAxialVelocity = 0.0f;
}

void SuspensionConstraintComponent::UpdateSupportState(float anAxialLength, float anEffectiveMaxLength, float aLateralConstraintError)
{
	const float effectiveRestLength = (std::max)(myRestLength, 0.0f);
	const float minSupportAxial = -(effectiveRestLength + kSupportAxialTolerance);
	const bool withinSupportTravel =
		anAxialLength >= minSupportAxial &&
		anAxialLength <= (anEffectiveMaxLength + kSupportAxialTolerance);
	const bool withinSupportLateral = aLateralConstraintError <= kSupportLateralTolerance;
	myHasSupportContact = myHasGroundContact && withinSupportTravel && withinSupportLateral;
	myLastWheelSupported = myHasSupportContact;
}

void SuspensionConstraintComponent::ApplySpringForces(float aDeltaTime)
{
	TryBindBodies();
	UpdateDerivedSuspensionParameters();
	if (!HasValidBodies() || !myOwner || !myOwner->GetParent() || aDeltaTime <= 0.0001f)
	{
		ResetStepMetrics();
		return;
	}

	const Transform chassisTransform = myOwner->GetParent()->GetTransform();
	const Transform wheelTransform = myOwner->GetTransform();
	const Vector3f axisWorld = NormalizeOr(TransformVectorNoScale(chassisTransform, mySuspensionAxisLocal), { 0.0f, -1.0f, 0.0f });
	const Vector3f chassisAnchorWorld = TransformPointNoScale(chassisTransform, myChassisAnchorLocal);
	const Vector3f wheelAnchorWorld = TransformPointNoScale(wheelTransform, myWheelAnchorLocal);
	const Vector3f delta = wheelAnchorWorld - chassisAnchorWorld;
	const float axialLength = delta.Dot(axisWorld);
	const Vector3f lateralOffset = delta - axisWorld * axialLength;
	const float lateralConstraintError = lateralOffset.Length();

	const float effectiveRestLength = (std::max)(myRestLength, 0.0f);
	const float effectiveMaxLength = (std::max)(myMaxLength, effectiveRestLength);

	RefreshGroundContactState();

	// snap wheel out of terrain and kill penetrating velocity
	if (myHasGroundContact && myLastSupportPenetration > 0.0f && myWheelBody->myOwner)
	{
		const Vector3f supportCenter = myGroundContactPoint + myGroundContactNormal * myWheelBody->mySphereRadius;
		const Quaternionf q = myWheelBody->GetWorldOrientation();
		const Vector3f colliderOffset = Quaternionf::RotateVectorByQuaternion(q, myWheelBody->myColliderCenterLocal);
		myWheelBody->myOwner->SetLocation(supportCenter - colliderOffset);
		const float normalVel = myWheelBody->myVelocity.Dot(myGroundContactNormal);
		if (normalVel < 0.0f)
		{
			myWheelBody->myVelocity = myWheelBody->myVelocity - myGroundContactNormal * normalVel;
		}
	}

	UpdateSupportState(axialLength, effectiveMaxLength, lateralConstraintError);

	const float springLength = myHasGroundContact ? mySupportedAxialLength : ClampFloat(axialLength, 0.0f, effectiveMaxLength);
	if (myHasGroundContact)
	{
		mySupportedAxialVelocity = (springLength - myPreviousSupportedAxialLength) / aDeltaTime;
	}
	else
	{
		const Vector3f chassisVel = myChassisBody->GetVelocityAtPoint(chassisAnchorWorld);
		const Vector3f wheelVel = myWheelBody->GetVelocityAtPoint(wheelAnchorWorld);
		mySupportedAxialVelocity = (wheelVel - chassisVel).Dot(axisWorld);
	}

	const float springDisplacement = springLength - effectiveRestLength;

	const bool springSupported = myHasSupportContact;
	const bool withinSpringTravel = springLength >= 0.0f && springLength <= effectiveMaxLength;
	const bool springForceEnabled = springSupported && withinSpringTravel;
	myVelocitySolveSupported = springForceEnabled;

	float springForceScalar = 0.0f;
	if (springForceEnabled)
	{
		springForceScalar = (-springDisplacement * myDerivedSpringStrength) - (mySupportedAxialVelocity * myDerivedSpringDamping);
		if (springForceScalar < 0.0f)
		{
			springForceScalar = 0.0f;
		}
	}

	// spring force on chassis only
	if (springForceScalar > 0.0001f)
	{
		const Vector3f springForce = axisWorld * springForceScalar;
		myChassisBody->AddForceAccumulatedAtPoint(springForce * -1.0f, chassisAnchorWorld);
	}

	myWheelLoad = springForceEnabled ? springForceScalar : 0.0f;
	myLastAxialLength = axialLength;
	myLastClampedLength = springLength;
	myLastEffectiveRestLength = effectiveRestLength;
	myLastEffectiveMaxLength = effectiveMaxLength;
	myLastSpringForceMagnitude = springForceScalar;
	myLastLateralForceMagnitude = 0.0f;
	myLastLimitForceMagnitude = 0.0f;
	myLastUnclampedForceMagnitude = springForceScalar;
	myLastTotalForceMagnitude = springForceScalar;
	myLastMaxConstraintForce = 0.0f;
	myLastSpringScale = 1.0f;
	myLastLateralScale = 0.0f;
}

void SuspensionConstraintComponent::SolveVelocityConstraints(float aDeltaTime)
{
	TryBindBodies();
	if (!HasValidBodies() || !myOwner || !myOwner->GetParent() || aDeltaTime <= 0.0001f)
	{
		return;
	}

	const Transform chassisTransform = myOwner->GetParent()->GetTransform();
	const Transform wheelTransform = myOwner->GetTransform();
	const Vector3f axisWorld = NormalizeOr(TransformVectorNoScale(chassisTransform, mySuspensionAxisLocal), { 0.0f, -1.0f, 0.0f });
	const Vector3f chassisAnchorWorld = TransformPointNoScale(chassisTransform, myChassisAnchorLocal);
	const Vector3f wheelAnchorWorld = TransformPointNoScale(wheelTransform, myWheelAnchorLocal);
	const Vector3f delta = wheelAnchorWorld - chassisAnchorWorld;
	const float axialLength = delta.Dot(axisWorld);
	const Vector3f lateralOffset = delta - axisWorld * axialLength;
	const float lateralConstraintError = lateralOffset.Length();

	const float effectiveRestLength = (std::max)(myRestLength, 0.0f);
	const float effectiveMaxLength = (std::max)(myMaxLength, effectiveRestLength);
	const float axialConstraintError =
		axialLength < 0.0f ? -axialLength :
		(axialLength > effectiveMaxLength ? axialLength - effectiveMaxLength : 0.0f);

	auto ApplyVelocityConstraint = [&](const Vector3f& direction, float errorLength)
		{
			if (errorLength <= kConstraintSlop)
			{
				return;
			}

			const Vector3f correctionDir = NormalizeOr(direction, axisWorld * -1.0f);
			const Vector3f chassisVel = myChassisBody->GetVelocityAtPoint(chassisAnchorWorld);
			const Vector3f wheelVel = myWheelBody->GetVelocityAtPoint(wheelAnchorWorld);
			const Vector3f relativeVelocity = wheelVel - chassisVel;
			const float relativeCorrectionSpeed = relativeVelocity.Dot(correctionDir);
			const float positionalBias = ((errorLength - kConstraintSlop) * kConstraintBaumgarte) / aDeltaTime;
			const float effectiveMass = ComputeConstraintEffectiveMass(myWheelBody, myChassisBody, wheelAnchorWorld, chassisAnchorWorld, correctionDir);
			if (effectiveMass > 0.0001f)
			{
				float correctiveImpulseMagnitude = -(relativeCorrectionSpeed - positionalBias) / effectiveMass;
				if (correctiveImpulseMagnitude > 0.0f)
				{
					const Vector3f correctiveImpulse = correctionDir * correctiveImpulseMagnitude;
					myWheelBody->AddImpulseAtPoint(correctiveImpulse, wheelAnchorWorld);
					myChassisBody->AddImpulseAtPoint(correctiveImpulse * -1.0f, chassisAnchorWorld);
				}
			}
		};

	auto ApplyHardLimitConstraint = [&](const Vector3f& direction)
		{
			const Vector3f correctionDir = NormalizeOr(direction, axisWorld * -1.0f);
			const Vector3f chassisVel = myChassisBody->GetVelocityAtPoint(chassisAnchorWorld);
			const Vector3f wheelVel = myWheelBody->GetVelocityAtPoint(wheelAnchorWorld);
			const Vector3f relativeVelocity = wheelVel - chassisVel;
			const float relativeCorrectionSpeed = relativeVelocity.Dot(correctionDir);
			if (relativeCorrectionSpeed >= 0.0f)
			{
				return;
			}

			const float effectiveMass = ComputeConstraintEffectiveMass(myWheelBody, myChassisBody, wheelAnchorWorld, chassisAnchorWorld, correctionDir);
			if (effectiveMass > 0.0001f)
			{
				const float correctiveImpulseMagnitude = -relativeCorrectionSpeed / effectiveMass;
				if (correctiveImpulseMagnitude > 0.0f)
				{
					const Vector3f correctiveImpulse = correctionDir * correctiveImpulseMagnitude;
					myWheelBody->AddImpulseAtPoint(correctiveImpulse, wheelAnchorWorld);
					myChassisBody->AddImpulseAtPoint(correctiveImpulse * -1.0f, chassisAnchorWorld);
				}
			}
		};

	if (lateralConstraintError > kConstraintSlop)
	{
		ApplyVelocityConstraint(lateralOffset * -1.0f, lateralConstraintError);
	}
	if (axialLength < 0.0f)
	{
		ApplyHardLimitConstraint(axisWorld);
	}
	else if (axialLength > effectiveMaxLength)
	{
		ApplyHardLimitConstraint(axisWorld * -1.0f);
	}

	// Support penetration is handled by the kinematic clamp in ApplySpringForces.
	// An impulse-based support constraint here would fight the spring force applied
	// in the same step and has no chassis coupling (previously passed nullptr for chassis).

	myLastAxialConstraintError = axialConstraintError;
	myLastLateralConstraintError = lateralConstraintError;
}

void SuspensionConstraintComponent::SolvePositionConstraints(float aDeltaTime)
{
	TryBindBodies();
	if (!HasValidBodies() || !myOwner || !myOwner->GetParent() || aDeltaTime <= 0.0001f)
	{
		return;
	}

	const float effectiveRestLength = (std::max)(myRestLength, 0.0f);
	const float effectiveMaxLength = (std::max)(myMaxLength, effectiveRestLength);

	auto ApplyPositionConstraint = [&](const Vector3f& direction, float errorLength, const Vector3f& fallbackAxis)
		{
			if (errorLength <= kConstraintSlop)
			{
				return;
			}

			const Transform chassisTransformNow = myOwner->GetParent()->GetTransform();
			const Transform wheelTransformNow = myOwner->GetTransform();
			const Vector3f axisWorldNow = NormalizeOr(TransformVectorNoScale(chassisTransformNow, mySuspensionAxisLocal), fallbackAxis);
			const Vector3f chassisAnchorWorldNow = TransformPointNoScale(chassisTransformNow, myChassisAnchorLocal);
			const Vector3f wheelAnchorWorldNow = TransformPointNoScale(wheelTransformNow, myWheelAnchorLocal);
			const Vector3f correctionDir = NormalizeOr(direction, axisWorldNow * -1.0f);
			const float effectiveMass = ComputeConstraintEffectiveMass(myWheelBody, myChassisBody, wheelAnchorWorldNow, chassisAnchorWorldNow, correctionDir);
			if (effectiveMass > 0.0001f)
			{
				float positionImpulseMagnitude = ((errorLength - kConstraintSlop) * kConstraintBaumgarte) / effectiveMass;
				if (positionImpulseMagnitude > kMaxLinearCorrection)
				{
					positionImpulseMagnitude = kMaxLinearCorrection;
				}
				if (positionImpulseMagnitude > 0.0f)
				{
					const Vector3f positionImpulse = correctionDir * positionImpulseMagnitude;
					myWheelBody->ApplyPositionImpulseAtPoint(positionImpulse, wheelAnchorWorldNow);
					myChassisBody->ApplyPositionImpulseAtPoint(positionImpulse * -1.0f, chassisAnchorWorldNow);
				}
			}
		};

	{
		const Transform chassisTransform = myOwner->GetParent()->GetTransform();
		const Transform wheelTransform = myOwner->GetTransform();
		const Vector3f axisWorld = NormalizeOr(TransformVectorNoScale(chassisTransform, mySuspensionAxisLocal), { 0.0f, -1.0f, 0.0f });
		const Vector3f chassisAnchorWorld = TransformPointNoScale(chassisTransform, myChassisAnchorLocal);
		const Vector3f wheelAnchorWorld = TransformPointNoScale(wheelTransform, myWheelAnchorLocal);
		const Vector3f delta = wheelAnchorWorld - chassisAnchorWorld;
		const Vector3f lateralOffset = delta - axisWorld * delta.Dot(axisWorld);
		const float lateralConstraintError = lateralOffset.Length();
		if (lateralConstraintError > kConstraintSlop)
		{
			ApplyPositionConstraint(lateralOffset * -1.0f, lateralConstraintError, axisWorld);
		}
	}

	{
		const Transform chassisTransform = myOwner->GetParent()->GetTransform();
		const Transform wheelTransform = myOwner->GetTransform();
		const Vector3f axisWorld = NormalizeOr(TransformVectorNoScale(chassisTransform, mySuspensionAxisLocal), { 0.0f, -1.0f, 0.0f });
		const Vector3f chassisAnchorWorld = TransformPointNoScale(chassisTransform, myChassisAnchorLocal);
		const Vector3f wheelAnchorWorld = TransformPointNoScale(wheelTransform, myWheelAnchorLocal);
		const Vector3f delta = wheelAnchorWorld - chassisAnchorWorld;
		const float axialLength = delta.Dot(axisWorld);
		if (axialLength < 0.0f)
		{
			ApplyPositionConstraint(axisWorld, -axialLength, axisWorld);
		}
		else if (axialLength > effectiveMaxLength)
		{
			ApplyPositionConstraint(axisWorld * -1.0f, axialLength - effectiveMaxLength, axisWorld);
		}

		const float axialConstraintError =
			axialLength < 0.0f ? -axialLength :
			(axialLength > effectiveMaxLength ? axialLength - effectiveMaxLength : 0.0f);
		const Vector3f lateralOffset = delta - axisWorld * axialLength;
		const float lateralConstraintError = lateralOffset.Length();
		myLastAxialConstraintError = axialConstraintError;
		myLastLateralConstraintError = lateralConstraintError;
		// Support state was established by ApplySpringForces. Do not call
		// RefreshGroundContactState here: it would overwrite myPreviousSupportedAxialLength
		// with the post-correction wheel position, corrupting the velocity estimate
		// computed as (L_spring_current - L_spring_prev) / dt next frame.
	}
}