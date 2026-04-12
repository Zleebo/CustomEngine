#pragma once

#include <ToolsInterface/ToolsInterface.h>
#include <filesystem>
#include <memory>
#include <string>

namespace fs = std::filesystem;
class Scene;

class AssetBrowser : public ToolsInterface
{
public:
	AssetBrowser(std::shared_ptr<Scene> aScene);

	virtual void Draw() override;

private:
	void DrawFolderTree(const fs::path& aPath);
	void DrawAssetGrid();

	// New-component wizard
	void DrawCreateComponentModal();
	static void WriteComponentFiles(const std::string& aName, const fs::path& aOutputDir);

	// New-prefab wizard
	void DrawCreatePrefabModal();
	static void WritePrefabFile(const std::string& aName, const std::string& aModel, const fs::path& aDir);

	std::shared_ptr<Scene> myScene;
	fs::path myRootPath;
	fs::path myCurrentPath;
	float    myThumbnailSize = 80.0f;

	// Selection & inline rename
	fs::path mySelectedPath;
	fs::path myRenamingPath;
	char     myRenameBuffer[256]   = {};
	bool     myRenameJustOpened    = false;

	// Component modal
	bool myOpenCreateComponentModal  = false;
	char myNewComponentName[128]     = "MyComponent";
	char myComponentOutputPath[512]  = {};

	// Prefab modal
	bool myOpenCreatePrefabModal = false;
	char myNewPrefabName[128]    = "NewPrefab";
	char myNewPrefabModel[256]   = "Cube";
};
