#pragma once
#include <Math/Vector.h>

struct AABB
{
	Vector3f myMin;
	Vector3f myMax;

	bool Intersects(const AABB& aOther) const
	{
		return (myMin.X <= aOther.myMax.X && myMax.X >= aOther.myMin.X) &&
		       (myMin.Y <= aOther.myMax.Y && myMax.Y >= aOther.myMin.Y) &&
		       (myMin.Z <= aOther.myMax.Z && myMax.Z >= aOther.myMin.Z);
	}

	Vector3f GetCenter() const
	{
		return { (myMin.X + myMax.X) * 0.5f, (myMin.Y + myMax.Y) * 0.5f, (myMin.Z + myMax.Z) * 0.5f };
	}

	Vector3f GetExtents() const
	{
		return { (myMax.X - myMin.X) * 0.5f, (myMax.Y - myMin.Y) * 0.5f, (myMax.Z - myMin.Z) * 0.5f };
	}
};

struct OBB
{
	Vector3f myCenter;
	Vector3f myAxis[3] = {
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f }
	};
	Vector3f myExtents = { 0.5f, 0.5f, 0.5f };
};

struct Sphere
{
	Vector3f myCenter;
	float myRadius = 1.0f;

	bool Intersects(const Sphere& aOther) const
	{
		float dx = myCenter.X - aOther.myCenter.X;
		float dy = myCenter.Y - aOther.myCenter.Y;
		float dz = myCenter.Z - aOther.myCenter.Z;
		float distSq = dx*dx + dy*dy + dz*dz;
		float rSum = myRadius + aOther.myRadius;
		return distSq <= rSum * rSum;
	}
};
