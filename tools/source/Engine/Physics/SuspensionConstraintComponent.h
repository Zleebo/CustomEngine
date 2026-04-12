#pragma once

#include <Components/Component.h>
#include <Math/Vector.h>

class RigidbodyComponent;
class ModelInstance;

class SuspensionConstraintComponent : public Component
{
public:
	SuspensionConstraintComponent();
	~SuspensionConstraintComponent() override;

	void OnStart() override;
	void Update(float aDeltaTime) override;
	void ApplySpringForces(float aDeltaTime);
	void SolveVelocityConstraints(float aDeltaTime);
	void SolvePositionConstraints(float aDeltaTime);

	float GetLastAxialLength() const { return myLastAxialLength; }
	float GetLastClampedLength() const { return myLastClampedLength; }
	float GetLastEffectiveRestLength() const { return myLastEffectiveRestLength; }
	float GetLastEffectiveMaxLength() const { return myLastEffectiveMaxLength; }
	float GetLastSpringForceMagnitude() const { return myLastSpringForceMagnitude; }
	float GetLastLateralForceMagnitude() const { return myLastLateralForceMagnitude; }
	float GetLastLimitForceMagnitude() const { return myLastLimitForceMagnitude; }
	float GetLastUnclampedForceMagnitude() const { return myLastUnclampedForceMagnitude; }
	float GetLastTotalForceMagnitude() const { return myLastTotalForceMagnitude; }
	float GetLastMaxConstraintForce() const { return myLastMaxConstraintForce; }
	float GetLastSpringScale() const { return myLastSpringScale; }
	float GetLastLateralScale() const { return myLastLateralScale; }
	bool WasWheelSupportedLastFrame() const { return myLastWheelSupported; }
	bool HasValidBodies() const { return myWheelBody != nullptr && myChassisBody != nullptr && myWheelBody != myChassisBody; }
	bool HasGroundContact() const { return myHasGroundContact; }
	bool HasSupportContact() const { return myHasSupportContact; }
	bool HadSupportContactInVelocitySolve() const { return myVelocitySolveSupported; }
	const Vector3f& GetGroundContactPoint() const { return myGroundContactPoint; }
	const Vector3f& GetGroundContactNormal() const { return myGroundContactNormal; }
	float GetWheelLoad() const { return myWheelLoad; }
	float GetLastAxialConstraintError() const { return myLastAxialConstraintError; }
	float GetLastLateralConstraintError() const { return myLastLateralConstraintError; }
	float GetSprungMass() const { return mySprungMass; }
	float GetDerivedSpringStrength() const { return myDerivedSpringStrength; }
	float GetDerivedSpringDamping() const { return myDerivedSpringDamping; }
	void SetSprungMass(float aSprungMass);

	float myRestLength = 0.14f;
	float myMaxLength = 0.35f;
	float mySuspensionFrequencyHz = 5.0f;
	float mySuspensionDampingRatio = 0.9f;
	bool myAutoConfigure = true;
	Vector3f myChassisAnchorLocal{ 0.0f, 0.0f, 0.0f };
	Vector3f myWheelAnchorLocal{ 0.0f, 0.0f, 0.0f };
	Vector3f mySuspensionAxisLocal{ 0.0f, -1.0f, 0.0f };

private:
	RigidbodyComponent* myWheelBody = nullptr;
	RigidbodyComponent* myChassisBody = nullptr;
	ModelInstance* myBoundParent = nullptr;
	bool myConfigured = false;

	float myLastAxialLength = 0.0f;
	float myLastClampedLength = 0.0f;
	float myLastEffectiveRestLength = 0.0f;
	float myLastEffectiveMaxLength = 0.0f;
	float myLastSpringForceMagnitude = 0.0f;
	float myLastLateralForceMagnitude = 0.0f;
	float myLastLimitForceMagnitude = 0.0f;
	float myLastUnclampedForceMagnitude = 0.0f;
	float myLastTotalForceMagnitude = 0.0f;
	float myLastMaxConstraintForce = 0.0f;
	float myLastSpringScale = 0.0f;
	float myLastLateralScale = 0.0f;
	bool myLastWheelSupported = false;
	float myLastAxialConstraintError = 0.0f;
	float myLastLateralConstraintError = 0.0f;

	float mySprungMass = 1.0f;
	float myDerivedSpringStrength = 0.0f;
	float myDerivedSpringDamping = 0.0f;

	bool myHasGroundContact = false;
	bool myHasSupportContact = false;
	bool myVelocitySolveSupported = false;
	bool myHasPredictedGroundContact = false;

	Vector3f myGroundContactPoint{ 0.0f, 0.0f, 0.0f };
	Vector3f myGroundContactNormal{ 0.0f, 1.0f, 0.0f };
	float myWheelLoad = 0.0f;
	Vector3f myLastWheelCenter{ 0.0f, 0.0f, 0.0f };

	float mySupportedAxialLength = 0.0f;
	float myPreviousSupportedAxialLength = 0.0f;
	float mySupportedAxialVelocity = 0.0f;
	float myLastSupportPenetration = 0.0f;

	void TryBindBodies();
	void AutoConfigureFromCurrentPose();
	void ResetStepMetrics();
	void UpdateDerivedSuspensionParameters();
	void RefreshGroundContactState();
	void UpdateSupportState(float anAxialLength, float anEffectiveMaxLength, float aLateralConstraintError);
};