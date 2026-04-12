#include "AssetBrowser.h"

#include <Engine.h>
#include <Core/EngineSettings.h>
#include <GraphicsEngine/Texture/Texture2D.h>
#include <Scene/Scene.h>
#include <Scene/Prefab.h>
#include <Selection.h>

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>

#include <imgui.h>

// ---------------------------------------------------------------------------
AssetBrowser::AssetBrowser(std::shared_ptr<Scene> aScene)
	: myScene(aScene)
{
	myRootPath    = Settings::paths::game_assets;
	myCurrentPath = myRootPath;

	// Default component output: <project_root>/tools/source/  (bin is one level down)
	// Try known ExampleGame source directories; fall back to the source root.
	fs::path sourceRoot = fs::current_path().parent_path() / "tools" / "source";
	for (const char* sub : { "Projects/ExampleGame", "ExampleGame" })
	{
		fs::path candidate = sourceRoot / sub;
		if (fs::exists(candidate)) { sourceRoot = candidate; break; }
	}
	strncpy_s(myComponentOutputPath, sizeof(myComponentOutputPath),
	          sourceRoot.string().c_str(), _TRUNCATE);
}

// ---------------------------------------------------------------------------
// Left panel: recursive folder tree
// ---------------------------------------------------------------------------
void AssetBrowser::DrawFolderTree(const fs::path& aPath)
{
	try
	{
		for (const auto& entry : fs::directory_iterator(aPath))
		{
			if (!entry.is_directory()) continue;

			bool hasSubDirs = false;
			try
			{
				for (const auto& child : fs::directory_iterator(entry.path()))
					if (child.is_directory()) { hasSubDirs = true; break; }
			} catch (...) {}

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth
			                        | ImGuiTreeNodeFlags_OpenOnArrow;
			if (!hasSubDirs)                   flags |= ImGuiTreeNodeFlags_Leaf;
			if (entry.path() == myCurrentPath) flags |= ImGuiTreeNodeFlags_Selected;

			bool open = ImGui::TreeNodeEx(entry.path().filename().string().c_str(), flags);

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				myCurrentPath = entry.path();

			if (open)
			{
				DrawFolderTree(entry.path());
				ImGui::TreePop();
			}
		}
	} catch (...) {}
}

// ---------------------------------------------------------------------------
// Right panel: thumbnail grid (folders then files)
// ---------------------------------------------------------------------------
void AssetBrowser::DrawAssetGrid()
{
	if (!fs::exists(myCurrentPath))
	{
		ImGui::TextDisabled("(folder not found)");
		return;
	}

	const float thumbSize = myThumbnailSize;
	const float cellWidth = thumbSize + 16.0f;
	const int   colCount  = (std::max)(1, (int)(ImGui::GetContentRegionAvail().x / cellWidth));

	// ---- Right-click on blank background ----
	if (ImGui::BeginPopupContextWindow("##bgctx",
		ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::BeginMenu("Create"))
		{
			if (ImGui::MenuItem("Empty ModelInstance"))
			{
				auto* mi = myScene->CreateModelInstance("cube");
				Selection::ClearSelection();
				Selection::AddToSelection(mi);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("New C++ Component..."))
				myOpenCreateComponentModal = true;
			const auto& sel = Selection::GetSelection();
			if (!sel.empty())
			{
				ImGui::Separator();
				if (ImGui::BeginMenu("Add Component to Selected"))
				{
					for (const auto& typeName : ComponentFactory::Get().GetRegisteredTypes())
					{
						if (ImGui::MenuItem(typeName.c_str()))
							for (auto* obj : sel)
								obj->AddComponent(ComponentFactory::Get().Create(typeName));
					}
					if (ComponentFactory::Get().GetRegisteredTypes().empty())
						ImGui::TextDisabled("(no types registered)");
					ImGui::EndMenu();
				}
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("New Prefab..."))
			myOpenCreatePrefabModal = true;
		if (ImGui::MenuItem("New Folder"))
		{
			fs::path dir = myCurrentPath / "NewFolder";
			int n = 1;
			while (fs::exists(dir)) dir = myCurrentPath / ("NewFolder" + std::to_string(n++));
			fs::create_directory(dir);
		}
		ImGui::EndPopup();
	}

	ImGui::Columns(colCount, nullptr, false);

	// ---- Helpers ----

	// Blue selection highlight drawn behind the item thumbnail
	auto drawSelectionHighlight = [&]()
	{
		ImVec2 p = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddRectFilled(
			{p.x - 2.0f, p.y - 2.0f},
			{p.x + thumbSize + 2.0f, p.y + thumbSize + 20.0f},
			IM_COL32(80, 130, 200, 80));
	};

	// Inline rename InputText or plain truncated label
	auto drawLabel = [&](const fs::path& itemPath, const std::string& name)
	{
		if (itemPath == myRenamingPath)
		{
			if (myRenameJustOpened)
			{
				ImGui::SetKeyboardFocusHere();
				myRenameJustOpened = false;
			}
			ImGui::SetNextItemWidth(thumbSize);
			bool confirmed = ImGui::InputText("##ren", myRenameBuffer, sizeof(myRenameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			bool cancelled = ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape);
			if (cancelled)
			{
				myRenamingPath.clear();
			} else if (confirmed || ImGui::IsItemDeactivated())
			{
				std::string newName = myRenameBuffer;
				if (!newName.empty() && newName != name)
				{
					fs::path newPath = itemPath.parent_path() / newName;
					std::error_code ec;
					if (!fs::exists(newPath))
					{
						fs::rename(itemPath, newPath, ec);
						if (!ec && mySelectedPath == itemPath)
							mySelectedPath = newPath;
					}
				}
				myRenamingPath.clear();
			}
		} else
		{
			std::string lbl = name.size() > 13 ? name.substr(0, 12) + "~" : name;
			ImGui::TextUnformatted(lbl.c_str());
			if (name.size() > 13 && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", name.c_str());
		}
	};

	// Per-item right-click context menu (Rename / Delete + optional extras)
	auto beginItemRename = [&](const fs::path& itemPath)
	{
		mySelectedPath     = itemPath;
		strncpy_s(myRenameBuffer, sizeof(myRenameBuffer),
			itemPath.filename().string().c_str(), _TRUNCATE);
		myRenamingPath     = itemPath;
		myRenameJustOpened = true;
	};

	try
	{
		// ---- Folders ----
		for (const auto& entry : fs::directory_iterator(myCurrentPath))
		{
			if (!entry.is_directory()) continue;

			const fs::path& itemPath = entry.path();
			std::string name = itemPath.filename().string();

			ImGui::PushID(("d_" + name).c_str());

			if (itemPath == mySelectedPath) drawSelectionHighlight();

			ImGui::PushStyleColor(ImGuiCol_Button,        {0.22f, 0.16f, 0.06f, 1.0f});
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.34f, 0.26f, 0.10f, 1.0f});
			ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.44f, 0.34f, 0.14f, 1.0f});
			ImGui::Button("##folder", {thumbSize, thumbSize});
			ImGui::PopStyleColor(3);

			ImVec2 bMin = ImGui::GetItemRectMin();
			ImGui::GetWindowDrawList()->AddText(
				{bMin.x + thumbSize * 0.5f - 16.0f, bMin.y + thumbSize * 0.5f - 7.0f},
				IM_COL32(255, 200, 80, 255), "Folder");

			if (ImGui::IsItemClicked())
				mySelectedPath = itemPath;
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				myCurrentPath = itemPath;

			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Open"))           myCurrentPath = itemPath;
				ImGui::Separator();
				if (ImGui::MenuItem("Rename  F2"))     beginItemRename(itemPath);
				if (ImGui::MenuItem("Delete  Del"))
				{
					std::error_code ec;
					fs::remove_all(itemPath, ec);
					if (mySelectedPath == itemPath) mySelectedPath.clear();
					if (myCurrentPath  == itemPath) myCurrentPath  = myCurrentPath.parent_path();
				}
				ImGui::EndPopup();
			}

			drawLabel(itemPath, name);

			ImGui::PopID();
			ImGui::NextColumn();
		}

		// ---- Files ----
		for (const auto& entry : fs::directory_iterator(myCurrentPath))
		{
			if (entry.is_directory()) continue;

			const fs::path& itemPath = entry.path();
			std::string name   = itemPath.filename().string();
			std::string ext    = itemPath.extension().string();
			std::string extLow = ext;
			for (auto& c : extLow) c = (char)tolower((unsigned char)c);

			const bool isTexture = (extLow == ".dds" || extLow == ".png" || extLow == ".jpg");
			const bool isModel   = (extLow == ".fbx" || extLow == ".obj");
			const bool isPrefab  = (extLow == ".prefab");

			ImGui::PushID(("f_" + name).c_str());

			if (itemPath == mySelectedPath) drawSelectionHighlight();

			if (isTexture)
			{
				void* tex = DX_Texture2D::GetTexture(itemPath.string().c_str());
				if (tex)
					ImGui::ImageButton(tex, {thumbSize, thumbSize});
				else
				{
					ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.12f, 0.12f, 1.0f});
					ImGui::Button("##tex", {thumbSize, thumbSize});
					ImGui::PopStyleColor();
				}
			} else if (isModel)
			{
				ImGui::PushStyleColor(ImGuiCol_Button,        {0.08f, 0.18f, 0.32f, 1.0f});
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.12f, 0.26f, 0.46f, 1.0f});
				ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.16f, 0.34f, 0.60f, 1.0f});
				ImGui::Button("##model", {thumbSize, thumbSize});
				ImGui::PopStyleColor(3);
				ImVec2 bMin = ImGui::GetItemRectMin();
				ImGui::GetWindowDrawList()->AddText(
					{bMin.x + thumbSize * 0.5f - 12.0f, bMin.y + thumbSize * 0.5f - 7.0f},
					IM_COL32(120, 180, 255, 255), "Mesh");
			} else if (isPrefab)
			{
				ImGui::PushStyleColor(ImGuiCol_Button,        {0.06f, 0.22f, 0.14f, 1.0f});
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.10f, 0.34f, 0.22f, 1.0f});
				ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.14f, 0.44f, 0.28f, 1.0f});
				ImGui::Button("##prefab", {thumbSize, thumbSize});
				ImGui::PopStyleColor(3);
				ImVec2 bMin = ImGui::GetItemRectMin();
				ImGui::GetWindowDrawList()->AddText(
					{bMin.x + thumbSize * 0.5f - 18.0f, bMin.y + thumbSize * 0.5f - 7.0f},
					IM_COL32(100, 220, 140, 255), "Prefab");
			} else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, {0.18f, 0.18f, 0.18f, 1.0f});
				ImGui::Button("##file", {thumbSize, thumbSize});
				ImGui::PopStyleColor();
				ImVec2 bMin = ImGui::GetItemRectMin();
				ImGui::GetWindowDrawList()->AddText(
					{bMin.x + 4.0f, bMin.y + thumbSize * 0.5f - 7.0f},
					IM_COL32(180, 180, 180, 255), extLow.c_str());
			}

			if (ImGui::IsItemClicked())
				mySelectedPath = itemPath;

			if (ImGui::BeginPopupContextItem())
			{
				if (isModel   && ImGui::MenuItem("Add to Scene"))
					myScene->CreateModelInstance(itemPath.string().c_str());
				if (isPrefab  && ImGui::MenuItem("Spawn in Scene"))
				{
					Prefab prefab;
					auto spawned = prefab.Spawn(myScene.get(), itemPath.string().c_str(), { 0.0f, 1.25f, 0.0f });
					Selection::ClearSelection();
					for (auto* mi : spawned)
						Selection::AddToSelection(mi);
				}
				if (isTexture && ImGui::MenuItem("Copy Path"))
					ImGui::SetClipboardText(itemPath.string().c_str());
				ImGui::Separator();
				if (ImGui::MenuItem("Rename  F2"))     beginItemRename(itemPath);
				if (ImGui::MenuItem("Delete  Del"))
				{
					std::error_code ec;
					fs::remove(itemPath, ec);
					if (mySelectedPath == itemPath) mySelectedPath.clear();
				}
				ImGui::EndPopup();
			}

			if (isTexture && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
			{
				std::string path = itemPath.string();
				ImGui::SetDragDropPayload(extLow.c_str(), path.c_str(), path.size() + 1);
				void* tex = DX_Texture2D::GetTexture(path.c_str());
				if (tex) ImGui::Image(tex, {32.0f, 32.0f});
				ImGui::TextUnformatted(name.c_str());
				ImGui::EndDragDropSource();
			}
			if (isPrefab && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
			{
				std::string path = itemPath.string();
				ImGui::SetDragDropPayload(".prefab", path.c_str(), path.size() + 1);
				ImGui::TextUnformatted(name.c_str());
				ImGui::EndDragDropSource();
			}

			drawLabel(itemPath, name);

			ImGui::PopID();
			ImGui::NextColumn();
		}
	} catch (...) {}

	ImGui::Columns(1);

	// ---- Keyboard shortcuts (active when the grid child window is focused) ----
	if (ImGui::IsWindowFocused() && myRenamingPath.empty())
	{
		if (!mySelectedPath.empty())
		{
			// F2: rename
			if (ImGui::IsKeyPressed(VK_F2))
				beginItemRename(mySelectedPath);

			// Delete: remove file or folder
			if (ImGui::IsKeyPressed(ImGuiKey_Delete))
			{
				std::error_code ec;
				if (fs::is_directory(mySelectedPath)) fs::remove_all(mySelectedPath, ec);
				else                                   fs::remove(mySelectedPath, ec);
				if (myCurrentPath == mySelectedPath) myCurrentPath = myCurrentPath.parent_path();
				mySelectedPath.clear();
			}

			// Enter: navigate into selected folder
			if (ImGui::IsKeyPressed(ImGuiKey_Enter) && fs::is_directory(mySelectedPath))
				myCurrentPath = mySelectedPath;
		}

		// Backspace: go up one directory
		if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && myCurrentPath != myRootPath)
			myCurrentPath = myCurrentPath.parent_path();
	}
}

// ---------------------------------------------------------------------------
// New C++ Component wizard
// ---------------------------------------------------------------------------
void AssetBrowser::DrawCreateComponentModal()
{
	// OpenPopup must be called outside of the BeginPopupModal block
	if (myOpenCreateComponentModal)
	{
		ImGui::OpenPopup("New C++ Component");
		myOpenCreateComponentModal = false;
	}

	// Center on the main viewport every frame while open
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		{vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f},
		ImGuiCond_Always, {0.5f, 0.5f});
	ImGui::SetNextWindowSize({500.0f, 0.0f}, ImGuiCond_Always);

	if (!ImGui::BeginPopupModal("New C++ Component", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
		return;

	// ---- Class name ----
	ImGui::TextUnformatted("Class Name");
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##cname", myNewComponentName, sizeof(myNewComponentName));

	ImGui::Spacing();

	// ---- Parent (fixed) ----
	ImGui::TextUnformatted("Parent Class");
	ImGui::SameLine();
	ImGui::TextDisabled("Component  (fixed)");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// ---- Output directory ----
	ImGui::TextUnformatted("Output Directory");
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##cdir", myComponentOutputPath, sizeof(myComponentOutputPath));
	ImGui::TextDisabled("Tip: set this to your game project's source folder.");

	// ---- Validation ----
	const std::string name   = myNewComponentName;
	const fs::path    outDir = myComponentOutputPath;

	bool nameOk = !name.empty()
	           && (std::isalpha((unsigned char)name[0]) || name[0] == '_')
	           && name.find(' ') == std::string::npos;
	bool dirOk  = fs::exists(outDir) && fs::is_directory(outDir);

	bool hAlreadyExists = nameOk && dirOk && fs::exists(outDir / (name + ".h"));
	bool cppAlreadyExists = nameOk && dirOk && fs::exists(outDir / (name + ".cpp"));

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// ---- Preview ----
	if (nameOk && dirOk)
	{
		ImGui::TextDisabled("Will create:");
		auto previewLine = [&](const std::string& filename, bool exists)
		{
			if (exists)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 0.6f, 0.2f, 1.0f});
				ImGui::Text("  %s  (already exists, will overwrite)", filename.c_str());
				ImGui::PopStyleColor();
			} else
			{
				ImGui::TextDisabled("  %s", filename.c_str());
			}
		};
		previewLine(name + ".h",   hAlreadyExists);
		previewLine(name + ".cpp", cppAlreadyExists);
	} else
	{
		if (!nameOk && !name.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 0.4f, 0.4f, 1.0f});
			ImGui::TextUnformatted("  Name must start with a letter and contain no spaces.");
			ImGui::PopStyleColor();
		}
		if (!dirOk)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 0.4f, 0.4f, 1.0f});
			ImGui::TextUnformatted("  Output directory does not exist.");
			ImGui::PopStyleColor();
		}
	}

	ImGui::Spacing();
	ImGui::TextDisabled("After creation, add the files to your Visual Studio project.");
	ImGui::Spacing();

	// ---- Buttons ----
	const bool canCreate = nameOk && dirOk;
	if (!canCreate) ImGui::BeginDisabled();
	if (ImGui::Button("Create Class", {130.0f, 0.0f}))
	{
		WriteComponentFiles(name, outDir);
		ImGui::CloseCurrentPopup();
	}
	if (!canCreate) ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Cancel", {80.0f, 0.0f}))
		ImGui::CloseCurrentPopup();

	ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// File generator
// ---------------------------------------------------------------------------
void AssetBrowser::WriteComponentFiles(const std::string& aName, const fs::path& aOutputDir)
{
	// ---- Header ----
	{
		std::ofstream f(aOutputDir / (aName + ".h"));
		f << "#pragma once\n"
		     "#include <Components/Component.h>\n"
		     "\n"
		     "class " << aName << " : public Component {\n"
		     "public:\n"
		     "\t" << aName << "();\n"
		     "\n"
		     "\tvoid OnStart() override;\n"
		     "\tvoid Update(float aDeltaTime) override;\n"
		     "\n"
		     "\t// ---------------------------------------------------------------\n"
		     "\t// Expose member variables to the editor inspector, for example:\n"
		     "\t//\n"
		     "\t//   EXPOSE_FLOAT(\"Speed\",     mySpeed);\n"
		     "\t//   EXPOSE_FLOAT_EX(\"Mass\",   myMass,   0.1f, 0.f, 1000.f);\n"
		     "\t//   EXPOSE_BOOL(\"Active\",     myActive);\n"
		     "\t//   EXPOSE_INT(\"Count\",       myCount);\n"
		     "\t//   EXPOSE_VEC3(\"Direction\",  myDirection);\n"
		     "\t// ---------------------------------------------------------------\n"
		     "\n"
		     "\t// float  mySpeed     = 1.0f;\n"
		     "\t// bool   myActive    = true;\n"
		     "\t// int    myCount     = 0;\n"
		     "};\n";
	}

	// ---- Source ----
	{
		std::ofstream f(aOutputDir / (aName + ".cpp"));
		f << "#include <stdafx.h>\n"
		     "#include \"" << aName << ".h\"\n"
		     "#include <Components/ComponentFactory.h>\n"
		     "\n"
		     "// Registers " << aName << " with the ComponentFactory at program startup so it\n"
		     "// appears in the editor's Add Component dropdown without any manual wiring.\n"
		     "namespace { const bool sRegistered = (REGISTER_COMPONENT(" << aName << "), true); }\n"
		     "\n"
		     << aName << "::" << aName << "() {\n"
		     "\t// Register exposed properties here, e.g.:\n"
		     "\t// EXPOSE_FLOAT(\"Speed\", mySpeed);\n"
		     "}\n"
		     "\n"
		     "void " << aName << "::OnStart() {\n"
		     "}\n"
		     "\n"
		     "void " << aName << "::Update(float aDeltaTime) {\n"
		     "}\n";
	}
}

// ---------------------------------------------------------------------------
// New Prefab wizard
// ---------------------------------------------------------------------------
void AssetBrowser::DrawCreatePrefabModal()
{
	if (myOpenCreatePrefabModal)
	{
		ImGui::OpenPopup("New Prefab");
		myOpenCreatePrefabModal = false;
	}

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		{vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f},
		ImGuiCond_Always, {0.5f, 0.5f});
	ImGui::SetNextWindowSize({400.0f, 0.0f}, ImGuiCond_Always);

	if (!ImGui::BeginPopupModal("New Prefab", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
		return;

	ImGui::TextUnformatted("Prefab Name");
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##pname", myNewPrefabName, sizeof(myNewPrefabName));

	ImGui::Spacing();
	ImGui::TextUnformatted("Model  (mesh name or path)");
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("##pmodel", myNewPrefabModel, sizeof(myNewPrefabModel));
	ImGui::TextDisabled("e.g. Cube, or a full path to an .fbx file");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	const std::string name  = myNewPrefabName;
	bool nameOk = !name.empty()
	           && (std::isalpha((unsigned char)name[0]) || name[0] == '_')
	           && name.find(' ') == std::string::npos;
	bool modelOk = strlen(myNewPrefabModel) > 0;
	bool alreadyExists = nameOk && fs::exists(myCurrentPath / (name + ".prefab"));

	if (alreadyExists)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, {1.0f, 0.6f, 0.2f, 1.0f});
		ImGui::Text("  %s.prefab already exists, will overwrite", name.c_str());
		ImGui::PopStyleColor();
	}

	const bool canCreate = nameOk && modelOk;
	if (!canCreate) ImGui::BeginDisabled();
	if (ImGui::Button("Create Prefab", {120.0f, 0.0f}))
	{
		WritePrefabFile(name, myNewPrefabModel, myCurrentPath);
		ImGui::CloseCurrentPopup();
	}
	if (!canCreate) ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Cancel", {80.0f, 0.0f}))
		ImGui::CloseCurrentPopup();

	ImGui::EndPopup();
}

void AssetBrowser::WritePrefabFile(const std::string& aName, const std::string& aModel,
                                    const fs::path& aDir)
                                    {
	std::ofstream f(aDir / (aName + ".prefab"));
	f << "[\n"
	  << "  {\n"
	  << "    \"Name\": \"" << aName << "\",\n"
	  << "    \"Model\": \"" << aModel << "\"\n"
	  << "  }\n"
	  << "]\n";
}

// ---------------------------------------------------------------------------
// Main draw: toolbar | left folder tree | right asset grid
// ---------------------------------------------------------------------------
void AssetBrowser::Draw()
{
	ImGui::Begin("Content Browser");
	{
		// ---- Thumbnail size slider ----
		ImGui::SetNextItemWidth(100.0f);
		ImGui::SliderFloat("##sz", &myThumbnailSize, 40.0f, 128.0f, "");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Thumbnail size");

		ImGui::SameLine(0, 12);

		// ---- Breadcrumb ----
		std::vector<fs::path> stack;
		for (fs::path p = myCurrentPath; ; p = p.parent_path())
		{
			stack.push_back(p);
			if (p == myRootPath || p == p.parent_path()) break;
		}
		std::reverse(stack.begin(), stack.end());

		for (int i = 0; i < (int)stack.size(); i++)
		{
			if (i > 0) { ImGui::SameLine(0, 4); ImGui::TextDisabled(">"); ImGui::SameLine(0, 4); }
			if (ImGui::SmallButton(stack[i].filename().string().c_str()))
				myCurrentPath = stack[i];
		}

		ImGui::Separator();

		// ---- Left: folder tree ----
		ImGui::BeginChild("##tree", {180.0f, 0.0f}, true);
		{
			ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen
			                            | ImGuiTreeNodeFlags_SpanAvailWidth
			                            | ImGuiTreeNodeFlags_OpenOnArrow;
			if (myCurrentPath == myRootPath) rootFlags |= ImGuiTreeNodeFlags_Selected;

			bool rootOpen = ImGui::TreeNodeEx("Content", rootFlags);
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				myCurrentPath = myRootPath;
			if (rootOpen)
			{
				DrawFolderTree(myRootPath);
				ImGui::TreePop();
			}
		}
		ImGui::EndChild();

		ImGui::SameLine();

		// ---- Right: asset grid ----
		ImGui::BeginChild("##grid", {0.0f, 0.0f}, false,
			ImGuiWindowFlags_HorizontalScrollbar);
		DrawAssetGrid();
		ImGui::EndChild();
	}
	ImGui::End();

	// Modals are drawn outside the Content Browser window so they centre
	// over the entire viewport instead of just the panel.
	DrawCreateComponentModal();
	DrawCreatePrefabModal();
}
