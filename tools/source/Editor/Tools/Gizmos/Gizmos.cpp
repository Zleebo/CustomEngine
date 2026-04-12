#include "Gizmos.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <Engine.h>
#include <GraphicsEngine/Camera/Camera.h>

#include <CommandManager.h>
#include <TransformCommand.h>
#include <ChangeTextureCommand.h>
#include <Selection.h>


Gizmos::Gizmos(std::shared_ptr<Scene> aScene) : myScene(aScene)
{
}

void Gizmos::Draw()
{
    if (ImGui::Button("t") || ImGui::IsKeyPressed('Q'))
    {
        myGizmoType = ImGuizmo::OPERATION::TRANSLATE;
    }
    ImGui::SameLine();
    if (ImGui::Button("r") || ImGui::IsKeyPressed('W'))
    {
        myGizmoType = ImGuizmo::OPERATION::ROTATE;
    }
    ImGui::SameLine();
    if (ImGui::Button("s") || ImGui::IsKeyPressed('E'))
    {
        myGizmoType = ImGuizmo::OPERATION::SCALE;
    }
    ImGui::SameLine();
    if (ImGui::Button("Deselect") || ImGui::IsKeyPressed(27))
    {
        myGizmoType = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Undo") || (ImGui::GetIO().KeyShift == false && ImGui::IsKeyPressed('Z') && ImGui::GetIO().KeyCtrl))
    {
        CommandManager::Undo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Redo") || (ImGui::IsKeyPressed('Z') && ImGui::GetIO().KeyCtrl) && ImGui::GetIO().KeyShift)
    {
        CommandManager::Redo();
    }
    float v = mySnap[0];
    if (ImGui::SliderFloat("Snapping", &v, 0.0f, 5.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp))
    {
        mySnap[0] = mySnap[1] = mySnap[2] = v;
    }
}

void Gizmos::DrawGizmos()
{

    if (Selection::GetSelection().size() <= 0) { return; }

    std::vector<ModelInstance*> selection = Selection::GetSelection();

    ImGui::SetItemDefaultFocus();

    auto io = ImGui::GetIO();
    {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

        const Camera* camera = myScene->GetActiveCamera();
        Matrix4x4f& view = Matrix4x4f::GetFastInverse(camera->GetTransform().GetMatrix());
        Matrix4x4f& proj = camera->get_projection();

        //////////////////////////////////////////////////////////////
        // "Fake" drop on object, since we don't have mouse picking ^^
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(".dds"))
            {
                const char* path = (const char*)payload->Data;
                for (ModelInstance* mi : Selection::GetSelection())
                {
                    CommandManager::DoCommand(std::make_unique<ChangeTextureCommand>(mi, mi->GetTexture(), path));
                }
            }

            ImGui::EndDragDropTarget();
        }

        if (ImGuizmo::IsUsing() == false)
        {
            myTransform = Selection::GetCentroid();
        }

        ImGuizmo::Manipulate(
            view.GetDataPtr(),
            proj.GetDataPtr(),
            (ImGuizmo::OPERATION)myGizmoType,
            ImGuizmo::MODE::LOCAL,
            myTransform.GetDataPtr(),
            nullptr,
            mySnap
        );

        static bool in_use = false;
        if (ImGuizmo::IsOver() && io.MouseClicked[0]) {                        
            myStartTransforms.reserve(selection.size());
            myStartTransforms.clear();
            mySceneModels.clear();

            for (const auto &s : selection)
            {
                myStartTransforms.push_back(s->GetTransform().GetMatrix());
                mySceneModels.push_back(s);

                auto childTree = Selection::GetChildrenInBranch(s);
                for (const auto& c : childTree)
                {
                    myStartTransforms.push_back(c->GetTransform().GetMatrix());
                    mySceneModels.push_back(c);
                }

            }
            //memcpy(myEndTransforms.data(), myStartTransforms.data(), sizeof(Matrix4x4f) * myStartTransforms.size());
            myEndTransforms = myStartTransforms;
            myStartTransform = myTransform;
            myStartTransformInverse = Matrix4x4f::GetFastInverse(myStartTransform);

            in_use = true;
        }
        if (in_use == true && io.MouseReleased[0])
        {
            in_use = false;
            CommandManager::DoCommand(std::make_unique<TransformCommand>(myStartTransforms, myEndTransforms, mySceneModels));
        }
        
        if (ImGuizmo::IsUsing())
        {
            for (size_t i=0; i < myStartTransforms.size(); ++i)
            {
                myEndTransforms[i] = myStartTransforms[i] * myStartTransformInverse * myTransform;

                Vector3f p, r, s;
                myEndTransforms[i].DecomposeMatrix(p, r, s);

                mySceneModels[i]->SetRotation(r);
                mySceneModels[i]->SetLocation(p);
                mySceneModels[i]->SetScale(s);
            }
        }
    }
}
