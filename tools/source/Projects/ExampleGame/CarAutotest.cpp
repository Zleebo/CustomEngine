#include "CarAutotest.h"

#include <Engine.h>

#include <Windows.h>
#include <DbgHelp.h>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "Dbghelp.lib")

namespace
{
	constexpr float kPi = 3.14159265358979323846f;
	constexpr float kDegToRad = kPi / 180.0f;
	constexpr float kRadToDeg = 180.0f / kPi;

	float WrapAngle180(float aDegrees)
	{
		while (aDegrees > 180.0f) aDegrees -= 360.0f;
		while (aDegrees < -180.0f) aDegrees += 360.0f;
		return aDegrees;
	}

	Vector3f ForwardFromYaw(float aYawDegrees)
	{
		const float yawRad = aYawDegrees * kDegToRad;
		return { std::sinf(yawRad), 0.0f, std::cosf(yawRad) };
	}

	Vector3f RightFromYaw(float aYawDegrees)
	{
		const float yawRad = aYawDegrees * kDegToRad;
		return { std::cosf(yawRad), 0.0f, -std::sinf(yawRad) };
	}

	float DistanceXZ(const Vector3f& aA, const Vector3f& aB)
	{
		const float dx = aA.X - aB.X;
		const float dz = aA.Z - aB.Z;
		return std::sqrt(dx * dx + dz * dz);
	}

	float ClampMin(float aValue, float aMin)
	{
		return aValue < aMin ? aMin : aValue;
	}

	Transform MakeNoScaleTransform(const Transform& aTransform)
	{
		Transform result = aTransform;
		result.SetScale({ 1.0f, 1.0f, 1.0f });
		return result;
	}

	Vector3f TransformPointNoScale(const Transform& aTransform, const Vector3f& aLocalPoint)
	{
		Transform noScale = MakeNoScaleTransform(aTransform);
		Vector3f point = aLocalPoint;
		return noScale.TransformPosition(point);
	}

	Vector3f InverseTransformPointNoScale(const Transform& aTransform, const Vector3f& aWorldPoint)
	{
		Transform noScale = MakeNoScaleTransform(aTransform);
		Vector3f point = aWorldPoint;
		return noScale.InverseTransformPosition(point);
	}

	bool IsFiniteFloat(float aValue)
	{
		return std::isfinite(aValue) != 0;
	}

	bool IsFiniteVec(const Vector3f& aValue)
	{
		return IsFiniteFloat(aValue.X) && IsFiniteFloat(aValue.Y) && IsFiniteFloat(aValue.Z);
	}

	bool IsFiniteRot(const Rotator& aValue)
	{
		return IsFiniteFloat(aValue.Pitch) && IsFiniteFloat(aValue.Roll) && IsFiniteFloat(aValue.Yaw);
	}

	float MaxAbsComponent(const Vector3f& aValue)
	{
		return (std::max)((std::max)(std::fabs(aValue.X), std::fabs(aValue.Y)), std::fabs(aValue.Z));
	}

	void EnsureSymbolEngineInitialized()
	{
		static bool initialized = false;
		if (initialized)
		{
			return;
		}

		HANDLE process = ::GetCurrentProcess();
		::SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
		::SymInitialize(process, nullptr, TRUE);
		initialized = true;
	}

	void WriteCallstack(std::ostream& aOut, unsigned short aSkipFrames = 0)
	{
		EnsureSymbolEngineInitialized();
		void* frames[64] = {};
		const USHORT frameCount = ::CaptureStackBackTrace(aSkipFrames + 1, 64, frames, nullptr);
		HANDLE process = ::GetCurrentProcess();
		char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
		SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = MAX_SYM_NAME;

		IMAGEHLP_LINE64 lineInfo = {};
		lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

		aOut << "callstack_begin\n";
		for (USHORT i = 0; i < frameCount; ++i)
		{
			const DWORD64 address = reinterpret_cast<DWORD64>(frames[i]);
			DWORD64 displacement = 0;
			DWORD lineDisplacement = 0;
			aOut << "frame" << i << "=";
			if (::SymFromAddr(process, address, &displacement, symbol))
			{
				aOut << symbol->Name;
			}
			else
			{
				aOut << "0x" << std::hex << address << std::dec;
			}

			if (::SymGetLineFromAddr64(process, address, &lineDisplacement, &lineInfo))
			{
				aOut << " @" << lineInfo.FileName << ":" << lineInfo.LineNumber;
			}
			aOut << "\n";
		}
		aOut << "callstack_end\n";
	}

	void ResetRigidbodyStateRecursive(ModelInstance* aRoot)
	{
		if (!aRoot)
		{
			return;
		}

		if (RigidbodyComponent* body = aRoot->GetComponent<RigidbodyComponent>())
		{
			body->myVelocity = { 0.0f, 0.0f, 0.0f };
			body->myAngularVelocity = { 0.0f, 0.0f, 0.0f };
			body->BeginPhysicsStep();
		}

		for (ModelInstance* child : aRoot->GetChildren())
		{
			ResetRigidbodyStateRecursive(child);
		}
	}

	void ApplyRootTransformToChildren(ModelInstance* aRoot, const Transform& aOldRootTransform, const Transform& aNewRootTransform)
	{
		if (!aRoot)
		{
			return;
		}

		const Quaternionf oldRootQ(aOldRootTransform.GetRotation() * Math::DegToRad);
		const Quaternionf newRootQ(aNewRootTransform.GetRotation() * Math::DegToRad);
		const Quaternionf rootDelta = (newRootQ * oldRootQ.GetConjugate()).GetNormalized();

		for (ModelInstance* child : aRoot->GetChildren())
		{
			if (!child)
			{
				continue;
			}

			const Vector3f childWorld = child->GetLocation();
			const Vector3f childLocal = InverseTransformPointNoScale(aOldRootTransform, childWorld);
			child->SetLocation(TransformPointNoScale(aNewRootTransform, childLocal));

			const Quaternionf childQ(child->GetTransform().GetRotation() * Math::DegToRad);
			const Quaternionf newChildQ = (rootDelta * childQ).GetNormalized();
			child->SetRotation(newChildQ.GetEulerAnglesDegrees());

			ApplyRootTransformToChildren(child, aOldRootTransform, aNewRootTransform);
		}
	}

	const char* ScenarioName(CarAutotest::Scenario aScenario)
	{
		switch (aScenario)
		{
		case CarAutotest::Scenario::PlayStart: return "play_start";
		case CarAutotest::Scenario::Drop: return "drop";
		case CarAutotest::Scenario::Forward: return "forward";
		case CarAutotest::Scenario::SteerLeft: return "steer_left";
		case CarAutotest::Scenario::SteerRight: return "steer_right";
		case CarAutotest::Scenario::Drive:
		default: return "drive";
		}
	}
}

CarAutotest::Scenario CarAutotest::ParseScenarioFromEnvironment() const
{
	char* modeEnv = nullptr;
	size_t modeEnvLen = 0;
	errno_t modeErr = _dupenv_s(&modeEnv, &modeEnvLen, "TE_AUTOTEST_MODE");
	if (modeErr != 0 || !modeEnv || modeEnv[0] == '\0')
	{
		if (modeEnv) free(modeEnv);
		return Scenario::Drive;
	}

	std::string mode = modeEnv;
	free(modeEnv);
	for (char& c : mode) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

	if (mode == "play_start") return Scenario::PlayStart;
	if (mode == "drop") return Scenario::Drop;
	if (mode == "forward") return Scenario::Forward;
	if (mode == "steer_left") return Scenario::SteerLeft;
	if (mode == "steer_right") return Scenario::SteerRight;
	return Scenario::Drive;
}

void CarAutotest::ResetMetrics()
{
	myFinished = false;
	myFirstUpdateDone = false;
	myScenarioPrepared = false;
	myDebugCaptureBegun = false;
	myInstabilitySnapshotWritten = false;
	myAllFourWheelsConnected = false;
	myAllFourWheelsGrounded = false;
	myUnstable = false;
	myGroundInstabilityActive = false;
	myTime = 0.0f;
	myMaxTerrainGain = 0.0f;
	myMaxYawDelta = 0.0f;
	myMaxSpeed = 0.0f;
	myMaxDistance = 0.0f;
	myMaxContactRatio = 0.0f;
	myMaxRearContactRatio = 0.0f;
	myMaxPitchAbs = 0.0f;
	myMaxRollAbs = 0.0f;
	myFirstInstabilityTime = -1.0f;
	myMaxWheelUnderTerrainDepth = 0.0f;
	myMaxWheelCenterUnderTerrainDepth = 0.0f;
	myMaxWheelBodyDistance = 0.0f;
	myMaxSupportPointPlaneError = 0.0f;
	myMaxBodyUnderTerrainDepth = 0.0f;
	myMaxDriveForceMagnitude = 0.0f;
	myMaxLateralForceMagnitude = 0.0f;
	myMaxBrakeForceMagnitude = 0.0f;
	myMaxSuspensionTotalForce = 0.0f;
	myMaxAirborneSuspensionForce = 0.0f;
	myMaxGroundedPitchRateDeg = 0.0f;
	myMaxGroundedRollRateDeg = 0.0f;
	myMaxGroundedYawRateDeg = 0.0f;
	myMaxWheelLoad = 0.0f;
	myMaxAbsSlipRatio = 0.0f;
	myMaxAbsSlipAngleDeg = 0.0f;
	myMaxSuspensionAxialError = 0.0f;
	myMaxSuspensionLateralError = 0.0f;
	myFinalForwardDistance = 0.0f;
	myFinalRightDistance = 0.0f;
	myMaxAbsRightDistance = 0.0f;
	myFinalYawDelta = 0.0f;
	myLastContactRatio = 0.0f;
	myLastRearContactRatio = 0.0f;
	myBodyTerrainHits = 0;
	myGroundFlipEvents = 0;
	myLandingSpikeEvents = 0;
	myFrameIndex = 0;
	myPreviousGroundedCount = 0;
	myFirstContactTime = -1.0f;
	myFirstTouchdownTime = -1.0f;
	myFirstContactFrame = -1;
	myFirstTouchdownFrame = -1;
	myFirstContactGroundedCount = 0;
	myFirstTouchdownGroundedCount = 0;
	myFirstTouchdownWheelIndex = -1;
	myTouchdownWindowFramesRemaining = 0;
	myTouchdownSnapshotWritten = false;
	myDebugFailureReason.clear();
	myDebugCsvHeader.clear();
	myLatestDebugFrameLine.clear();
	myInstabilitySnapshotText.clear();
	myTouchdownSummaryText.clear();
	myTouchdownCsvText.clear();
	myRecentDebugFrames.clear();
	if (myDebugCsv.is_open())
	{
		myDebugCsv.close();
	}
}

void CarAutotest::BeginDebugCapture()
{
	if (myDebugCaptureBegun)
	{
		return;
	}

	myDebugCaptureBegun = true;
	{
		std::ofstream clearSnapshot("CarDriveAutotest.debug.txt", std::ios::out | std::ios::trunc);
		if (clearSnapshot.is_open())
		{
			clearSnapshot << "reason=none\n";
			clearSnapshot << "scenario=" << myScenarioName << "\n";
			clearSnapshot << "note=no_instability_snapshot_written_yet\n";
		}
	}
	{
		std::ofstream clearTouchdown("CarDriveAutotest.touchdown.txt", std::ios::out | std::ios::trunc);
		if (clearTouchdown.is_open())
		{
			clearTouchdown << "scenario=" << myScenarioName << "\n";
			clearTouchdown << "note=no_touchdown_recorded_yet\n";
		}
	}
	{
		std::ofstream clearTouchdownCsv("CarDriveAutotest.touchdown.csv", std::ios::out | std::ios::trunc);
	}
	myDebugCsv.open("CarDriveAutotest.debug.csv", std::ios::out | std::ios::trunc);
	if (!myDebugCsv.is_open())
	{
		return;
	}

	std::ostringstream header;
	header << std::fixed << std::setprecision(4);
	header
		<< "frame,time,scenario,body_x,body_y,body_z,pitch,roll,yaw,vel_x,vel_y,vel_z,ang_x,ang_y,ang_z,"
		<< "body_mass,body_contact_count,body_terrain_contact_count,body_acc_force_x,body_acc_force_y,body_acc_force_z,body_acc_torque_x,body_acc_torque_y,body_acc_torque_z,"
		<< "body_inertia_x,body_inertia_y,body_inertia_z,"
		<< "grounded,air_time,grounded_count,contact_ratio,rear_contact_ratio,drive_force,lateral_force,brake_force,"
		<< "body_under_depth,pitch_rate_deg,roll_rate_deg,yaw_rate_deg";

	for (int i = 0; i < 4; ++i)
	{
		header
			<< ",wheel" << i << "_grounded"
			<< ",wheel" << i << "_world_x"
			<< ",wheel" << i << "_world_y"
			<< ",wheel" << i << "_world_z"
			<< ",wheel" << i << "_support_x"
			<< ",wheel" << i << "_support_y"
			<< ",wheel" << i << "_support_z"
			<< ",wheel" << i << "_normal_x"
			<< ",wheel" << i << "_normal_y"
			<< ",wheel" << i << "_normal_z"
			<< ",wheel" << i << "_gap"
			<< ",wheel" << i << "_compression"
			<< ",wheel" << i << "_suspension_axial"
			<< ",wheel" << i << "_suspension_clamped_length"
			<< ",wheel" << i << "_suspension_rest_length"
			<< ",wheel" << i << "_suspension_max_length"
			<< ",wheel" << i << "_suspension_spring_force"
			<< ",wheel" << i << "_suspension_lateral_force"
			<< ",wheel" << i << "_suspension_limit_force"
			<< ",wheel" << i << "_suspension_unclamped_force"
			<< ",wheel" << i << "_suspension_total_force"
			<< ",wheel" << i << "_suspension_max_constraint_force"
			<< ",wheel" << i << "_suspension_spring_scale"
			<< ",wheel" << i << "_suspension_lateral_scale"
			<< ",wheel" << i << "_suspension_supported"
			<< ",wheel" << i << "_suspension_force_solve_supported"
			<< ",wheel" << i << "_suspension_axial_error"
			<< ",wheel" << i << "_suspension_lateral_error"
			<< ",wheel" << i << "_contact_count"
			<< ",wheel" << i << "_terrain_contact_count"
			<< ",wheel" << i << "_avg_contact_point_x"
			<< ",wheel" << i << "_avg_contact_point_y"
			<< ",wheel" << i << "_avg_contact_point_z"
			<< ",wheel" << i << "_avg_contact_normal_x"
			<< ",wheel" << i << "_avg_contact_normal_y"
			<< ",wheel" << i << "_avg_contact_normal_z"
			<< ",wheel" << i << "_terrain_load_estimate"
			<< ",wheel" << i << "_wheel_load"
			<< ",wheel" << i << "_wheel_angular_speed"
			<< ",wheel" << i << "_slip_ratio"
			<< ",wheel" << i << "_slip_angle"
			<< ",wheel" << i << "_center_under"
			<< ",wheel" << i << "_wheel_under"
			<< ",wheel" << i << "_support_plane_error";
	}
	header << "\n";
	myDebugCsvHeader = header.str();
	myDebugCsv << myDebugCsvHeader;
}

void CarAutotest::AppendDebugFrame(
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
	float aYawRateDeg)
{
	BeginDebugCapture();
	if (!myDebugCsv.is_open() || !aOwner || !aRigidbody)
	{
		return;
	}

	const Rotator rot = aOwner->GetTransform().GetRotation();
	const Vector3f pos = aOwner->GetLocation();
	const Vector3f accumulatedForce = aRigidbody->GetAccumulatedForceDebug();
	const Vector3f accumulatedTorque = aRigidbody->GetAccumulatedTorqueDebug();
	const Vector3f inertiaDiagonal = aRigidbody->GetBodyInertiaDiagonalDebug();
	std::ostringstream row;
	row << std::fixed << std::setprecision(4);
	row
		<< myFrameIndex
		<< "," << myTime
		<< "," << myScenarioName
		<< "," << pos.X
		<< "," << pos.Y
		<< "," << pos.Z
		<< "," << rot.Pitch
		<< "," << rot.Roll
		<< "," << rot.Yaw
		<< "," << aRigidbody->myVelocity.X
		<< "," << aRigidbody->myVelocity.Y
		<< "," << aRigidbody->myVelocity.Z
		<< "," << aRigidbody->myAngularVelocity.X
		<< "," << aRigidbody->myAngularVelocity.Y
		<< "," << aRigidbody->myAngularVelocity.Z
		<< "," << aRigidbody->myMass
		<< "," << aRigidbody->GetContactCount()
		<< "," << aRigidbody->GetTerrainContactCount()
		<< "," << accumulatedForce.X
		<< "," << accumulatedForce.Y
		<< "," << accumulatedForce.Z
		<< "," << accumulatedTorque.X
		<< "," << accumulatedTorque.Y
		<< "," << accumulatedTorque.Z
		<< "," << inertiaDiagonal.X
		<< "," << inertiaDiagonal.Y
		<< "," << inertiaDiagonal.Z
		<< "," << (aIsGrounded ? 1 : 0)
		<< "," << aAirTime
		<< "," << aGroundedCount
		<< "," << aContactRatio
		<< "," << aRearContactRatio
		<< "," << aDriveForceMagnitude
		<< "," << aLateralForceMagnitude
		<< "," << aBrakeForceMagnitude
		<< "," << aBodyUnderDepth
		<< "," << aPitchRateDeg
		<< "," << aRollRateDeg
		<< "," << aYawRateDeg;

	for (int i = 0; i < 4; ++i)
	{
		if (someWheelTelemetry && i < aWheelCount)
		{
			const WheelTelemetry& wheel = someWheelTelemetry[i];
			const bool hasTerrainManifold = wheel.wheelTerrainContactCount > 0;
			const PhysicsWorld::TerrainSample terrain = PhysicsWorld::Get().GetTerrainSample(wheel.worldPos.X, wheel.worldPos.Z);
			const Vector3f planePoint = hasTerrainManifold ? wheel.wheelAverageContactPoint : terrain.point;
			const Vector3f planeNormal = hasTerrainManifold ? wheel.wheelAverageContactNormal : terrain.normal;
			const float centerSignedDistance = (wheel.worldPos - planePoint).Dot(planeNormal);
			const float sampledGap = centerSignedDistance - aWheelRadius;
			const float centerUnderDepth = ClampMin(-centerSignedDistance, 0.0f);
			const float wheelUnderDepth = ClampMin(-sampledGap, 0.0f);
			const float supportPlaneDistance = std::fabs((wheel.supportPoint - planePoint).Dot(planeNormal) - aWheelRadius);
			row
				<< "," << (wheel.grounded ? 1 : 0)
				<< "," << wheel.worldPos.X
				<< "," << wheel.worldPos.Y
				<< "," << wheel.worldPos.Z
				<< "," << wheel.supportPoint.X
				<< "," << wheel.supportPoint.Y
				<< "," << wheel.supportPoint.Z
				<< "," << wheel.normal.X
				<< "," << wheel.normal.Y
				<< "," << wheel.normal.Z
				<< "," << wheel.gap
				<< "," << wheel.compression
				<< "," << wheel.suspensionAxialLength
				<< "," << wheel.suspensionClampedLength
				<< "," << wheel.suspensionRestLength
				<< "," << wheel.suspensionMaxLength
				<< "," << wheel.suspensionSpringForce
				<< "," << wheel.suspensionLateralForce
				<< "," << wheel.suspensionLimitForce
				<< "," << wheel.suspensionUnclampedForce
				<< "," << wheel.suspensionTotalForce
				<< "," << wheel.suspensionMaxConstraintForce
				<< "," << wheel.suspensionSpringScale
				<< "," << wheel.suspensionLateralScale
				<< "," << (wheel.suspensionWheelSupported ? 1 : 0)
				<< "," << (wheel.suspensionForceSolveSupported ? 1 : 0)
				<< "," << wheel.suspensionAxialError
				<< "," << wheel.suspensionLateralError
				<< "," << wheel.wheelContactCount
				<< "," << wheel.wheelTerrainContactCount
				<< "," << wheel.wheelAverageContactPoint.X
				<< "," << wheel.wheelAverageContactPoint.Y
				<< "," << wheel.wheelAverageContactPoint.Z
				<< "," << wheel.wheelAverageContactNormal.X
				<< "," << wheel.wheelAverageContactNormal.Y
				<< "," << wheel.wheelAverageContactNormal.Z
				<< "," << wheel.wheelTerrainLoadEstimate
				<< "," << wheel.wheelLoad
				<< "," << wheel.wheelAngularSpeed
				<< "," << wheel.slipRatio
				<< "," << wheel.slipAngle
				<< "," << centerUnderDepth
				<< "," << wheelUnderDepth
				<< "," << supportPlaneDistance;
		}
		else
		{
			row << ",0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0";
		}
	}
	row << "\n";
	myLatestDebugFrameLine = row.str();
	myRecentDebugFrames.push_back(myLatestDebugFrameLine);
	while (myRecentDebugFrames.size() > 12)
	{
		myRecentDebugFrames.pop_front();
	}
	myDebugCsv << myLatestDebugFrameLine;
	myDebugCsv.flush();

	if (myTouchdownWindowFramesRemaining > 0)
	{
		myTouchdownCsvText += myLatestDebugFrameLine;
		--myTouchdownWindowFramesRemaining;
	}
}

void CarAutotest::WriteInstabilitySnapshot(
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
	float aYawRateDeg)
{
	if (myInstabilitySnapshotWritten || !aOwner || !aRigidbody)
	{
		return;
	}

	myInstabilitySnapshotWritten = true;
	std::ostringstream debug;

	const Rotator rot = aOwner->GetTransform().GetRotation();
	const Vector3f pos = aOwner->GetLocation();
	const Vector3f accumulatedForce = aRigidbody->GetAccumulatedForceDebug();
	const Vector3f accumulatedTorque = aRigidbody->GetAccumulatedTorqueDebug();
	const Vector3f inertiaDiagonal = aRigidbody->GetBodyInertiaDiagonalDebug();
	debug << std::fixed << std::setprecision(4);
	debug << "reason=" << (aReason ? aReason : "unspecified") << "\n";
	debug << "scenario=" << myScenarioName << "\n";
	debug << "frame=" << myFrameIndex << "\n";
	debug << "time=" << myTime << "\n";
	debug << "body_pos=" << pos.X << "," << pos.Y << "," << pos.Z << "\n";
	debug << "body_rot=" << rot.Pitch << "," << rot.Roll << "," << rot.Yaw << "\n";
	debug << "body_vel=" << aRigidbody->myVelocity.X << "," << aRigidbody->myVelocity.Y << "," << aRigidbody->myVelocity.Z << "\n";
	debug << "body_ang_vel=" << aRigidbody->myAngularVelocity.X << "," << aRigidbody->myAngularVelocity.Y << "," << aRigidbody->myAngularVelocity.Z << "\n";
	debug << "body_mass=" << aRigidbody->myMass << "\n";
	debug << "body_contact_count=" << aRigidbody->GetContactCount() << "\n";
	debug << "body_terrain_contact_count=" << aRigidbody->GetTerrainContactCount() << "\n";
	debug << "body_accumulated_force=" << accumulatedForce.X << "," << accumulatedForce.Y << "," << accumulatedForce.Z << "\n";
	debug << "body_accumulated_torque=" << accumulatedTorque.X << "," << accumulatedTorque.Y << "," << accumulatedTorque.Z << "\n";
	debug << "body_inertia_diagonal=" << inertiaDiagonal.X << "," << inertiaDiagonal.Y << "," << inertiaDiagonal.Z << "\n";
	const Vector3f colliderCenter = aRigidbody->GetColliderCenterWorld();
	const Vector3f centerOfMass = aRigidbody->GetCenterOfMassWorld();
	const Vector3f averageContactNormal = aRigidbody->GetAverageContactNormal();
	const Vector3f averageContactPoint = aRigidbody->GetAverageContactPoint();
	debug << "body_collider_center=" << colliderCenter.X << "," << colliderCenter.Y << "," << colliderCenter.Z << "\n";
	debug << "body_center_of_mass=" << centerOfMass.X << "," << centerOfMass.Y << "," << centerOfMass.Z << "\n";
	debug << "body_avg_contact_normal=" << averageContactNormal.X << "," << averageContactNormal.Y << "," << averageContactNormal.Z << "\n";
	debug << "body_avg_contact_point=" << averageContactPoint.X << "," << averageContactPoint.Y << "," << averageContactPoint.Z << "\n";
	debug << "grounded=" << (aIsGrounded ? 1 : 0) << "\n";
	debug << "air_time=" << aAirTime << "\n";
	debug << "grounded_count=" << aGroundedCount << "\n";
	debug << "contact_ratio=" << aContactRatio << "\n";
	debug << "rear_contact_ratio=" << aRearContactRatio << "\n";
	debug << "first_contact_time=" << myFirstContactTime << "\n";
	debug << "first_contact_frame=" << myFirstContactFrame << "\n";
	debug << "first_contact_grounded_count=" << myFirstContactGroundedCount << "\n";
	debug << "first_touchdown_time=" << myFirstTouchdownTime << "\n";
	debug << "first_touchdown_frame=" << myFirstTouchdownFrame << "\n";
	debug << "first_touchdown_grounded_count=" << myFirstTouchdownGroundedCount << "\n";
	debug << "first_touchdown_wheel_index=" << myFirstTouchdownWheelIndex << "\n";
	debug << "body_under_depth=" << aBodyUnderDepth << "\n";
	debug << "pitch_abs=" << aPitchAbs << "\n";
	debug << "roll_abs=" << aRollAbs << "\n";
	debug << "pitch_rate_deg=" << aPitchRateDeg << "\n";
	debug << "roll_rate_deg=" << aRollRateDeg << "\n";
	debug << "yaw_rate_deg=" << aYawRateDeg << "\n";
	debug << "drive_force=" << aDriveForceMagnitude << "\n";
	debug << "lateral_force=" << aLateralForceMagnitude << "\n";
	debug << "brake_force=" << aBrakeForceMagnitude << "\n";

	for (int i = 0; i < aWheelCount; ++i)
	{
		const WheelTelemetry& wheel = someWheelTelemetry[i];
		const bool hasTerrainManifold = wheel.wheelTerrainContactCount > 0;
		const PhysicsWorld::TerrainSample terrain = PhysicsWorld::Get().GetTerrainSample(wheel.worldPos.X, wheel.worldPos.Z);
		const Vector3f planePoint = hasTerrainManifold ? wheel.wheelAverageContactPoint : terrain.point;
		const Vector3f planeNormal = hasTerrainManifold ? wheel.wheelAverageContactNormal : terrain.normal;
		const float centerSignedDistance = (wheel.worldPos - planePoint).Dot(planeNormal);
		const float centerUnderDepth = ClampMin(-centerSignedDistance, 0.0f);
		const float wheelUnderDepth = ClampMin(aWheelRadius - centerSignedDistance, 0.0f);
		const float supportPlaneDistance = std::fabs((wheel.supportPoint - planePoint).Dot(planeNormal) - aWheelRadius);
		debug << "wheel" << i
			<< "="
			<< " grounded:" << (wheel.grounded ? 1 : 0)
			<< " world:" << wheel.worldPos.X << "," << wheel.worldPos.Y << "," << wheel.worldPos.Z
			<< " support:" << wheel.supportPoint.X << "," << wheel.supportPoint.Y << "," << wheel.supportPoint.Z
			<< " normal:" << wheel.normal.X << "," << wheel.normal.Y << "," << wheel.normal.Z
			<< " gap:" << wheel.gap
			<< " compression:" << wheel.compression
			<< " suspension_axial:" << wheel.suspensionAxialLength
			<< " suspension_clamped:" << wheel.suspensionClampedLength
			<< " suspension_rest:" << wheel.suspensionRestLength
			<< " suspension_max:" << wheel.suspensionMaxLength
			<< " suspension_spring:" << wheel.suspensionSpringForce
			<< " suspension_lateral:" << wheel.suspensionLateralForce
			<< " suspension_limit:" << wheel.suspensionLimitForce
			<< " suspension_unclamped:" << wheel.suspensionUnclampedForce
			<< " suspension_total:" << wheel.suspensionTotalForce
			<< " suspension_cap:" << wheel.suspensionMaxConstraintForce
			<< " suspension_spring_scale:" << wheel.suspensionSpringScale
			<< " suspension_lateral_scale:" << wheel.suspensionLateralScale
			<< " suspension_supported:" << (wheel.suspensionWheelSupported ? 1 : 0)
			<< " suspension_force_solve_supported:" << (wheel.suspensionForceSolveSupported ? 1 : 0)
			<< " suspension_axial_error:" << wheel.suspensionAxialError
			<< " suspension_lateral_error:" << wheel.suspensionLateralError
			<< " wheel_contact_count:" << wheel.wheelContactCount
			<< " wheel_terrain_contact_count:" << wheel.wheelTerrainContactCount
			<< " wheel_avg_contact_point:" << wheel.wheelAverageContactPoint.X << "," << wheel.wheelAverageContactPoint.Y << "," << wheel.wheelAverageContactPoint.Z
			<< " wheel_avg_contact_normal:" << wheel.wheelAverageContactNormal.X << "," << wheel.wheelAverageContactNormal.Y << "," << wheel.wheelAverageContactNormal.Z
			<< " wheel_terrain_load_estimate:" << wheel.wheelTerrainLoadEstimate
			<< " wheel_load:" << wheel.wheelLoad
			<< " wheel_angular_speed:" << wheel.wheelAngularSpeed
			<< " slip_ratio:" << wheel.slipRatio
			<< " slip_angle_deg:" << (wheel.slipAngle * kRadToDeg)
			<< " center_under:" << centerUnderDepth
			<< " wheel_under:" << wheelUnderDepth
			<< " support_plane_error:" << supportPlaneDistance
			<< "\n";
	}
	WriteCallstack(debug, 1);
	myInstabilitySnapshotText = debug.str();
}

void CarAutotest::WriteTouchdownSnapshot(
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
	float aYawRateDeg)
{
	if (myTouchdownSnapshotWritten || !aOwner || !aRigidbody)
	{
		return;
	}

	myTouchdownSnapshotWritten = true;

	std::ostringstream summary;
	const Rotator rot = aOwner->GetTransform().GetRotation();
	const Vector3f pos = aOwner->GetLocation();
	summary << std::fixed << std::setprecision(4);
	summary << "scenario=" << myScenarioName << "\n";
	summary << "touchdown_frame=" << myFirstTouchdownFrame << "\n";
	summary << "touchdown_time=" << myFirstTouchdownTime << "\n";
	summary << "first_contact_frame=" << myFirstContactFrame << "\n";
	summary << "first_contact_time=" << myFirstContactTime << "\n";
	summary << "touchdown_grounded_count=" << myFirstTouchdownGroundedCount << "\n";
	summary << "touchdown_wheel_index=" << myFirstTouchdownWheelIndex << "\n";
	summary << "body_pos=" << pos.X << "," << pos.Y << "," << pos.Z << "\n";
	summary << "body_rot=" << rot.Pitch << "," << rot.Roll << "," << rot.Yaw << "\n";
	summary << "body_vel=" << aRigidbody->myVelocity.X << "," << aRigidbody->myVelocity.Y << "," << aRigidbody->myVelocity.Z << "\n";
	summary << "body_ang_vel=" << aRigidbody->myAngularVelocity.X << "," << aRigidbody->myAngularVelocity.Y << "," << aRigidbody->myAngularVelocity.Z << "\n";
	summary << "contact_ratio=" << aContactRatio << "\n";
	summary << "rear_contact_ratio=" << aRearContactRatio << "\n";
	summary << "grounded=" << (aIsGrounded ? 1 : 0) << "\n";
	summary << "air_time=" << aAirTime << "\n";
	summary << "body_under_depth=" << aBodyUnderDepth << "\n";
	summary << "pitch_abs=" << aPitchAbs << "\n";
	summary << "roll_abs=" << aRollAbs << "\n";
	summary << "pitch_rate_deg=" << aPitchRateDeg << "\n";
	summary << "roll_rate_deg=" << aRollRateDeg << "\n";
	summary << "yaw_rate_deg=" << aYawRateDeg << "\n";
	summary << "drive_force=" << aDriveForceMagnitude << "\n";
	summary << "lateral_force=" << aLateralForceMagnitude << "\n";
	summary << "brake_force=" << aBrakeForceMagnitude << "\n";

	for (int i = 0; i < aWheelCount; ++i)
	{
		const WheelTelemetry& wheel = someWheelTelemetry[i];
		summary
			<< "wheel" << i
			<< "= grounded:" << (wheel.grounded ? 1 : 0)
			<< " world:" << wheel.worldPos.X << "," << wheel.worldPos.Y << "," << wheel.worldPos.Z
			<< " support:" << wheel.supportPoint.X << "," << wheel.supportPoint.Y << "," << wheel.supportPoint.Z
			<< " normal:" << wheel.normal.X << "," << wheel.normal.Y << "," << wheel.normal.Z
			<< " gap:" << wheel.gap
			<< " compression:" << wheel.compression
			<< " suspension_axial:" << wheel.suspensionAxialLength
			<< " suspension_clamped:" << wheel.suspensionClampedLength
			<< " suspension_rest:" << wheel.suspensionRestLength
			<< " suspension_max:" << wheel.suspensionMaxLength
			<< " suspension_spring:" << wheel.suspensionSpringForce
			<< " suspension_lateral:" << wheel.suspensionLateralForce
			<< " suspension_limit:" << wheel.suspensionLimitForce
			<< " suspension_unclamped:" << wheel.suspensionUnclampedForce
			<< " suspension_total:" << wheel.suspensionTotalForce
			<< " suspension_cap:" << wheel.suspensionMaxConstraintForce
			<< " suspension_supported:" << (wheel.suspensionWheelSupported ? 1 : 0)
			<< " suspension_force_solve_supported:" << (wheel.suspensionForceSolveSupported ? 1 : 0)
			<< " suspension_axial_error:" << wheel.suspensionAxialError
			<< " suspension_lateral_error:" << wheel.suspensionLateralError
			<< " wheel_contact_count:" << wheel.wheelContactCount
			<< " wheel_terrain_contact_count:" << wheel.wheelTerrainContactCount
			<< " wheel_load:" << wheel.wheelLoad
			<< " wheel_angular_speed:" << wheel.wheelAngularSpeed
			<< " slip_ratio:" << wheel.slipRatio
			<< " slip_angle_deg:" << (wheel.slipAngle * kRadToDeg)
			<< "\n";
	}
	myTouchdownSummaryText = summary.str();
	myTouchdownCsvText = myDebugCsvHeader;
	for (const std::string& line : myRecentDebugFrames)
	{
		myTouchdownCsvText += line;
	}

	myTouchdownWindowFramesRemaining = 8;
}

void CarAutotest::WriteStartMarker(ModelInstance* aOwner, RigidbodyComponent* aRigidbody) const
{
	std::ofstream started("CarDriveAutotest.started", std::ios::out | std::ios::trunc);
	if (!started.is_open() || !aOwner)
	{
		return;
	}

	ID3D11ShaderResourceView* ownerTextureResource = aOwner->GetTextureResource();
	started << "started\n";
	started << "scenario=" << myScenarioName << "\n";
	started << "has_rigidbody=" << (aRigidbody ? 1 : 0) << "\n";
	started << "owner_model=";
	if (aOwner->GetModel() && aOwner->GetModel()->GetPath()) started << aOwner->GetModel()->GetPath();
	started << "\n";
	started << "owner_texture=";
	if (aOwner->GetTexture()) started << aOwner->GetTexture();
	started << "\n";
	started << "owner_texture_loaded=" << (ownerTextureResource ? 1 : 0) << "\n";
}

void CarAutotest::PrepareScenarioStart(ModelInstance* aOwner, RigidbodyComponent* aRigidbody)
{
	if (!aOwner || !aRigidbody)
	{
		return;
	}

	const Transform oldRootTransform = aOwner->GetTransform();

	float bestYaw = aOwner->GetTransform().GetRotation().Yaw;
	float bestScore = -100000.0f;
	for (int i = 0; i < 32; ++i)
	{
		const float yaw = (360.0f / 32.0f) * static_cast<float>(i);
		const Vector3f dir = ForwardFromYaw(yaw);
		float score = 0.0f;
		float prevHeight = myStartTerrain;
		bool validPath = true;

		for (float d = 4.0f; d <= 18.0f; d += 2.0f)
		{
			const Vector3f samplePos = myStartPos + dir * d;
			const float sampleTerrain = PhysicsWorld::Get().GetTerrainHeight(samplePos.X, samplePos.Z);
			const float segmentRise = sampleTerrain - prevHeight;
			const float segmentSlope = segmentRise / 2.0f;

			if (segmentSlope > 0.55f || segmentSlope < -0.35f)
			{
				validPath = false;
				break;
			}

			score += segmentRise * 3.0f;
			score -= std::fabs(segmentSlope) * 1.5f;
			prevHeight = sampleTerrain;
		}

		if (!validPath)
		{
			continue;
		}

		if (score > bestScore)
		{
			bestScore = score;
			bestYaw = yaw;
		}
	}

	Transform newRootTransform = oldRootTransform;
	Rotator rot = oldRootTransform.GetRotation();
	rot.Yaw = bestYaw;
	newRootTransform.SetRotation(rot);
	myStartYaw = bestYaw;

	if (myScenario == Scenario::Drop)
	{
		const float terrain = PhysicsWorld::Get().GetTerrainHeight(myStartPos.X, myStartPos.Z);
		Vector3f pos = myStartPos;
		pos.Y = terrain + 6.0f;
		newRootTransform.SetPosition(pos);
		myStartPos = pos;
		myStartTerrain = terrain;
	}

	aOwner->SetTransform(newRootTransform);
	ApplyRootTransformToChildren(aOwner, oldRootTransform, newRootTransform);
	ResetRigidbodyStateRecursive(aOwner);
}

void CarAutotest::Initialize(ModelInstance* aOwner, RigidbodyComponent* aRigidbody)
{
	char* autoTestEnv = nullptr;
	size_t autoTestEnvLen = 0;
	errno_t envErr = _dupenv_s(&autoTestEnv, &autoTestEnvLen, "TE_AUTOTEST_CAR");
	const bool envEnabled = (envErr == 0 && autoTestEnv && autoTestEnv[0] != '\0' && autoTestEnv[0] != '0');
	if (autoTestEnv)
	{
		free(autoTestEnv);
	}

	std::ifstream enableFile("CarDriveAutotest.enable");
	const bool fileEnabled = enableFile.good();
	myEnabled = envEnabled || fileEnabled;
	if (!myEnabled || !aOwner)
	{
		return;
	}

	myScenario = ParseScenarioFromEnvironment();
	myScenarioName = ScenarioName(myScenario);
	char* preserveSceneEnv = nullptr;
	size_t preserveSceneEnvLen = 0;
	errno_t preserveErr = _dupenv_s(&preserveSceneEnv, &preserveSceneEnvLen, "TE_AUTOTEST_PRESERVE_SCENE");
	myPreserveRuntimeScene = (preserveErr == 0 && preserveSceneEnv && preserveSceneEnv[0] != '\0' && preserveSceneEnv[0] != '0');
	if (preserveSceneEnv)
	{
		free(preserveSceneEnv);
	}
	myStartPos = aOwner->GetLocation();
	myStartTerrain = PhysicsWorld::Get().GetTerrainHeight(myStartPos.X, myStartPos.Z);
	ResetMetrics();
	BeginDebugCapture();
	WriteStartMarker(aOwner, aRigidbody);
}

void CarAutotest::Finish(const char* aStatus, const char* aReason)
{
	if (!myEnabled || myFinished)
	{
		return;
	}

	myFinished = true;

	std::ofstream log("CarDriveAutotest.log", std::ios::out | std::ios::trunc);
	if (!log.is_open())
	{
		return;
	}

	log << "status=" << (aStatus ? aStatus : "COMPLETE") << "\n";
	log << "scenario=" << myScenarioName << "\n";
	log << "reason=" << (aReason ? aReason : "") << "\n";
	log << "max_terrain_gain=" << myMaxTerrainGain << "\n";
	log << "max_yaw_delta=" << myMaxYawDelta << "\n";
	log << "max_speed=" << myMaxSpeed << "\n";
	log << "max_distance=" << myMaxDistance << "\n";
	log << "max_contact_ratio=" << myMaxContactRatio << "\n";
	log << "max_rear_contact_ratio=" << myMaxRearContactRatio << "\n";
	log << "last_contact_ratio=" << myLastContactRatio << "\n";
	log << "last_rear_contact_ratio=" << myLastRearContactRatio << "\n";
	log << "first_contact_time=" << myFirstContactTime << "\n";
	log << "first_contact_frame=" << myFirstContactFrame << "\n";
	log << "first_contact_grounded_count=" << myFirstContactGroundedCount << "\n";
	log << "first_touchdown_time=" << myFirstTouchdownTime << "\n";
	log << "first_touchdown_frame=" << myFirstTouchdownFrame << "\n";
	log << "first_touchdown_grounded_count=" << myFirstTouchdownGroundedCount << "\n";
	log << "first_touchdown_wheel_index=" << myFirstTouchdownWheelIndex << "\n";
	log << "max_pitch_abs=" << myMaxPitchAbs << "\n";
	log << "max_roll_abs=" << myMaxRollAbs << "\n";
	log << "first_instability_time=" << myFirstInstabilityTime << "\n";
	log << "body_terrain_hits=" << myBodyTerrainHits << "\n";
	log << "max_body_under_terrain_depth=" << myMaxBodyUnderTerrainDepth << "\n";
	log << "max_wheel_under_terrain_depth=" << myMaxWheelUnderTerrainDepth << "\n";
	log << "max_wheel_center_under_terrain_depth=" << myMaxWheelCenterUnderTerrainDepth << "\n";
	log << "max_wheel_body_distance=" << myMaxWheelBodyDistance << "\n";
	log << "max_support_point_plane_error=" << myMaxSupportPointPlaneError << "\n";
	log << "max_drive_force=" << myMaxDriveForceMagnitude << "\n";
	log << "max_lateral_force=" << myMaxLateralForceMagnitude << "\n";
	log << "max_brake_force=" << myMaxBrakeForceMagnitude << "\n";
	log << "max_suspension_total_force=" << myMaxSuspensionTotalForce << "\n";
	log << "max_airborne_suspension_force=" << myMaxAirborneSuspensionForce << "\n";
	log << "max_grounded_pitch_rate_deg=" << myMaxGroundedPitchRateDeg << "\n";
	log << "max_grounded_roll_rate_deg=" << myMaxGroundedRollRateDeg << "\n";
	log << "max_grounded_yaw_rate_deg=" << myMaxGroundedYawRateDeg << "\n";
	log << "max_wheel_load=" << myMaxWheelLoad << "\n";
	log << "max_abs_slip_ratio=" << myMaxAbsSlipRatio << "\n";
	log << "max_abs_slip_angle_deg=" << myMaxAbsSlipAngleDeg << "\n";
	log << "max_suspension_axial_error=" << myMaxSuspensionAxialError << "\n";
	log << "max_suspension_lateral_error=" << myMaxSuspensionLateralError << "\n";
	log << "ground_flip_events=" << myGroundFlipEvents << "\n";
	log << "landing_spike_events=" << myLandingSpikeEvents << "\n";
	log << "final_forward_distance=" << myFinalForwardDistance << "\n";
	log << "final_right_distance=" << myFinalRightDistance << "\n";
	log << "max_abs_right_distance=" << myMaxAbsRightDistance << "\n";
	log << "final_yaw_delta=" << myFinalYawDelta << "\n";
	log << "all_wheels_connected=" << (myAllFourWheelsConnected ? 1 : 0) << "\n";
	log << "all_wheels_grounded=" << (myAllFourWheelsGrounded ? 1 : 0) << "\n";
	log << "debug_reason=" << myDebugFailureReason << "\n";
	log << "debug_csv=CarDriveAutotest.debug.csv" << "\n";
	log << "debug_snapshot=CarDriveAutotest.debug.txt" << "\n";
	log << "touchdown_csv=CarDriveAutotest.touchdown.csv" << "\n";
	log << "touchdown_snapshot=CarDriveAutotest.touchdown.txt" << "\n";

	if (myDebugCsv.is_open())
	{
		myDebugCsv.flush();
		myDebugCsv.close();
	}
	if (!myTouchdownSummaryText.empty())
	{
		std::ofstream touchdownSummary("CarDriveAutotest.touchdown.txt", std::ios::out | std::ios::trunc);
		if (touchdownSummary.is_open())
		{
			touchdownSummary << myTouchdownSummaryText;
		}
	}
	if (!myTouchdownCsvText.empty())
	{
		std::ofstream touchdownCsv("CarDriveAutotest.touchdown.csv", std::ios::out | std::ios::trunc);
		if (touchdownCsv.is_open())
		{
			touchdownCsv << myTouchdownCsvText;
		}
	}
	if (!myInstabilitySnapshotText.empty())
	{
		std::ofstream debugSnapshot("CarDriveAutotest.debug.txt", std::ios::out | std::ios::trunc);
		if (debugSnapshot.is_open())
		{
			debugSnapshot << myInstabilitySnapshotText;
		}
	}
	::PostQuitMessage(0);
}

void CarAutotest::ApplyScript(float aDeltaTime, float& inOutThrottleInput, float& inOutSteerInput, bool& outBraking)
{
	if (!myEnabled || myFinished)
	{
		return;
	}

	myTime += aDeltaTime;
	{
		std::ofstream status("CarDriveAutotest.status", std::ios::out | std::ios::trunc);
		status << "time=" << myTime << "\n";
		status << "scenario=" << myScenarioName << "\n";
	}

	switch (myScenario)
	{
	case Scenario::PlayStart:
		inOutThrottleInput = 0.0f;
		inOutSteerInput = 0.0f;
		outBraking = false;
		if (myTime >= 5.0f)
		{
			Finish("COMPLETE", "play_start_complete");
		}
		break;

	case Scenario::Drop:
		inOutThrottleInput = 0.0f;
		inOutSteerInput = 0.0f;
		outBraking = false;
		if (myTime >= 4.0f)
		{
			Finish("COMPLETE", "drop_complete");
		}
		break;

	case Scenario::Forward:
		if (myFirstContactTime < 0.0f || (myTime - myFirstContactTime) < 0.35f)
		{
			inOutThrottleInput = 0.0f; inOutSteerInput = 0.0f; outBraking = false;
		} else if ((myTime - myFirstContactTime) < 3.1f)
		{
			inOutThrottleInput = 1.0f; inOutSteerInput = 0.0f; outBraking = false;
		} else
		{
			Finish("COMPLETE", "forward_complete");
		}
		break;

	case Scenario::SteerLeft:
		if (myFirstContactTime < 0.0f || (myTime - myFirstContactTime) < 0.35f)
		{
			inOutThrottleInput = 0.0f; inOutSteerInput = 0.0f; outBraking = false;
		} else if ((myTime - myFirstContactTime) < 3.1f)
		{
			inOutThrottleInput = 1.0f; inOutSteerInput = -0.8f; outBraking = false;
		} else
		{
			Finish("COMPLETE", "steer_left_complete");
		}
		break;

	case Scenario::SteerRight:
		if (myFirstContactTime < 0.0f || (myTime - myFirstContactTime) < 0.35f)
		{
			inOutThrottleInput = 0.0f; inOutSteerInput = 0.0f; outBraking = false;
		} else if ((myTime - myFirstContactTime) < 3.1f)
		{
			inOutThrottleInput = 1.0f; inOutSteerInput = 0.8f; outBraking = false;
		} else
		{
			Finish("COMPLETE", "steer_right_complete");
		}
		break;

	case Scenario::Drive:
	default:
		if (myTime < 0.6f)
		{
			inOutThrottleInput = 0.0f;
			inOutSteerInput = 0.0f;
			outBraking = false;
		} else if (myTime < 4.6f)
		{
			inOutThrottleInput = 1.0f;
			inOutSteerInput = 0.0f;
			outBraking = false;
		} else if (myTime < 6.4f)
		{
			inOutThrottleInput = 1.0f;
			inOutSteerInput = 0.8f;
			outBraking = false;
		} else if (myTime < 8.0f)
		{
			inOutThrottleInput = 0.0f;
			inOutSteerInput = 0.0f;
			outBraking = true;
		} else
		{
			Finish("COMPLETE", "drive_complete");
		}
		break;
	}
}

void CarAutotest::NoteWheelBindingState(ModelInstance* aOwner, RigidbodyComponent* aRigidbody, bool aAllFourWheelsConnected)
{
	if (!myEnabled || myFinished)
	{
		return;
	}

	if (!myFirstUpdateDone)
	{
		myFirstUpdateDone = true;
	}
	if (aAllFourWheelsConnected)
	{
		myAllFourWheelsConnected = true;
	}
	if (!myScenarioPrepared && aOwner && aRigidbody && aAllFourWheelsConnected)
	{
		if (!myPreserveRuntimeScene && myScenario != Scenario::PlayStart)
		{
			PrepareScenarioStart(aOwner, aRigidbody);
		}
		myScenarioPrepared = true;
	}
}

void CarAutotest::NoteFrame(
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
	float aAirTime)
{
	if (!myEnabled || myFinished || !aOwner || !aRigidbody)
	{
		return;
	}

	const Rotator currentRotation = aOwner->GetTransform().GetRotation();
	const float pitchAbs = std::fabs(WrapAngle180(currentRotation.Pitch));
	const float rollAbs = std::fabs(WrapAngle180(currentRotation.Roll));
	if (pitchAbs > myMaxPitchAbs) myMaxPitchAbs = pitchAbs;
	if (rollAbs > myMaxRollAbs) myMaxRollAbs = rollAbs;

	const Matrix4x4f bodyMatrix = aOwner->GetTransform().GetMatrix(true);
	const Vector3f bodyRight = bodyMatrix.GetRight().GetNormalized();
	const Vector3f bodyUp = bodyMatrix.GetUp().GetNormalized();
	const Vector3f bodyForward = bodyMatrix.GetForward().GetNormalized();
	const Vector3f currentPos = aOwner->GetLocation();
	const float pitchRateDeg = std::fabs(aRigidbody->myAngularVelocity.Dot(bodyRight) * kRadToDeg);
	const float rollRateDeg = std::fabs(aRigidbody->myAngularVelocity.Dot(bodyForward) * kRadToDeg);
	const float yawRateDeg = std::fabs(aRigidbody->myAngularVelocity.Dot(bodyUp) * kRadToDeg);
	const bool nonFiniteState =
		!IsFiniteVec(currentPos) ||
		!IsFiniteRot(currentRotation) ||
		!IsFiniteVec(aRigidbody->myVelocity) ||
		!IsFiniteVec(aRigidbody->myAngularVelocity);
	const bool absurdVelocity =
		MaxAbsComponent(aRigidbody->myVelocity) > 10000.0f ||
		MaxAbsComponent(aRigidbody->myAngularVelocity) > 10000.0f;

	if (aIsGrounded)
	{
		if (pitchRateDeg > myMaxGroundedPitchRateDeg) myMaxGroundedPitchRateDeg = pitchRateDeg;
		if (rollRateDeg > myMaxGroundedRollRateDeg) myMaxGroundedRollRateDeg = rollRateDeg;
		if (yawRateDeg > myMaxGroundedYawRateDeg) myMaxGroundedYawRateDeg = yawRateDeg;

		const bool groundedInstability = (pitchAbs > 65.0f || rollAbs > 65.0f || pitchRateDeg > 220.0f || rollRateDeg > 220.0f) && aAirTime < 0.15f;
		if (groundedInstability && !myGroundInstabilityActive)
		{
			++myGroundFlipEvents;
		}
		myGroundInstabilityActive = groundedInstability;
	}
	else
	{
		myGroundInstabilityActive = false;
	}

	const float bodyTerrain = PhysicsWorld::Get().GetTerrainHeight(currentPos.X, currentPos.Z);
	const float bodyBottom = currentPos.Y - aRigidbody->myHalfExtents.Y;
	const float bodyUnderDepth = ClampMin(bodyTerrain - bodyBottom, 0.0f);
	if (bodyUnderDepth > 0.01f)
	{
		++myBodyTerrainHits;
	}
	if (bodyUnderDepth > myMaxBodyUnderTerrainDepth) myMaxBodyUnderTerrainDepth = bodyUnderDepth;
	bool firstTouchdownThisFrame = false;

	if (someWheelTelemetry && aWheelCount > 0)
	{
		float frameMaxSuspensionForce = 0.0f;
		bool frameHadSupportedSuspensionForce = false;
		int firstGroundedWheelThisFrame = -1;
		for (int i = 0; i < aWheelCount; ++i)
		{
			const WheelTelemetry& wheel = someWheelTelemetry[i];
			const bool hasTerrainManifold = wheel.wheelTerrainContactCount > 0;
			const PhysicsWorld::TerrainSample terrain = PhysicsWorld::Get().GetTerrainSample(wheel.worldPos.X, wheel.worldPos.Z);
			const Vector3f planePoint = hasTerrainManifold ? wheel.wheelAverageContactPoint : terrain.point;
			const Vector3f planeNormal = hasTerrainManifold ? wheel.wheelAverageContactNormal : terrain.normal;
			const float centerSignedDistance = (wheel.worldPos - planePoint).Dot(planeNormal);
			const float sampledGap = centerSignedDistance - aWheelRadius;
			const float centerUnderDepth = ClampMin(-centerSignedDistance, 0.0f);
			const float wheelUnderDepth = ClampMin(-sampledGap, 0.0f);
			const float supportPlaneDistance = std::fabs((wheel.supportPoint - planePoint).Dot(planeNormal) - aWheelRadius);
			const float wheelBodyDistance = (wheel.worldPos - currentPos).Length();
			const bool hasUsableSupportMetric = wheel.grounded || hasTerrainManifold || wheel.suspensionWheelSupported;

			if (centerUnderDepth > myMaxWheelCenterUnderTerrainDepth) myMaxWheelCenterUnderTerrainDepth = centerUnderDepth;
			if (wheelUnderDepth > myMaxWheelUnderTerrainDepth) myMaxWheelUnderTerrainDepth = wheelUnderDepth;
			if (wheelBodyDistance > myMaxWheelBodyDistance) myMaxWheelBodyDistance = wheelBodyDistance;
			if (hasUsableSupportMetric && supportPlaneDistance > myMaxSupportPointPlaneError) myMaxSupportPointPlaneError = supportPlaneDistance;
			if (wheel.wheelLoad > myMaxWheelLoad) myMaxWheelLoad = wheel.wheelLoad;
			const float absSlipRatio = std::fabs(wheel.slipRatio);
			if (absSlipRatio > myMaxAbsSlipRatio) myMaxAbsSlipRatio = absSlipRatio;
			const float absSlipAngleDeg = std::fabs(wheel.slipAngle * kRadToDeg);
			if (absSlipAngleDeg > myMaxAbsSlipAngleDeg) myMaxAbsSlipAngleDeg = absSlipAngleDeg;
			if (wheel.suspensionAxialError > myMaxSuspensionAxialError) myMaxSuspensionAxialError = wheel.suspensionAxialError;
			if (wheel.suspensionLateralError > myMaxSuspensionLateralError) myMaxSuspensionLateralError = wheel.suspensionLateralError;
			if (wheel.suspensionTotalForce > frameMaxSuspensionForce) frameMaxSuspensionForce = wheel.suspensionTotalForce;
			if (wheel.suspensionForceSolveSupported && wheel.suspensionTotalForce > 0.0f)
			{
				frameHadSupportedSuspensionForce = true;
			}
			if (firstGroundedWheelThisFrame < 0 && wheel.grounded)
			{
				firstGroundedWheelThisFrame = i;
			}
		}
		if (frameMaxSuspensionForce > myMaxSuspensionTotalForce) myMaxSuspensionTotalForce = frameMaxSuspensionForce;
		if (!frameHadSupportedSuspensionForce && frameMaxSuspensionForce > myMaxAirborneSuspensionForce) myMaxAirborneSuspensionForce = frameMaxSuspensionForce;
		const bool touchdownThisFrame = (myPreviousGroundedCount == 0 && aGroundedCount > 0);
		if (aGroundedCount > 0 && myFirstContactTime < 0.0f)
		{
			myFirstContactTime = myTime;
			myFirstContactFrame = myFrameIndex;
			myFirstContactGroundedCount = aGroundedCount;
		}
		firstTouchdownThisFrame = touchdownThisFrame && myFirstTouchdownTime < 0.0f;
		if (firstTouchdownThisFrame)
		{
			myFirstTouchdownTime = myTime;
			myFirstTouchdownFrame = myFrameIndex;
			myFirstTouchdownGroundedCount = aGroundedCount;
			myFirstTouchdownWheelIndex = firstGroundedWheelThisFrame;
		}
		if (touchdownThisFrame && (pitchRateDeg > 500.0f || rollRateDeg > 500.0f || yawRateDeg > 500.0f || frameMaxSuspensionForce > 1200.0f))
		{
			++myLandingSpikeEvents;
			if (myDebugFailureReason.empty())
			{
				myDebugFailureReason = "touchdown_spike";
			}
			WriteInstabilitySnapshot(
				myDebugFailureReason.c_str(),
				aOwner,
				aRigidbody,
				someWheelTelemetry,
				aWheelCount,
				aWheelRadius,
				aContactRatio,
				aRearContactRatio,
				aGroundedCount,
				aDriveForceMagnitude,
				aLateralForceMagnitude,
				aBrakeForceMagnitude,
				aIsGrounded,
				aAirTime,
				bodyUnderDepth,
				pitchAbs,
				rollAbs,
				pitchRateDeg,
				rollRateDeg,
				yawRateDeg);
		}
	}

	// Grace window must be computed AFTER myFirstTouchdownTime is set so the first
	// touchdown frame itself is protected (previously it was computed before, causing
	// the first-contact frame to always slip through and trigger weak_support).
	const bool touchdownGraceWindowActive = (myFirstTouchdownTime >= 0.0f) && ((myTime - myFirstTouchdownTime) < 0.35f);

	AppendDebugFrame(
		aOwner,
		aRigidbody,
		someWheelTelemetry,
		aWheelCount,
		aWheelRadius,
		aContactRatio,
		aRearContactRatio,
		aGroundedCount,
		aDriveForceMagnitude,
		aLateralForceMagnitude,
		aBrakeForceMagnitude,
		aIsGrounded,
		aAirTime,
		bodyUnderDepth,
		pitchAbs,
		rollAbs,
		pitchRateDeg,
		rollRateDeg,
		yawRateDeg);

	if (firstTouchdownThisFrame)
	{
		WriteTouchdownSnapshot(
			aOwner,
			aRigidbody,
			someWheelTelemetry,
			aWheelCount,
			aWheelRadius,
			aContactRatio,
			aRearContactRatio,
			aGroundedCount,
			aDriveForceMagnitude,
			aLateralForceMagnitude,
			aBrakeForceMagnitude,
			aIsGrounded,
			aAirTime,
			bodyUnderDepth,
			pitchAbs,
			rollAbs,
			pitchRateDeg,
			rollRateDeg,
			yawRateDeg);
	}

	if (nonFiniteState || absurdVelocity)
	{
		myUnstable = true;
		if (myFirstInstabilityTime < 0.0f)
		{
			myFirstInstabilityTime = myTime;
		}
		if (myDebugFailureReason.empty())
		{
			myDebugFailureReason = nonFiniteState ? "non_finite_state" : "absurd_velocity";
		}
		WriteInstabilitySnapshot(
			myDebugFailureReason.c_str(),
			aOwner,
			aRigidbody,
			someWheelTelemetry,
			aWheelCount,
			aWheelRadius,
			aContactRatio,
			aRearContactRatio,
			aGroundedCount,
			aDriveForceMagnitude,
			aLateralForceMagnitude,
			aBrakeForceMagnitude,
			aIsGrounded,
			aAirTime,
			bodyUnderDepth,
			pitchAbs,
			rollAbs,
			pitchRateDeg,
			rollRateDeg,
			yawRateDeg);
	}

	if (aDriveForceMagnitude > myMaxDriveForceMagnitude) myMaxDriveForceMagnitude = aDriveForceMagnitude;
	if (aLateralForceMagnitude > myMaxLateralForceMagnitude) myMaxLateralForceMagnitude = aLateralForceMagnitude;
	if (aBrakeForceMagnitude > myMaxBrakeForceMagnitude) myMaxBrakeForceMagnitude = aBrakeForceMagnitude;

	const bool shouldEvaluateWeakSupport = (myScenario != Scenario::PlayStart) || (myFirstTouchdownTime >= 0.0f);
	if (!myUnstable && myTime > 1.0f && shouldEvaluateWeakSupport)
	{
		const bool extremeAttitude = pitchAbs > 55.0f || rollAbs > 55.0f;
		const bool weakSupport = aContactRatio < 0.5f;
		if (!touchdownGraceWindowActive && (extremeAttitude || weakSupport))
		{
			myUnstable = true;
			myFirstInstabilityTime = myTime;
			if (myDebugFailureReason.empty())
			{
				myDebugFailureReason = extremeAttitude ? "extreme_attitude" : "weak_support";
			}
			WriteInstabilitySnapshot(
				myDebugFailureReason.c_str(),
				aOwner,
				aRigidbody,
				someWheelTelemetry,
				aWheelCount,
				aWheelRadius,
				aContactRatio,
				aRearContactRatio,
				aGroundedCount,
				aDriveForceMagnitude,
				aLateralForceMagnitude,
				aBrakeForceMagnitude,
				aIsGrounded,
				aAirTime,
				bodyUnderDepth,
				pitchAbs,
				rollAbs,
				pitchRateDeg,
				rollRateDeg,
				yawRateDeg);
		}
	}

	if (aContactRatio > myMaxContactRatio) myMaxContactRatio = aContactRatio;
	if (aRearContactRatio > myMaxRearContactRatio) myMaxRearContactRatio = aRearContactRatio;
	myLastContactRatio = aContactRatio;
	myLastRearContactRatio = aRearContactRatio;
	if (aGroundedCount == 4) myAllFourWheelsGrounded = true;

	const float terrainNow = PhysicsWorld::Get().GetTerrainHeight(currentPos.X, currentPos.Z);
	const float terrainGain = terrainNow - myStartTerrain;
	if (terrainGain > myMaxTerrainGain) myMaxTerrainGain = terrainGain;

	const float yawNow = aOwner->GetTransform().GetRotation().Yaw;
	const float yawDelta = WrapAngle180(yawNow - myStartYaw);
	const float yawDeltaAbs = std::fabs(yawDelta);
	if (yawDeltaAbs > myMaxYawDelta) myMaxYawDelta = yawDeltaAbs;
	myFinalYawDelta = yawDelta;

	const float horizSpeed = std::sqrt(aRigidbody->myVelocity.X * aRigidbody->myVelocity.X + aRigidbody->myVelocity.Z * aRigidbody->myVelocity.Z);
	if (horizSpeed > myMaxSpeed) myMaxSpeed = horizSpeed;

	const Vector3f delta = currentPos - myStartPos;
	const Vector3f startForward = ForwardFromYaw(myStartYaw);
	const Vector3f startRight = RightFromYaw(myStartYaw);
	myFinalForwardDistance = delta.Dot(startForward);
	myFinalRightDistance = delta.Dot(startRight);
	const float absRight = std::fabs(myFinalRightDistance);
	if (absRight > myMaxAbsRightDistance) myMaxAbsRightDistance = absRight;

	const float dist = DistanceXZ(currentPos, myStartPos);
	if (dist > myMaxDistance) myMaxDistance = dist;
	myPreviousGroundedCount = aGroundedCount;
	++myFrameIndex;
}

void CarAutotest::FailMissingRuntime()
{
	if (!myEnabled || myFinished)
	{
		return;
	}
	Finish("FAIL", "missing_rigidbody_or_owner");
}
