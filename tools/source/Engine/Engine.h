#pragma once

#include "Core/EngineSettings.h"

#include "WindowManagement/Win32Window.h"
#include "GraphicsEngine/GraphicsFrameworks/DX11/DX11Framework.h"
#include "GraphicsEngine/GraphicsEngine.h"

#include "Application/Application.h"
#include "Input/InputManager.h"
#include "GraphicsEngine/Lights/LightManager.h"
#include "Components/Component.h"
#include "Components/ScriptComponent.h"
#include "Components/ComponentFactory.h"
#include "Physics/PhysicsWorld.h"
#include "Physics/Collider.h"
#include "Physics/RigidbodyComponent.h"
#include "Audio/AudioEngine.h"
#include "Audio/AudioSourceComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialRegistry.h"
#include "Assets/AssetRegistry.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleRenderer.h"
#include "GraphicsEngine/PostProcess/PostProcessor.h"
#include "UI/UIRenderer.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimationClip.h"
#include "Animation/AnimatorComponent.h"
#include "Debug/Profiler.h"

#include <Math/Math.hpp>
#include <Math/Matrix.h>
#include <Math/Quaternion.h>
#include <Math/Transform.h>
#include <Math/Vector.h>
#include <Math/Vertex.h>
#include <Math/Matrix4x4.h>

#include <Models/ModelInstance.h>
#include <Models/Model.h>

#include <Core/EngineCore.h>

template <typename T>
using ref = std::shared_ptr<T>;
template <typename T>
using local = std::unique_ptr<T>;