#include <stdafx.h>
#include "Transform.h"

void Transform::SetPosition(Vector3f somePosition)
{
	myPosition = somePosition;
}

void Transform::SetRotation(Rotator someRotation)
{
	myRotation.Roll = fmod(someRotation.Roll, 360.0f);
	if (myRotation.Roll > 180)
		myRotation.Roll -= 360;
	else if (myRotation.Roll < -180)
		myRotation.Roll += 360;

	myRotation.Pitch = fmod(someRotation.Pitch, 360.0f);
	if (myRotation.Pitch > 180)
		myRotation.Pitch -= 360;
	else if (myRotation.Pitch < -180)
		myRotation.Pitch += 360;

	myRotation.Yaw = fmod(someRotation.Yaw, 360.0f);
	if (myRotation.Yaw > 180)
		myRotation.Yaw -= 360;
	else if (myRotation.Yaw < -180)
		myRotation.Yaw += 360;
}

void Transform::SetScale(Vector3f someScale)
{
	myScale = someScale;
}

void Transform::AddRotation(Rotator someRotation)
{
	SetRotation(myRotation + someRotation);
}

void Transform::Translate(const Vector3f& aDelta)
{
	SetPosition(myPosition + aDelta);
}
