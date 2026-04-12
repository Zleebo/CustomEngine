#pragma once
#include <Components/Component.h>
#include <Math/Vector.h>
#include <vector>
#include <string>

struct Particle
{
	Vector3f myPosition;
	Vector3f myVelocity;
	Vector4f myColour{ 1, 1, 1, 1 };
	float myLifetime = 0.0f;
	float myMaxLifetime = 2.0f;
	float mySize = 0.1f;
	bool myAlive = false;
};

class ParticleEmitter : public Component
{
public:
	void Update(float aDeltaTime) override;

	const std::vector<Particle>& GetParticles() const { return myParticles; }

	int myMaxParticles = 100;
	float myEmitRate = 10.0f;
	Vector3f myEmitDirection{ 0, 1, 0 };
	float mySpread = 0.5f;
	float myParticleLifetime = 2.0f;
	float myParticleSpeed = 3.0f;
	float myParticleSize = 0.1f;
	Vector4f myColour{ 1, 1, 1, 1 };
	bool myLooping = true;

private:
	void EmitParticle();
	float RandomFloat(float aMin, float aMax) const;

	std::vector<Particle> myParticles;
	float myAccumulator = 0.0f;
};
