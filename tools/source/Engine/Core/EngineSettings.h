#pragma once

#include <array>
#include <string>
#include <unordered_map>

namespace Settings
{
////////////////////////
	// Type definitions
	struct WindowData
	{
		int x = 100, y = 100, w = 1600, h = 900;
		std::array<float, 4> clear_color{ 0.0f, 0.1f, 0.2f, 1.0f };
		std::string title = "Tools assignments";
	};

////////////////////////
	// Variable declarations
	//extern std::unordered_map<std::string, std::string> paths;
	extern WindowData window_data;
	// better paths..
	namespace paths
	{
		extern std::string game_asset_root;
		extern std::string engine_resources_path;
		extern std::string engine_assets;
		extern std::string shaders;
		extern std::string game_assets;
		extern std::string window_settings;
	};

////////////////////////
	// function declarations
	
	void load_window_data(const char *path="");
	void load_engine_settings(const char *path="");

	bool load_config_files();
}