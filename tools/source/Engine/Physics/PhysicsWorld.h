#pragma once
#include <vector>
#include <functional>

class RigidbodyComponent;
class SuspensionConstraintComponent;

class IPhysicsVehicleController
{
public:
	virtual ~IPhysicsVehicleController() = default;
	virtual void ApplyVehicleForces(float aDeltaTime) = 0;
};

class PhysicsWorld
{
public:
	static PhysicsWorld& Get();

	void Update(float aDeltaTime);

	void AddBody(RigidbodyComponent* aBody);
	void RemoveBody(RigidbodyComponent* aBody);
	void AddSuspensionConstraint(SuspensionConstraintComponent* aConstraint);
	void RemoveSuspensionConstraint(SuspensionConstraintComponent* aConstraint);
	void AddVehicleController(IPhysicsVehicleController* aController);
	void RemoveVehicleController(IPhysicsVehicleController* aController);

	using CollisionCallback = std::function<void(RigidbodyComponent*, RigidbodyComponent*)>;
	void SetOnCollision(CollisionCallback aCallback) { myCollisionCallback = std::move(aCallback); }

	struct TerrainCollisionMesh
	{
		const float* heightData = nullptr;
		int          width      = 0;
		int          height     = 0;
		float        offsetX    = 0.f;
		float        offsetY    = 0.f;
		float        offsetZ    = 0.f;
	};
	void SetTerrainMesh(const TerrainCollisionMesh& mesh) { myTerrainMesh = mesh; }

	struct TerrainSample
	{
		float height = 0.0f;
		Vector3f point = { 0.0f, 0.0f, 0.0f };
		Vector3f normal = { 0.0f, 1.0f, 0.0f };
	};

	struct TerrainSweepHit
	{
		bool hit = false;
		float fraction = 0.0f;
		float gap = 0.0f;
		Vector3f center = { 0.0f, 0.0f, 0.0f };
		Vector3f point = { 0.0f, 0.0f, 0.0f };
		Vector3f normal = { 0.0f, 1.0f, 0.0f };
	};

	float GetTerrainHeight(float worldX, float worldZ) const;
	TerrainSample GetTerrainSample(float worldX, float worldZ) const;
	bool SweepSphereAgainstTerrain(const Vector3f& aStartCenter, const Vector3f& aEndCenter, float aRadius, float aTolerance, TerrainSweepHit& outHit) const;

private:
	PhysicsWorld() = default;

	void ResolveCollisions();
	void ResolveTerrainCollisions();

	std::vector<RigidbodyComponent*> myBodies;
	std::vector<SuspensionConstraintComponent*> mySuspensionConstraints;
	std::vector<IPhysicsVehicleController*> myVehicleControllers;
	CollisionCallback myCollisionCallback;
	TerrainCollisionMesh myTerrainMesh;
};