#pragma once

#include <Engine.h>

#include <deque>
#include <fstream>
#include <string>

class RigidbodyComponent;
class ModelInstance;

class CarAutotest
{
public:
	enum class Scenario
	{
		Drive,
		PlayStart,
		Drop,
		Forward,
		SteerLeft,
		SteerRight
	};

	struct WheelTelemetry
	{
		Vector3f worldPos = { 0.0f, 0.0f, 0.0f };
		Vector3f normal = { 0.0f, 1.0f, 0.0f };
		Vector3f supportPoint = { 0.0f, 0.0f, 0.0f };
		float gap = 0.0f;
		float compression = 0.0f;
		float suspensionAxialLength = 0.0f;
		float suspensionClampedLength = 0.0f;
		float suspensionRestLength = 0.0f;
		float suspensionMaxLength = 0.0f;
		float suspensionSpringForce = 0.0f;
		float suspensionLateralForce = 0.0f;
		float suspensionLimitForce = 0.0f;
		float suspensionUnclampedForce = 0.0f;
		float suspensionTotalForce = 0.0f;
		float suspensionMaxConstraintForce = 0.0f;
		float suspensionSpringScale = 0.0f;
		float suspensionLateralScale = 0.0f;
		bool suspensionWheelSupported = false;
		bool suspensionForceSolveSupported = false;
		float suspensionAxialError = 0.0f;
		float suspensionLateralError = 0.0f;
		int wheelContactCount = 0;
		int wheelTerrainContactCount = 0;
		Vector3f wheelAverageContactPoint = { 0.0f, 0.0f, 0.0f };
		Vector3f wheelAverageContactNormal = { 0.0f, 1.0f, 0.0f };
		float wheelTerrainLoadEstimate = 0.0f;
		float wheelLoad = 0.0f;
		float wheelAngularSpeed = 0.0f;
		float slipRatio = 0.0f;
		float slipAngle = 0.0f;
		bool grounded = false;
	};

	void Initialize(ModelInstance* aOwner, RigidbodyComponent* aRigidbody);
	bool IsEnabled() const { return myEnabled; }
	bool IsFinished() const { return myFinished; }

	void ApplyScript(float aDeltaTime, float& inOutThrottleInput, float& inOutSteerInput, bool& outBraking);
	void NoteWheelBindingState(ModelInstance* aOwner, RigidbodyComponent* aRigidbody, bool aAllFourWheelsConnected);
	void NoteFrame(
		ModelInstance* aOwner,
		RigidbodyComponent* aRigidbody,
		const WheelTelemetry* someWheelTelemetry,
		int aWheelCount,
		float aWheelRadius,
		float aContactRatio,
		float aRearContactRatio,
		int aGroundedCount,
		float aDriveForceMagnitude,
		float aLateralForceMagnitude,
		float aBrakeForceMagnitude,
		bool aIsGrounded,
		float aAirTime);
	void FailMissingRuntime();

private:
	void Finish(const char* aStatus, const char* aReason);
	void ResetMetrics();
	void WriteStartMarker(ModelInstance* aOwner, RigidbodyComponent* aRigidbody) const;
	void PrepareScenarioStart(ModelInstance* aOwner, RigidbodyComponent* aRigidbody);
	Scenario ParseScenarioFromEnvironment() const;
	void BeginDebugCapture();
	void AppendDebugFrame(
		ModelInstance* aOwner,
		RigidbodyComponent* aRigidbody,
		const WheelTelemetry* someWheelTelemetry,
		int aWheelCount,
		float aWheelRadius,
		float aContactRatio,
		float aRearContactRatio,
		int aGroundedCount,
		float aDriveForceMagnitude,
		float aLateralForceMagnitude,
		float aBrakeForceMagnitude,
		bool aIsGrounded,
		float aAirTime,
		float aBodyUnderDepth,
		float aPitchAbs,
		float aRollAbs,
		float aPitchRateDeg,
		float aRollRateDeg,
		float aYawRateDeg);
	void WriteInstabilitySnapshot(
		const char* aReason,
		ModelInstance* aOwner,
		RigidbodyComponent* aRigidbody,
		const WheelTelemetry* someWheelTelemetry,
		int aWheelCount,
		float aWheelRadius,
		float aContactRatio,
		float aRearContactRatio,
		int aGroundedCount,
		float aDriveForceMagnitude,
		float aLateralForceMagnitude,
		float aBrakeForceMagnitude,
		bool aIsGrounded,
		float aAirTime,
		float aBodyUnderDepth,
		float aPitchAbs,
		float aRollAbs,
		float aPitchRateDeg,
		float aRollRateDeg,
		float aYawRateDeg);
	void WriteTouchdownSnapshot(
		ModelInstance* aOwner,
		RigidbodyComponent* aRigidbody,
		const WheelTelemetry* someWheelTelemetry,
		int aWheelCount,
		float aWheelRadius,
		float aContactRatio,
		float aRearContactRatio,
		int aGroundedCount,
		float aDriveForceMagnitude,
		float aLateralForceMagnitude,
		float aBrakeForceMagnitude,
		bool aIsGrounded,
		float aAirTime,
		float aBodyUnderDepth,
		float aPitchAbs,
		float aRollAbs,
		float aPitchRateDeg,
		float aRollRateDeg,
		float aYawRateDeg);

	bool myEnabled = false;
	bool myFinished = false;
	bool myFirstUpdateDone = false;
	bool myScenarioPrepared = false;
	bool myDebugCaptureBegun = false;
	bool myInstabilitySnapshotWritten = false;
	bool myAllFourWheelsConnected = false;
	bool myAllFourWheelsGrounded = false;
	bool myUnstable = false;
	bool myGroundInstabilityActive = false;
	bool myPreserveRuntimeScene = false;
	float myTime = 0.0f;
	float myStartTerrain = 0.0f;
	float myMaxTerrainGain = 0.0f;
	float myStartYaw = 0.0f;
	float myMaxYawDelta = 0.0f;
	float myMaxSpeed = 0.0f;
	float myMaxDistance = 0.0f;
	float myMaxContactRatio = 0.0f;
	float myMaxRearContactRatio = 0.0f;
	float myMaxPitchAbs = 0.0f;
	float myMaxRollAbs = 0.0f;
	float myFirstInstabilityTime = -1.0f;
	float myMaxWheelUnderTerrainDepth = 0.0f;
	float myMaxWheelCenterUnderTerrainDepth = 0.0f;
	float myMaxWheelBodyDistance = 0.0f;
	float myMaxSupportPointPlaneError = 0.0f;
	float myMaxBodyUnderTerrainDepth = 0.0f;
	float myMaxDriveForceMagnitude = 0.0f;
	float myMaxLateralForceMagnitude = 0.0f;
	float myMaxBrakeForceMagnitude = 0.0f;
	float myMaxSuspensionTotalForce = 0.0f;
	float myMaxAirborneSuspensionForce = 0.0f;
	float myMaxGroundedPitchRateDeg = 0.0f;
	float myMaxGroundedRollRateDeg = 0.0f;
	float myMaxGroundedYawRateDeg = 0.0f;
	float myMaxWheelLoad = 0.0f;
	float myMaxAbsSlipRatio = 0.0f;
	float myMaxAbsSlipAngleDeg = 0.0f;
	float myMaxSuspensionAxialError = 0.0f;
	float myMaxSuspensionLateralError = 0.0f;
	float myFinalForwardDistance = 0.0f;
	float myFinalRightDistance = 0.0f;
	float myMaxAbsRightDistance = 0.0f;
	float myFinalYawDelta = 0.0f;
	float myLastContactRatio = 0.0f;
	float myLastRearContactRatio = 0.0f;
	float myFirstContactTime = -1.0f;
	float myFirstTouchdownTime = -1.0f;
	int myBodyTerrainHits = 0;
	int myGroundFlipEvents = 0;
	int myLandingSpikeEvents = 0;
	int myFrameIndex = 0;
	int myPreviousGroundedCount = 0;
	int myFirstContactFrame = -1;
	int myFirstTouchdownFrame = -1;
	int myFirstContactGroundedCount = 0;
	int myFirstTouchdownGroundedCount = 0;
	int myFirstTouchdownWheelIndex = -1;
	int myTouchdownWindowFramesRemaining = 0;
	bool myTouchdownSnapshotWritten = false;
	Vector3f myStartPos = { 0.0f, 0.0f, 0.0f };
	std::string myDebugFailureReason;
	std::string myScenarioName = "drive";
	std::string myDebugCsvHeader;
	std::string myLatestDebugFrameLine;
	std::string myInstabilitySnapshotText;
	std::string myTouchdownSummaryText;
	std::string myTouchdownCsvText;
	Scenario myScenario = Scenario::Drive;
	std::ofstream myDebugCsv;
	std::deque<std::string> myRecentDebugFrames;
};
