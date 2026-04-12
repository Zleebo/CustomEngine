#include <stdafx.h>
#include "ParticleEmitter.h"
#include <Models/ModelInstance.h>
#include <cstdlib>

void ParticleEmitter::Update(float aDeltaTime)
{
	if (!myLooping) return;

	myAccumulator += aDeltaTime;
	float interval = 1.0f / myEmitRate;

	while (myAccumulator >= interval)
	{
		myAccumulator -= interval;
		EmitParticle();
	}

	for (auto& p : myParticles)
	{
		if (!p.myAlive) continue;

		p.myLifetime += aDeltaTime;
		if (p.myLifetime >= p.myMaxLifetime)
		{
			p.myAlive = false;
			continue;
		}

		p.myPosition = p.myPosition + (p.myVelocity * aDeltaTime);

		float t = 1.0f - (p.myLifetime / p.myMaxLifetime);
		p.myColour.W = t;
	}
}

void ParticleEmitter::EmitParticle()
{
	Particle* slot = nullptr;
	for (auto& p : myParticles)
	{
		if (!p.myAlive) { slot = &p; break; }
	}

	if (!slot)
	{
		if ((int)myParticles.size() < myMaxParticles)
		{
			myParticles.emplace_back();
			slot = &myParticles.back();
		}
		else
		{
			return;
		}
	}

	Vector3f origin{};
	if (myOwner) origin = myOwner->GetLocation();

	slot->myAlive = true;
	slot->myLifetime = 0.0f;
	slot->myMaxLifetime = myParticleLifetime;
	slot->myPosition = origin;
	slot->mySize = myParticleSize;
	slot->myColour = myColour;

	Vector3f dir = myEmitDirection;
	dir.X += RandomFloat(-mySpread, mySpread);
	dir.Y += RandomFloat(-mySpread, mySpread);
	dir.Z += RandomFloat(-mySpread, mySpread);
	slot->myVelocity = dir * myParticleSpeed;
}

float ParticleEmitter::RandomFloat(float aMin, float aMax) const
{
	return aMin + (static_cast<float>(rand()) / RAND_MAX) * (aMax - aMin);
}
