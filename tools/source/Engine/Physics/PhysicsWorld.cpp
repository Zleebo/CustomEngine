#include <stdafx.h>
#include "PhysicsWorld.h"
#include "RigidbodyComponent.h"
#include "SuspensionConstraintComponent.h"
#include <Models/ModelInstance.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <fstream>

namespace
{
	constexpr float kCollisionFriction = 0.7f;
	constexpr float kTerrainFriction = 0.85f;
	constexpr int kSuspensionVelocityIterations = 4;

	struct OBBContact
	{
		Vector3f normal = { 0.0f, 1.0f, 0.0f };
		float penetration = 0.0f;
		bool intersecting = false;
	};

	struct SphereContact
	{
		Vector3f normal = { 0.0f, 1.0f, 0.0f };
		Vector3f point = { 0.0f, 0.0f, 0.0f };
		float penetration = 0.0f;
		bool intersecting = false;
	};

	float ProjectRadiusOntoAxis(const OBB& box, const Vector3f& axis)
	{
		return
			std::fabs(axis.Dot(box.myAxis[0])) * box.myExtents.X +
			std::fabs(axis.Dot(box.myAxis[1])) * box.myExtents.Y +
			std::fabs(axis.Dot(box.myAxis[2])) * box.myExtents.Z;
	}

	Vector3f GetSupportPoint(const OBB& box, const Vector3f& direction)
	{
		return box.myCenter +
			box.myAxis[0] * ((box.myAxis[0].Dot(direction) >= 0.0f ? 1.0f : -1.0f) * box.myExtents.X) +
			box.myAxis[1] * ((box.myAxis[1].Dot(direction) >= 0.0f ? 1.0f : -1.0f) * box.myExtents.Y) +
			box.myAxis[2] * ((box.myAxis[2].Dot(direction) >= 0.0f ? 1.0f : -1.0f) * box.myExtents.Z);
	}

	OBBContact ComputeOBBContact(const OBB& a, const OBB& b)
	{
		constexpr float axisEpsilon = 1e-5f;
		OBBContact result;
		float minOverlap = FLT_MAX;
		Vector3f bestAxis = { 0.0f, 1.0f, 0.0f };
		const Vector3f centerDelta = b.myCenter - a.myCenter;

		Vector3f axes[15];
		int axisCount = 0;
		for (int i = 0; i < 3; ++i) axes[axisCount++] = a.myAxis[i];
		for (int i = 0; i < 3; ++i) axes[axisCount++] = b.myAxis[i];
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				axes[axisCount++] = a.myAxis[i].Cross(b.myAxis[j]);
			}
		}

		for (int i = 0; i < axisCount; ++i)
		{
			Vector3f axis = axes[i];
			const float axisLength = axis.Length();
			if (axisLength <= axisEpsilon)
			{
				continue;
			}

			axis *= (1.0f / axisLength);
			const float radiusA = ProjectRadiusOntoAxis(a, axis);
			const float radiusB = ProjectRadiusOntoAxis(b, axis);
			const float distance = std::fabs(centerDelta.Dot(axis));
			const float overlap = (radiusA + radiusB) - distance;
			if (overlap <= 0.0f)
			{
				return result;
			}

			if (overlap < minOverlap)
			{
				minOverlap = overlap;
				bestAxis = (centerDelta.Dot(axis) < 0.0f) ? (axis * -1.0f) : axis;
			}
		}

		result.intersecting = true;
		result.normal = bestAxis;
		result.penetration = minOverlap;
		return result;
	}

	Vector3f ComputeContactPoint(const OBB& a, const OBB& b, const Vector3f& normal)
	{
		const Vector3f pointA = GetSupportPoint(a, normal);
		const Vector3f pointB = GetSupportPoint(b, normal * -1.0f);
		return (pointA + pointB) * 0.5f;
	}

	Vector3f ClosestPointOnOBB(const OBB& box, const Vector3f& point)
	{
		Vector3f d = point - box.myCenter;
		Vector3f closest = box.myCenter;
		for (int i = 0; i < 3; ++i)
		{
			float dist = d.Dot(box.myAxis[i]);
			if (dist > box.myExtents.myValues[i]) dist = box.myExtents.myValues[i];
			if (dist < -box.myExtents.myValues[i]) dist = -box.myExtents.myValues[i];
			closest += box.myAxis[i] * dist;
		}
		return closest;
	}

	SphereContact ComputeSphereSphereContact(const Sphere& a, const Sphere& b)
	{
		SphereContact result;
		const Vector3f delta = b.myCenter - a.myCenter;
		const float distance = delta.Length();
		const float radiusSum = a.myRadius + b.myRadius;
		const float penetration = radiusSum - distance;
		if (penetration <= 0.0f)
		{
			return result;
		}

		result.intersecting = true;
		result.normal = distance > 0.0001f ? delta * (1.0f / distance) : Vector3f{ 0.0f, 1.0f, 0.0f };
		result.penetration = penetration;
		result.point = a.myCenter + result.normal * a.myRadius;
		return result;
	}

	SphereContact ComputeSphereOBBContact(const Sphere& sphere, const OBB& box)
	{
		SphereContact result;
		const Vector3f closest = ClosestPointOnOBB(box, sphere.myCenter);
		const Vector3f delta = sphere.myCenter - closest;
		const float distance = delta.Length();
		const float penetration = sphere.myRadius - distance;
		if (penetration <= 0.0f)
		{
			return result;
		}

		result.intersecting = true;
		result.normal = distance > 0.0001f ? delta * (1.0f / distance) : box.myAxis[1];
		result.penetration = penetration;
		result.point = closest;
		return result;
	}
}

PhysicsWorld& PhysicsWorld::Get()
{
	static PhysicsWorld instance;
	return instance;
}

void PhysicsWorld::Update(float aDeltaTime)
{
	if (aDeltaTime <= 0.0f)
	{
		return;
	}

	constexpr int kPhysicsSubsteps = 8;
	const float subDt = aDeltaTime / static_cast<float>(kPhysicsSubsteps);

	for (int substep = 0; substep < kPhysicsSubsteps; ++substep)
	{
		for (auto* body : myBodies)
		{
			if (body)
			{
				body->BeginPhysicsStep();
			}
		}

		for (auto* constraint : mySuspensionConstraints)
		{
			if (constraint)
			{
				constraint->ApplySpringForces(subDt);
			}
		}

		for (int iteration = 0; iteration < kSuspensionVelocityIterations; ++iteration)
		{
			for (auto* constraint : mySuspensionConstraints)
			{
				if (constraint)
				{
					constraint->SolveVelocityConstraints(subDt);
				}
			}
		}

		for (auto* controller : myVehicleControllers)
		{
			if (controller)
			{
				controller->ApplyVehicleForces(subDt);
			}
		}

		for (auto* body : myBodies)
		{
			if (body)
			{
				body->Simulate(subDt);
			}
		}

		ResolveTerrainCollisions();
		ResolveCollisions();
	}
}

void PhysicsWorld::AddBody(RigidbodyComponent* aBody)
{
	myBodies.push_back(aBody);
}

void PhysicsWorld::RemoveBody(RigidbodyComponent* aBody)
{
	myBodies.erase(std::remove(myBodies.begin(), myBodies.end(), aBody), myBodies.end());
}

void PhysicsWorld::AddSuspensionConstraint(SuspensionConstraintComponent* aConstraint)
{
	if (!aConstraint) return;
	if (std::find(mySuspensionConstraints.begin(), mySuspensionConstraints.end(), aConstraint) == mySuspensionConstraints.end())
	{
		mySuspensionConstraints.push_back(aConstraint);
	}
}

void PhysicsWorld::RemoveSuspensionConstraint(SuspensionConstraintComponent* aConstraint)
{
	mySuspensionConstraints.erase(std::remove(mySuspensionConstraints.begin(), mySuspensionConstraints.end(), aConstraint), mySuspensionConstraints.end());
}

void PhysicsWorld::AddVehicleController(IPhysicsVehicleController* aController)
{
	if (!aController) return;
	if (std::find(myVehicleControllers.begin(), myVehicleControllers.end(), aController) == myVehicleControllers.end())
	{
		myVehicleControllers.push_back(aController);
	}
}

void PhysicsWorld::RemoveVehicleController(IPhysicsVehicleController* aController)
{
	myVehicleControllers.erase(std::remove(myVehicleControllers.begin(), myVehicleControllers.end(), aController), myVehicleControllers.end());
}

void PhysicsWorld::ResolveCollisions()
{
	for (size_t i = 0; i < myBodies.size(); i++)
	{
		for (size_t j = i + 1; j < myBodies.size(); j++)
		{
			auto* a = myBodies[i];
			auto* b = myBodies[j];

			if (a->myIsStatic && b->myIsStatic) continue;
			if (a->myOwner == nullptr || b->myOwner == nullptr) continue;
			if (a->myIgnoreParentChildCollision || b->myIgnoreParentChildCollision)
			{
				ModelInstance* parentA = a->myOwner->GetParent();
				ModelInstance* parentB = b->myOwner->GetParent();
				if (a->myOwner == parentB || b->myOwner == parentA || (parentA && parentA == parentB))
				{
					continue;
				}
			}

			const AABB aabbA = a->GetAABB();
			const AABB aabbB = b->GetAABB();
			if (!aabbA.Intersects(aabbB)) continue;

			if (myCollisionCallback) myCollisionCallback(a, b);

			Vector3f normal = { 0.0f, 1.0f, 0.0f };
			float penetration = 0.0f;
			Vector3f contactPoint = {};
			const auto typeA = a->GetColliderType();
			const auto typeB = b->GetColliderType();

			if (typeA == RigidbodyComponent::ColliderType::Sphere && typeB == RigidbodyComponent::ColliderType::Sphere)
			{
				const SphereContact contact = ComputeSphereSphereContact(a->GetSphere(), b->GetSphere());
				if (!contact.intersecting) continue;
				normal = contact.normal;
				penetration = contact.penetration;
				contactPoint = contact.point;
			}
			else if (typeA == RigidbodyComponent::ColliderType::Sphere && typeB == RigidbodyComponent::ColliderType::Box)
			{
				const SphereContact contact = ComputeSphereOBBContact(a->GetSphere(), b->GetOBB());
				if (!contact.intersecting) continue;
				normal = contact.normal;
				penetration = contact.penetration;
				contactPoint = contact.point;
			}
			else if (typeA == RigidbodyComponent::ColliderType::Box && typeB == RigidbodyComponent::ColliderType::Sphere)
			{
				const SphereContact contact = ComputeSphereOBBContact(b->GetSphere(), a->GetOBB());
				if (!contact.intersecting) continue;
				normal = contact.normal * -1.0f;
				penetration = contact.penetration;
				contactPoint = contact.point;
			}
			else
			{
				const OBBContact contact = ComputeOBBContact(a->GetOBB(), b->GetOBB());
				if (!contact.intersecting) continue;
				normal = contact.normal;
				penetration = contact.penetration;
				contactPoint = ComputeContactPoint(a->GetOBB(), b->GetOBB(), normal);
			}

			const float correction = penetration * 0.8f;
			float totalInvMass = (!a->myIsStatic ? 1.f / a->myMass : 0.f)
				+ (!b->myIsStatic ? 1.f / b->myMass : 0.f);
			if (totalInvMass > 0.f)
			{
				if (!a->myIsStatic)
				{
					Vector3f pos = a->myOwner->GetLocation();
					a->myOwner->SetLocation(pos - normal * (correction * (1.f / a->myMass) / totalInvMass));
				}
				if (!b->myIsStatic)
				{
					Vector3f pos = b->myOwner->GetLocation();
					b->myOwner->SetLocation(pos + normal * (correction * (1.f / b->myMass) / totalInvMass));
				}
			}

			const Vector3f ra = contactPoint - a->GetCenterOfMassWorld();
			const Vector3f rb = contactPoint - b->GetCenterOfMassWorld();
			const Vector3f velA = a->GetVelocityAtPoint(contactPoint);
			const Vector3f velB = b->GetVelocityAtPoint(contactPoint);
			const Vector3f relVelVec = velB - velA;
			const float relVel = relVelVec.Dot(normal);
			if (relVel > 0.f)
			{
				a->RegisterContact(normal * -1.0f, contactPoint, false, 0.0f);
				b->RegisterContact(normal, contactPoint, false, 0.0f);
				continue;
			}

			const Vector3f raCrossN = ra.Cross(normal);
			const Vector3f rbCrossN = rb.Cross(normal);
			const float denom = totalInvMass + normal.Dot(
				(a->myIsStatic ? Vector3f{} : a->InverseInertiaMultiplyWorld(raCrossN).Cross(ra)) +
				(b->myIsStatic ? Vector3f{} : b->InverseInertiaMultiplyWorld(rbCrossN).Cross(rb)));
			if (std::fabs(denom) <= 0.0001f)
			{
				a->RegisterContact(normal * -1.0f, contactPoint, false, 0.0f);
				b->RegisterContact(normal, contactPoint, false, 0.0f);
				continue;
			}

			const float e = (a->myBounciness + b->myBounciness) * 0.5f;
			const float normalImpulseMag = -(1.f + e) * relVel / denom;
			const Vector3f normalImpulse = normal * normalImpulseMag;

			a->RegisterContact(normal * -1.0f, contactPoint, false, normalImpulseMag);
			b->RegisterContact(normal, contactPoint, false, normalImpulseMag);

			if (!a->myIsStatic) a->AddImpulseAtPoint(normalImpulse * -1.0f, contactPoint);
			if (!b->myIsStatic) b->AddImpulseAtPoint(normalImpulse, contactPoint);

			Vector3f tangent = relVelVec - normal * relVel;
			const float tangentLen = tangent.Length();
			if (tangentLen > 0.0001f)
			{
				tangent *= (1.0f / tangentLen);
				const float tangentRelVel = relVelVec.Dot(tangent);
				const Vector3f raCrossT = ra.Cross(tangent);
				const Vector3f rbCrossT = rb.Cross(tangent);
				const float tangentDenom = totalInvMass + tangent.Dot(
					(a->myIsStatic ? Vector3f{} : a->InverseInertiaMultiplyWorld(raCrossT).Cross(ra)) +
					(b->myIsStatic ? Vector3f{} : b->InverseInertiaMultiplyWorld(rbCrossT).Cross(rb)));
				if (std::fabs(tangentDenom) > 0.0001f)
				{
					float jt = -tangentRelVel / tangentDenom;
					const float frictionLimit = normalImpulseMag * kCollisionFriction;
					if (jt > frictionLimit) jt = frictionLimit;
					if (jt < -frictionLimit) jt = -frictionLimit;
					const Vector3f frictionImpulse = tangent * jt;
					if (!a->myIsStatic) a->AddImpulseAtPoint(frictionImpulse * -1.0f, contactPoint);
					if (!b->myIsStatic) b->AddImpulseAtPoint(frictionImpulse, contactPoint);
				}
			}
		}
	}
}

PhysicsWorld::TerrainSample PhysicsWorld::GetTerrainSample(float worldX, float worldZ) const
{
	TerrainSample sample;
	if (!myTerrainMesh.heightData) return sample;
	const TerrainCollisionMesh& mesh = myTerrainMesh;

	float lx = worldX - mesh.offsetX;
	float lz = worldZ - mesh.offsetZ;

	lx = lx < 0.f ? 0.f : (lx > (float)mesh.width ? (float)mesh.width : lx);
	lz = lz < 0.f ? 0.f : (lz > (float)mesh.height ? (float)mesh.height : lz);

	int ix = (int)lx;
	int iz = (int)lz;
	if (ix >= mesh.width)  ix = mesh.width - 1;
	if (iz >= mesh.height) iz = mesh.height - 1;

	const float fx = lx - (float)ix;
	const float fz = lz - (float)iz;
	const int W = mesh.width + 1;
	const float h00 = mesh.heightData[iz * W + ix];
	const float h10 = mesh.heightData[iz * W + ix + 1];
	const float h01 = mesh.heightData[(iz + 1) * W + ix];
	const float h11 = mesh.heightData[(iz + 1) * W + ix + 1];

	const Vector3f p00 = { mesh.offsetX + (float)ix,     mesh.offsetY + h00, mesh.offsetZ + (float)iz };
	const Vector3f p10 = { mesh.offsetX + (float)ix + 1, mesh.offsetY + h10, mesh.offsetZ + (float)iz };
	const Vector3f p01 = { mesh.offsetX + (float)ix,     mesh.offsetY + h01, mesh.offsetZ + (float)iz + 1 };
	const Vector3f p11 = { mesh.offsetX + (float)ix + 1, mesh.offsetY + h11, mesh.offsetZ + (float)iz + 1 };

	Vector3f planePoint;
	Vector3f planeNormal;
	if (fx + fz <= 1.0f)
	{
		planePoint = p00;
		planeNormal = (p10 - p00).Cross(p01 - p00).GetNormalized();
	}
	else
	{
		planePoint = p11;
		planeNormal = (p01 - p11).Cross(p10 - p11).GetNormalized();
	}

	if (planeNormal.Y < 0.0f)
	{
		planeNormal *= -1.0f;
	}

	float planeY = planePoint.Y;
	if (std::fabs(planeNormal.Y) > 0.0001f)
	{
		planeY = planePoint.Y - (planeNormal.X * (worldX - planePoint.X) + planeNormal.Z * (worldZ - planePoint.Z)) / planeNormal.Y;
	}

	sample.height = planeY;
	sample.point = { worldX, planeY, worldZ };
	sample.normal = planeNormal;
	return sample;
}

float PhysicsWorld::GetTerrainHeight(float worldX, float worldZ) const
{
	return GetTerrainSample(worldX, worldZ).height;
}

bool PhysicsWorld::SweepSphereAgainstTerrain(const Vector3f& aStartCenter, const Vector3f& aEndCenter, float aRadius, float aTolerance, TerrainSweepHit& outHit) const
{
	outHit = {};
	if (!myTerrainMesh.heightData)
	{
		return false;
	}

	constexpr int kSweepSteps = 12;
	constexpr int kRefineIterations = 6;

	auto evaluateGap = [&](float aT, TerrainSweepHit& outEval)
	{
		const Vector3f center = aStartCenter + (aEndCenter - aStartCenter) * aT;
		const TerrainSample sample = GetTerrainSample(center.X, center.Z);
		const float signedDistance = (center - sample.point).Dot(sample.normal);
		outEval.hit = (signedDistance - aRadius) <= aTolerance;
		outEval.fraction = aT;
		outEval.gap = signedDistance - aRadius;
		outEval.center = center;
		outEval.point = sample.point;
		outEval.normal = sample.normal;
	};

	TerrainSweepHit previousEval = {};
	evaluateGap(0.0f, previousEval);
	if (previousEval.hit)
	{
		outHit = previousEval;
		return true;
	}

	for (int step = 1; step <= kSweepSteps; ++step)
	{
		const float t = static_cast<float>(step) / static_cast<float>(kSweepSteps);
		TerrainSweepHit currentEval = {};
		evaluateGap(t, currentEval);
		if (!currentEval.hit)
		{
			previousEval = currentEval;
			continue;
		}

		float low = previousEval.fraction;
		float high = currentEval.fraction;
		TerrainSweepHit highEval = currentEval;
		for (int i = 0; i < kRefineIterations; ++i)
		{
			const float mid = 0.5f * (low + high);
			TerrainSweepHit midEval = {};
			evaluateGap(mid, midEval);
			if (midEval.hit)
			{
				high = mid;
				highEval = midEval;
			}
			else
			{
				low = mid;
			}
		}

		outHit = highEval;
		return true;
	}

	return false;
}

void PhysicsWorld::ResolveTerrainCollisions()
{
	if (!myTerrainMesh.heightData) return;

	for (auto* body : myBodies)
	{
		if (body->myIsStatic || body->myOwner == nullptr) continue;
		if (body->myIgnoreTerrainCollision) continue;

		if (body->GetColliderType() == RigidbodyComponent::ColliderType::Sphere)
		{
			const Sphere sphere = body->GetSphere();
			const TerrainSample sample = GetTerrainSample(sphere.myCenter.X, sphere.myCenter.Z);
			const float signedDistance = (sphere.myCenter - sample.point).Dot(sample.normal);
			const float penetration = body->mySphereRadius - signedDistance;
			if (penetration > 0.0f)
			{
				const Vector3f contactPoint = sample.point;
				Vector3f pos = body->myOwner->GetLocation();
				pos += sample.normal * penetration;
				body->myOwner->SetLocation(pos);

				const Vector3f pointVelocity = body->GetVelocityAtPoint(contactPoint);
				const float normalVelocity = pointVelocity.Dot(sample.normal);
				float normalImpulseMag = 0.0f;
				if (normalVelocity < 0.0f)
				{
					const Vector3f r = contactPoint - body->GetCenterOfMassWorld();
					const Vector3f rn = r.Cross(sample.normal);
					const float invMass = 1.0f / body->myMass;
					const float denom = invMass + sample.normal.Dot(body->InverseInertiaMultiplyWorld(rn).Cross(r));
					if (std::fabs(denom) > 0.0001f)
					{
						normalImpulseMag = -(1.0f + body->myBounciness) * normalVelocity / denom;
						body->AddImpulseAtPoint(sample.normal * normalImpulseMag, contactPoint);
					}
				}
				body->RegisterContact(sample.normal, contactPoint, true, normalImpulseMag);

				const Vector3f postImpulseVelocity = body->GetVelocityAtPoint(contactPoint);
				Vector3f tangent = postImpulseVelocity - sample.normal * postImpulseVelocity.Dot(sample.normal);
				const float tangentLength = tangent.Length();
				if (tangentLength > 0.0001f)
				{
					tangent *= (1.0f / tangentLength);
					const Vector3f r = contactPoint - body->GetCenterOfMassWorld();
					const Vector3f rt = r.Cross(tangent);
					const float invMass = 1.0f / body->myMass;
					const float tangentDenom = invMass + tangent.Dot(body->InverseInertiaMultiplyWorld(rt).Cross(r));
					if (std::fabs(tangentDenom) > 0.0001f)
					{
						float tangentImpulseMag = -postImpulseVelocity.Dot(tangent) / tangentDenom;
						const float frictionLimit = (normalImpulseMag > 0.0f ? normalImpulseMag : body->myMass * 9.81f * 0.016f) * kTerrainFriction;
						if (tangentImpulseMag > frictionLimit) tangentImpulseMag = frictionLimit;
						if (tangentImpulseMag < -frictionLimit) tangentImpulseMag = -frictionLimit;
						body->AddImpulseAtPoint(tangent * tangentImpulseMag, contactPoint);
					}
				}
			}
			continue;
		}

		const OBB obb = body->GetOBB();
		const Vector3f axisX = obb.myAxis[0] * obb.myExtents.X;
		const Vector3f axisY = obb.myAxis[1] * obb.myExtents.Y;
		const Vector3f axisZ = obb.myAxis[2] * obb.myExtents.Z;

		const Vector3f corners[8] =
		{
			obb.myCenter - axisX - axisY - axisZ,
			obb.myCenter + axisX - axisY - axisZ,
			obb.myCenter - axisX + axisY - axisZ,
			obb.myCenter + axisX + axisY - axisZ,
			obb.myCenter - axisX - axisY + axisZ,
			obb.myCenter + axisX - axisY + axisZ,
			obb.myCenter - axisX + axisY + axisZ,
			obb.myCenter + axisX + axisY + axisZ,
		};

		int penetrationCount = 0;
		float maxPenetration = 0.0f;
		Vector3f averagedNormalSum = { 0.0f, 0.0f, 0.0f };
		Vector3f deepestNormal = { 0.0f, 1.0f, 0.0f };

		for (const Vector3f& corner : corners)
		{
			const TerrainSample sample = GetTerrainSample(corner.X, corner.Z);
			const float signedDistance = (corner - sample.point).Dot(sample.normal);
			const float penetration = -signedDistance;
			if (penetration > 0.0f)
			{
				++penetrationCount;
				averagedNormalSum += sample.normal;
				if (penetration > maxPenetration)
				{
					maxPenetration = penetration;
					deepestNormal = sample.normal;
				}
			}
		}

		if (penetrationCount > 0)
		{
			Vector3f contactNormal = averagedNormalSum * (1.0f / static_cast<float>(penetrationCount));
			const float normalLength = contactNormal.Length();
			if (normalLength > 0.0001f)
			{
				contactNormal *= (1.0f / normalLength);
			}
			else
			{
				contactNormal = deepestNormal;
			}

			Vector3f pos = body->myOwner->GetLocation();
			pos += contactNormal * maxPenetration;
			body->myOwner->SetLocation(pos);

			const Vector3f contactPoint = body->GetCenterOfMassWorld() - contactNormal * obb.myExtents.Y;
			const Vector3f pointVelocity = body->GetVelocityAtPoint(contactPoint);
			const float normalVelocity = pointVelocity.Dot(contactNormal);

			float normalImpulseMag = 0.0f;
			if (normalVelocity < 0.0f)
			{
				const Vector3f r = contactPoint - body->GetCenterOfMassWorld();
				const Vector3f rn = r.Cross(contactNormal);
				const float invMass = 1.0f / body->myMass;
				const float denom = invMass + contactNormal.Dot(body->InverseInertiaMultiplyWorld(rn).Cross(r));
				if (std::fabs(denom) > 0.0001f)
				{
					const float v_com_normal = body->myVelocity.Dot(contactNormal);
					const float v_capped = (v_com_normal >= 0.f) ? 0.f
						: (normalVelocity > v_com_normal ? normalVelocity : v_com_normal);

					if (v_capped < 0.f)
					{
						normalImpulseMag = -(1.0f + body->myBounciness) * v_capped / denom;
						body->myVelocity += contactNormal * (normalImpulseMag / body->myMass);

						constexpr float kOBBTerrainAngularFraction = 0.2f;
						body->myAngularVelocity += body->InverseInertiaMultiplyWorld(rn * normalImpulseMag) * kOBBTerrainAngularFraction;
					}
				}
			}

			body->RegisterContact(contactNormal, contactPoint, true, normalImpulseMag);
		}
	}
}