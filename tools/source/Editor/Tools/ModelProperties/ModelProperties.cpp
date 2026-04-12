#include "ModelProperties.h"

#include <Engine.h>
#include <GraphicsEngine/Texture/Texture2D.h>

#include <imgui.h>

#include <CommandManager.h>
#include <Commands/TransformCommand.h>
#include <Commands/ChangeTextureCommand.h>
#include <Commands/RenameCommand.h>
#include <Selection.h>
#include <vector>

#include <filesystem>
#include <typeinfo>

ModelProperties::ModelProperties(std::vector<ModelInstance*> &aModelList) : myModelList(aModelList)
{

}

void ModelProperties::Draw()
{

	ImGui::Begin("Properties");
	{
		int id = 0;
		for (auto mi : Selection::GetSelection())
		{
			if (mi != nullptr)
			{
				ImGui::PushID(id++);
				if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
				{
					Transform t = mi->GetTransform();
					auto pos = t.GetPosition();

					char buff[128] = {};
					strncpy_s(buff, mi->GetName(), sizeof(buff) - 1);
					if (ImGui::IsItemActivated())
					{
						myStartName = mi->GetName();
					}
					if (ImGui::InputText("Name", buff, sizeof(buff)))
					{
						mi->SetName(buff);
					}
					if (ImGui::IsItemDeactivatedAfterEdit())
					{
						if (myStartName != buff)
						{
							CommandManager::DoCommand(std::make_unique<RenameCommand>(mi, myStartName.c_str(), buff));
						}
					}

					if (ImGui::DragFloat3("Translation", pos.data(), 0.01f))
					{
						mi->SetLocation(pos);
					}
					if (ImGui::IsItemActivated())
					{
						myStartTransform.reset(new Transform(mi->GetTransform()));
					}
					else if (ImGui::IsItemDeactivatedAfterEdit())
					{
						std::vector<Matrix4x4f> from;
						from.push_back(Matrix4x4f::CreateTranslationMatrix(myStartTransform->GetPosition()));
						std::vector<Matrix4x4f> to;
						to.push_back(Matrix4x4f::CreateTranslationMatrix(mi->GetLocation()));

						CommandManager::DoCommand(std::make_unique<TransformCommand>(from, to, myModelList));
					}

					auto rot = t.GetRotation();
					Vector3f rotDeg = { rot.X * Math::RadToDeg, rot.Y * Math::RadToDeg, rot.Z * Math::RadToDeg };
					if (ImGui::DragFloat3("Rotation", rotDeg.data(), 0.5f))
					{
						mi->SetRotation({ rotDeg.X * Math::DegToRad, rotDeg.Y * Math::DegToRad, rotDeg.Z * Math::DegToRad });
					}
					if (ImGui::IsItemActivated())
					{
						myStartTransform.reset(new Transform(mi->GetTransform()));
					}
					else if (ImGui::IsItemDeactivatedAfterEdit())
					{
						std::vector<Matrix4x4f> from;
						from.push_back(myStartTransform->GetMatrix());
						std::vector<Matrix4x4f> to;
						to.push_back(mi->GetTransform().GetMatrix());
						CommandManager::DoCommand(std::make_unique<TransformCommand>(from, to, myModelList));
					}

					auto scale = t.GetScale();
					if (ImGui::DragFloat3("Scale", scale.data(), 0.01f))
					{
						mi->SetScale(scale);
					}
					if (ImGui::IsItemActivated())
					{
						myStartTransform.reset(new Transform(mi->GetTransform()));
					}
					else if (ImGui::IsItemDeactivatedAfterEdit())
					{
						std::vector<Matrix4x4f> from;
						from.push_back(myStartTransform->GetMatrix());
						std::vector<Matrix4x4f> to;
						to.push_back(mi->GetTransform().GetMatrix());
						CommandManager::DoCommand(std::make_unique<TransformCommand>(from, to, myModelList));
					}

					ImGui::ImageButton((ImTextureID)mi->GetTextureResource(), ImVec2(32, 32));
					if (ImGui::BeginDragDropTarget())
					{
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(".dds"))
						{
							const char* path = (const char *)payload->Data;
							CommandManager::DoCommand(std::make_unique<ChangeTextureCommand>(mi, mi->GetTexture(), path));
						}
						ImGui::EndDragDropTarget();
					}

					if (ImGui::BeginCombo("##combo", "foobar"))
					{
						std::filesystem::path texture_path = Settings::paths::game_assets + "/Textures";
						for (auto const& item : std::filesystem::directory_iterator(texture_path))
						{
							std::string filename = item.path().filename().string();
							bool is_selected = (filename.c_str() == mi->GetModel()->GetPath());
							if (ImGui::Selectable(filename.c_str(), is_selected))
							{
								CommandManager::DoCommand(
									std::make_unique<ChangeTextureCommand>(
										mi, mi->GetTexture(), item.path().string().c_str()
									)
								);
							}
							if (is_selected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
				}
				if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
				{
					const auto& components = mi->GetAllComponents();
					int removeIndex = -1;
					for (int ci = 0; ci < (int)components.size(); ci++)
					{
						ImGui::PushID(ci);
						const char* rawName = typeid(*components[ci].get()).name();
						const char* displayName = rawName;
						if (strncmp(rawName, "class ", 6) == 0) displayName = rawName + 6;
						else if (strncmp(rawName, "struct ", 7) == 0) displayName = rawName + 7;

						bool open = ImGui::TreeNodeEx(displayName, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);
						ImGui::SameLine(ImGui::GetContentRegionAvail().x - 14.0f);
						if (ImGui::SmallButton("X")) removeIndex = ci;

						if (open)
						{
							const auto& props = components[ci]->myExposedProperties;
							if (!props.empty())
							{
								for (const auto& prop : props)
								{
									switch (prop.type)
									{
										case ExposedProperty::Type::Float:
											ImGui::DragFloat(prop.label.c_str(), (float*)prop.ptr, prop.speed, prop.min, prop.max);
											break;
										case ExposedProperty::Type::Int:
											ImGui::DragInt(prop.label.c_str(), (int*)prop.ptr);
											break;
										case ExposedProperty::Type::Bool:
											ImGui::Checkbox(prop.label.c_str(), (bool*)prop.ptr);
											break;
										case ExposedProperty::Type::Vec3:
											ImGui::DragFloat3(prop.label.c_str(), (float*)prop.ptr, prop.speed);
											break;
									}
								}
							} else
							{
								components[ci]->DrawInspector();
							}
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					if (removeIndex >= 0) mi->RemoveComponent(removeIndex);

					ImGui::Spacing();
					auto types = ComponentFactory::Get().GetRegisteredTypes();
					if (!types.empty())
					{
						static int selectedType = 0;
						if (selectedType >= (int)types.size()) selectedType = 0;
						ImGui::SetNextItemWidth(-80.0f);
						if (ImGui::BeginCombo("##comptype", types[selectedType].c_str()))
						{
							for (int ti = 0; ti < (int)types.size(); ti++)
							{
								bool sel = (ti == selectedType);
								if (ImGui::Selectable(types[ti].c_str(), sel)) selectedType = ti;
								if (sel) ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
						ImGui::SameLine();
						if (ImGui::Button("Add"))
						{
							mi->AddComponent(ComponentFactory::Get().Create(types[selectedType]));
						}
					} else
					{
						ImGui::TextDisabled("(no types registered)");
					}
				}

				ImGui::PopID();
			}
		}
	}
	ImGui::End();
}
