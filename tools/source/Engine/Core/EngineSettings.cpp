#include <stdafx.h>
#include "EngineSettings.h"

#include <fstream>
#include <filesystem>
//#include <json.h>
#include <rapidjson/document.h>

#include <unordered_map>

namespace Settings
{
	Settings::WindowData window_data;
	namespace paths
	{
		std::string game_asset_root;
		std::string engine_resources_path;
		std::string engine_assets;
		std::string shaders;
		std::string game_assets;
		std::string window_settings;
	};
}

bool Settings::load_config_files()
{
	load_engine_settings();
	load_window_data();
	return true;
}

void Settings::load_engine_settings(const char* path)
{
	std::string engineSettingsPath = "../EngineSettings/engine-settings.json";
	std::filesystem::path baseDir = std::filesystem::absolute(engineSettingsPath).parent_path();

	std::ifstream engine_ifs(engineSettingsPath);

	std::string engineSettingsString(
		(std::istreambuf_iterator<char>(engine_ifs)),
		(std::istreambuf_iterator<char>())
	);
	engine_ifs.close();
	rapidjson::Document engineSettings;
	engineSettings.Parse(engineSettingsString.c_str());

//	json::jobject engineSettings = json::jobject::parse(engineSettingsString);

	auto resolve = [&](const std::string& p)
	{
		return std::filesystem::weakly_canonical(baseDir / p).string();
	};

	using namespace paths;
	engine_resources_path	= resolve(engineSettings["engine_resources_path"].GetString());
	engine_assets			= engine_resources_path + "/Assets";
	shaders					= engine_assets + "/Shaders";

	game_asset_root = resolve(engineSettings["game_resources_path"].GetString());
	game_assets = game_asset_root + "/Assets";

	window_settings = engine_resources_path + engineSettings["window_settings"].GetString();
}

void Settings::load_window_data(const char *path)
{
	std::ifstream windowSettings_ifs(paths::window_settings);

	std::string engineSettingsString(
		(std::istreambuf_iterator<char>(windowSettings_ifs)),
		(std::istreambuf_iterator<char>())
	);
	rapidjson::Document doc;
	doc.Parse(engineSettingsString.c_str());
	windowSettings_ifs.close();

	auto rect = doc["rect"].GetObj();
	Settings::window_data.x = rect["x"].GetInt();
	Settings::window_data.y = rect["y"].GetInt();
	Settings::window_data.w = rect["w"].GetInt();
	Settings::window_data.h = rect["h"].GetInt();

	auto color = doc["clear_color"].GetObj();
	Settings::window_data.clear_color[0] = color["r"].GetFloat();
	Settings::window_data.clear_color[1] = color["g"].GetFloat();
	Settings::window_data.clear_color[2] = color["b"].GetFloat();
	Settings::window_data.clear_color[3] = color["a"].GetFloat();

	Settings::window_data.title = doc["title"].GetString();
}