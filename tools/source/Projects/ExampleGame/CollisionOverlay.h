#pragma once

#include <Engine.h>
#include <Scene/Scene.h>
#include <GraphicsEngine/Camera/Camera.h>
#include <Physics/RigidbodyComponent.h>
#include <Input/InputManager.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "CarController.h"

namespace CollisionOverlay
{
	inline bool IsEnabled()
	{
		static bool enabled = []()
		{
			char* envValue = nullptr;
			size_t envLen = 0;
			const errno_t err = _dupenv_s(&envValue, &envLen, "TE_AUTOTEST_SHOW_COLLISION");
			const bool enabledFromEnv = (err == 0 && envValue && envValue[0] != '\0' && envValue[0] != '0');
			if (envValue) free(envValue);
			return enabledFromEnv;
		}();
		if (InputManager::Get().IsKeyPressed('P'))
		{
			enabled = !enabled;
		}
		return enabled;
	}

	inline bool ProjectWorldToScreen(const Vector3f& worldPos, const Camera* camera, const ImVec2& viewportMin, const ImVec2& viewportSize, ImVec2& outScreen)
	{
		if (!camera || viewportSize.x <= 1.0f || viewportSize.y <= 1.0f)
		{
			return false;
		}

		const Matrix4x4f view = Matrix4x4f::GetFastInverse(camera->GetTransform().GetMatrix());
		const Matrix4x4f worldToClip = view * camera->get_projection();
		const Vector4f clip = Vector4f(worldPos.X, worldPos.Y, worldPos.Z, 1.0f) * worldToClip;
		if (clip.W <= 0.001f)
		{
			return false;
		}

		const float invW = 1.0f / clip.W;
		const float ndcX = clip.X * invW;
		const float ndcY = clip.Y * invW;
		const float ndcZ = clip.Z * invW;
		if (ndcZ < -0.25f || ndcZ > 1.25f)
		{
			return false;
		}

		outScreen.x = viewportMin.x + (ndcX * 0.5f + 0.5f) * viewportSize.x;
		outScreen.y = viewportMin.y + (-ndcY * 0.5f + 0.5f) * viewportSize.y;
		return true;
	}

	inline void DrawSegment(ImDrawList* draw, const Vector3f& a, const Vector3f& b, const Camera* camera, const ImVec2& viewportMin, const ImVec2& viewportSize, ImU32 color, float thickness)
	{
		ImVec2 p0;
		ImVec2 p1;
		if (ProjectWorldToScreen(a, camera, viewportMin, viewportSize, p0) &&
			ProjectWorldToScreen(b, camera, viewportMin, viewportSize, p1))
		{
			draw->AddLine(p0, p1, color, thickness);
		}
	}

	inline void DrawAABB(ImDrawList* draw, const AABB& aabb, const Camera* camera, const ImVec2& viewportMin, const ImVec2& viewportSize, ImU32 color, float thickness)
	{
		const Vector3f corners[8] =
		{
			{ aabb.myMin.X, aabb.myMin.Y, aabb.myMin.Z },
			{ aabb.myMax.X, aabb.myMin.Y, aabb.myMin.Z },
			{ aabb.myMax.X, aabb.myMax.Y, aabb.myMin.Z },
			{ aabb.myMin.X, aabb.myMax.Y, aabb.myMin.Z },
			{ aabb.myMin.X, aabb.myMin.Y, aabb.myMax.Z },
			{ aabb.myMax.X, aabb.myMin.Y, aabb.myMax.Z },
			{ aabb.myMax.X, aabb.myMax.Y, aabb.myMax.Z },
			{ aabb.myMin.X, aabb.myMax.Y, aabb.myMax.Z },
		};

		static const int edges[12][2] =
		{
			{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
			{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
		};

		for (const auto& edge : edges)
		{
			DrawSegment(draw, corners[edge[0]], corners[edge[1]], camera, viewportMin, viewportSize, color, thickness);
		}
	}

	inline void DrawOBB(ImDrawList* draw, const OBB& obb, const Camera* camera, const ImVec2& viewportMin, const ImVec2& viewportSize, ImU32 color, float thickness)
	{
		const Vector3f axisX = obb.myAxis[0] * obb.myExtents.X;
		const Vector3f axisY = obb.myAxis[1] * obb.myExtents.Y;
		const Vector3f axisZ = obb.myAxis[2] * obb.myExtents.Z;
		const Vector3f corners[8] =
		{
			obb.myCenter - axisX - axisY - axisZ,
			obb.myCenter + axisX - axisY - axisZ,
			obb.myCenter + axisX + axisY - axisZ,
			obb.myCenter - axisX + axisY - axisZ,
			obb.myCenter - axisX - axisY + axisZ,
			obb.myCenter + axisX - axisY + axisZ,
			obb.myCenter + axisX + axisY + axisZ,
			obb.myCenter - axisX + axisY + axisZ,
		};

		static const int edges[12][2] =
		{
			{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
			{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
		};

		for (const auto& edge : edges)
		{
			DrawSegment(draw, corners[edge[0]], corners[edge[1]], camera, viewportMin, viewportSize, color, thickness);
		}
	}

	inline void DrawSphereRings(ImDrawList* draw, const Vector3f& center, float radius, const Camera* camera, const ImVec2& viewportMin, const ImVec2& viewportSize, ImU32 color, float thickness)
	{
		constexpr int segments = 18;
		constexpr float tau = 6.28318530717958647692f;

		const auto drawRing = [&](const Vector3f& axisA, const Vector3f& axisB)
		{
			for (int i = 0; i < segments; ++i)
			{
				const float t0 = (static_cast<float>(i) / static_cast<float>(segments)) * tau;
				const float t1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * tau;
				const Vector3f p0 = center + axisA * (std::cosf(t0) * radius) + axisB * (std::sinf(t0) * radius);
				const Vector3f p1 = center + axisA * (std::cosf(t1) * radius) + axisB * (std::sinf(t1) * radius);
				DrawSegment(draw, p0, p1, camera, viewportMin, viewportSize, color, thickness);
			}
		};

		drawRing({ 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
		drawRing({ 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });
		drawRing({ 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });
	}

	inline void DrawCarCollisionOverlay(const Scene* scene, const Camera* camera, const ImVec2& viewportMin, const ImVec2& viewportSize)
	{
		if (!scene || !camera || !IsEnabled())
		{
			return;
		}

		ImDrawList* draw = ImGui::GetForegroundDrawList();
		if (!draw)
		{
			return;
		}

		const ImU32 bodyColor = IM_COL32(80, 255, 120, 255);
		const ImU32 wheelGroundedColor = IM_COL32(80, 255, 120, 255);
		const ImU32 wheelAirColor = IM_COL32(80, 255, 120, 200);
		const ImU32 supportColor = IM_COL32(80, 220, 255, 255);
		const ImU32 contactColor = IM_COL32(255, 255, 255, 255);

		for (ModelInstance* model : scene->SceneModels())
		{
			if (!model)
			{
				continue;
			}

			CarController* car = model->GetComponent<CarController>();
			RigidbodyComponent* rigidbody = model->GetComponent<RigidbodyComponent>();
			if (!car || !rigidbody)
			{
				continue;
			}

			DrawOBB(draw, rigidbody->GetOBB(), camera, viewportMin, viewportSize, bodyColor, 2.0f);

			for (int wheelIndex = 0; wheelIndex < 4; ++wheelIndex)
			{
				const float radius = (std::max)(car->GetWheelRadius(), 0.05f);
				if (car->HasWheel(wheelIndex))
				{
					const CarController::WheelContact& contact = car->GetWheelContact(wheelIndex);
					const ImU32 wheelColor = contact.grounded ? wheelGroundedColor : wheelAirColor;

					DrawSphereRings(draw, contact.worldPos, radius, camera, viewportMin, viewportSize, wheelColor, 1.6f);
					DrawSegment(draw, contact.worldPos, contact.contactPoint, camera, viewportMin, viewportSize, supportColor, 1.5f);

					const Vector3f normalTip = contact.contactPoint + contact.normal * (std::max)(radius * 0.65f, 0.2f);
					DrawSegment(draw, contact.contactPoint, normalTip, camera, viewportMin, viewportSize, contactColor, 1.3f);
				}
			}
		}
	}
}
