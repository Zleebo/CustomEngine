#include "SceneHierarchy.h"

#include <Engine.h>
#include <Models/ModelInstance.h>

#include <imgui.h>

#include "Selection.h"

#include <CommandManager.h>
#include <Commands/DeleteCommand.h>
#include <Commands/SetParentCommand.h>
#include <string>

SceneHierarchy::SceneHierarchy(std::shared_ptr<Scene> aScene) : myScene(aScene)
{
    myScene->onBeforeClear = []{ Selection::ClearSelection(); };
    // Keep whatever scene the launcher already loaded; only fall back to Editor.json
    // when nothing is present.
    if (myScene->SceneModels().empty())
    {
        myScene->Load("Editor.json");
    }
}

void SceneHierarchy::DrawBranch(const std::vector<ModelInstance*>& aBranch, ModelInstance* const theParent)
{
    for (ModelInstance* mi : aBranch)
    {
        if (!mi || mi->GetParent() != theParent || (ImGui::GetDragDropPayload() && Selection::Contains(mi)))
        {
            continue;
        }

        ImGui::PushID(mi);

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (Selection::Contains(mi))
        {
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!mi->GetChildren().empty())
        {
            nodeFlags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
        }
        else
        {
            nodeFlags |= ImGuiTreeNodeFlags_Leaf;
        }

        bool nodeOpen = ImGui::TreeNodeEx(mi->GetName(), nodeFlags);

        if (ImGui::GetIO().MouseReleased[0] && ImGui::IsItemHovered())
        {
            if (ImGui::GetIO().KeyCtrl)
            {
                if (Selection::Contains(mi)) Selection::RemoveFromSelection(mi);
                else Selection::AddToSelection(mi);
            }
            else
            {
                Selection::ClearSelection();
                Selection::AddToSelection(mi);
            }
        }

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete"))
            {
                std::vector<ModelInstance*> toDelete = Selection::Contains(mi)
                    ? Selection::GetSelection()
                    : std::vector<ModelInstance*>{ mi };
                CommandManager::DoCommand(std::make_shared<DeleteCommand>(myScene, toDelete));
                Selection::ClearSelection();
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ModelInstance"))
            {
                (void)payload;
                for (auto* selected : Selection::GetSelection())
                {
                    if (!selected || selected == mi || selected->GetParent() == mi || mi->IsDescendantOf(selected))
                    {
                        continue;
                    }
                    CommandManager::DoCommand(std::make_shared<SetParentCommand>(selected, selected->GetParent(), mi));
                }
                Selection::ClearSelection();
                Selection::AddToSelection(mi);
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("ModelInstance", nullptr, 0);

            if (!Selection::Contains(mi) && !(ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift))
            {
                Selection::ClearSelection();
            }
            Selection::AddToSelection(mi);
            ImGui::TextUnformatted(mi->GetName());
            ImGui::EndDragDropSource();
        }

        if (nodeOpen)
        {
            DrawBranch(mi->GetChildren(), mi);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

void SceneHierarchy::Draw()
{
    {
        std::vector<ModelInstance*> sceneModels = myScene->SceneModels();
        ImGui::Begin("Scene List");
        {
            if (ImGui::Button("New ModelInstance"))
            {
                auto m = myScene->CreateModelInstance("cube");
                Selection::ClearSelection();
                Selection::AddToSelection(m);
            }

            if (ImGui::BeginListBox("##SceneList", ImGui::GetContentRegionAvail()))
            {
                DrawBranch(sceneModels);

                // Drop to root to unparent.
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ModelInstance"))
                    {
                        (void)payload;
                        for (auto* selected : Selection::GetSelection())
                        {
                            if (selected && selected->GetParent() != nullptr)
                            {
                                CommandManager::DoCommand(std::make_shared<SetParentCommand>(selected, selected->GetParent(), nullptr));
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::EndListBox();
            }

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                && !Selection::GetSelection().empty()
                && ImGui::IsKeyPressed(ImGuiKey_Delete))
            {
                CommandManager::DoCommand(
                    std::make_shared<DeleteCommand>(myScene, Selection::GetSelection()));
                Selection::ClearSelection();
            }

        } ImGui::End();
    }
}
