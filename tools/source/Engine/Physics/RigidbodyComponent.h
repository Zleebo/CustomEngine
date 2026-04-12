#pragma once
#include <Components/Component.h>
#include <Math/Vector.h>
#include <Math/Quaternion.h>
#include "Collider.h"

class RigidbodyComponent : public Component
{
public:
	enum class ColliderType
	{
		Box = 0,
		Sphere = 1
	};

	RigidbodyComponent()
	{
		EXPOSE_FLOAT_EX("Mass",         myMass,         0.01f, 0.01f, 10000.0f);
		EXPOSE_FLOAT_EX("Linear Drag",  myLinearDrag,   0.001f, 0.0f, 1.0f);
		EXPOSE_FLOAT_EX("Bounciness",   myBounciness,   0.01f, 0.0f, 1.0f);
		EXPOSE_BOOL(    "Is Static",    myIsStatic);
		EXPOSE_BOOL(    "Use Gravity",  myUseGravity);
		// When true, PhysicsWorld skips terrain collision for this body.
		// Use for bodies that should opt out of terrain response entirely.
		EXPOSE_BOOL(    "Ignore Terrain", myIgnoreTerrainCollision);
		EXPOSE_BOOL(    "Ignore Parent Collision", myIgnoreParentChildCollision);
		EXPOSE_INT(     "Collider Type", myColliderType);
		EXPOSE_VEC3(    "Half Extents", myHalfExtents);
		EXPOSE_FLOAT_EX("Sphere Radius", mySphereRadius, 0.01f, 0.01f, 100.0f);
		EXPOSE_VEC3(    "Collider Center", myColliderCenterLocal);
		EXPOSE_VEC3(    "Center Of Mass", myCenterOfMassLocal);
		EXPOSE_VEC3(    "Inertia Scale", myInertiaScale);
		EXPOSE_VEC3(    "Velocity",     myVelocity);
		EXPOSE_FLOAT_EX("Angular Drag", myAngularDrag, 0.001f, 0.0f, 10.0f);
		EXPOSE_VEC3(    "Angular Velocity", myAngularVelocity);
	}
	~RigidbodyComponent();

	void OnStart() override;
	void Update(float aDeltaTime) override;
	void Simulate(float aDeltaTime);

	AABB GetAABB() const;
	OBB GetOBB() const;
	Sphere GetSphere() const;
	ColliderType GetColliderType() const { return static_cast<ColliderType>(myColliderType); }
	Vector3f GetCenterOfMassWorld() const;
	Vector3f GetColliderCenterWorld() const;
	Vector3f GetVelocityAtPoint(const Vector3f& aWorldPoint) const;
	Vector3f InverseInertiaMultiplyWorld(const Vector3f& aWorldVector) const;
	Vector3f InertiaMultiplyWorld(const Vector3f& aWorldVector) const;
	bool HasAnyContact() const { return myContactCount > 0; }
	bool HasTerrainContact() const { return myTerrainContactCount > 0; }
	int GetContactCount() const { return myContactCount; }
	int GetTerrainContactCount() const { return myTerrainContactCount; }
	Vector3f GetAverageContactNormal() const;
	Vector3f GetAverageContactPoint() const;
	Vector3f GetAverageTerrainContactNormal() const;
	Vector3f GetAverageTerrainContactPoint() const;
	float GetAccumulatedTerrainNormalImpulse() const { return myAccumulatedTerrainNormalImpulse; }
	float GetTerrainLoadEstimate() const;
	Vector3f GetAccumulatedForceDebug() const { return myAccumulatedForce; }
	Vector3f GetAccumulatedTorqueDebug() const { return myAccumulatedTorque; }
	Vector3f GetBodyInertiaDiagonalDebug() const { return GetBodyInertiaDiagonal(); }

	Vector3f myVelocity{};
	Vector3f myAngularVelocity{};
	float myMass = 1.0f;
	float myLinearDrag = 0.05f;
	float myAngularDrag = 0.08f;
	float myBounciness = 0.3f;
	bool myIsStatic = false;
	bool myUseGravity = true;
	bool myIgnoreTerrainCollision = false;
	bool myIgnoreParentChildCollision = false;
	int myColliderType = static_cast<int>(ColliderType::Box);
	Vector3f myHalfExtents{ 0.5f, 0.5f, 0.5f };
	float mySphereRadius = 0.5f;
	Vector3f myColliderCenterLocal{ 0.0f, 0.0f, 0.0f };
	Vector3f myCenterOfMassLocal{ 0.0f, 0.0f, 0.0f };
	Vector3f myInertiaScale{ 1.0f, 1.0f, 1.0f };
	Quaternionf myOrientation{};

	// Instant velocity change (impulse)
	void AddForce(const Vector3f& aForce) { myVelocity = myVelocity + (aForce * (1.0f / myMass)); }
	// Accumulated force applied during the next physics step.
	void AddForceAccumulated(const Vector3f& aForce) { myAccumulatedForce = myAccumulatedForce + aForce; }
	void AddTorqueAccumulated(const Vector3f& aTorque) { myAccumulatedTorque = myAccumulatedTorque + aTorque; }
	void AddForceAccumulatedAtPoint(const Vector3f& aForce, const Vector3f& aWorldPoint);
	void AddImpulseAtPoint(const Vector3f& anImpulse, const Vector3f& aWorldPoint);
	void ApplyPositionImpulseAtPoint(const Vector3f& anImpulse, const Vector3f& aWorldPoint);
	void BeginPhysicsStep();
	void RegisterContact(const Vector3f& aNormal, const Vector3f& aPoint, bool aIsTerrainContact, float aNormalImpulse = 0.0f);

private:
	Vector3f myAccumulatedForce{};
	Vector3f myAccumulatedTorque{};
	Vector3f myAccumulatedContactNormal{};
	Vector3f myAccumulatedContactPoint{};
	Vector3f myAccumulatedTerrainContactNormal{};
	Vector3f myAccumulatedTerrainContactPoint{};
	float myAccumulatedTerrainNormalImpulse = 0.0f;
	int myContactCount = 0;
	int myTerrainContactCount = 0;
	float myLastPhysicsStepDeltaTime = 1.0f / 60.0f;
	bool myOrientationInitialized = false;
	Rotator myLastOwnerRotation{};

	void SyncOrientationFromOwner();
	void SyncOwnerRotationFromOrientation();
	Vector3f GetBodyInertiaDiagonal() const;

public:
	Quaternionf GetWorldOrientation() const;

private:
	static constexpr float ourGravity = -9.81f;
};
