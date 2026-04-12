require('vstudio')

workspace "Tools"
	location ".."
	architecture "x64"
	startproject "Editor"

	configurations {
		"Debug",
		"Release",
		"Distribution"
	}

dirs = {}
dirs.outdir = os.realpath("../bin")
dirs.intdir = os.realpath("../int-bin")
dirs.libdir = os.realpath("../lib")
dirs.localdir = os.realpath("../local")

for i,v in pairs(dirs) do
	if not os.isdir(v) then
		os.mkdir(v)
		print ('created: ' .. v)
	end
end

dirs.srcdir = os.realpath("source")
dirs.extdir = os.realpath("source/External")

game = {}

include ("source/Projects")

resdir = {}
resdir.engine = os.realpath("EngineResources")
resdir.engineraw = "EngineResources"

resdir.game = os.realpath( "../" .. game.name)
resdir.gameraw = "../" .. game.name

if not os.isdir(resdir.game) then
	os.mkdir(resdir.game)
	os.mkdir(resdir.game .. "/Assets")
	os.mkdir(resdir.game .. "/Assets/Textures")
end

settingsdir = {}
settingsdir.engine = os.realpath("../EngineSettings")

if not os.isdir(resdir.engine) or not os.isfile(resdir.engine .. "/window-settings.json") then
	os.mkdir(resdir.engine)
	default_window = {
		rect = {x = 0, y = 0, w = 1600, h = 900},
		title = "Tools Uppgift #3",
		clear_color = {r=0.0, g=0.2, b=0.25, a=1.0}
	}
	io.writefile(
		resdir.engine .. "/window-settings.json", 
		json.encode(default_window)
	)
end

--if not os.isdir(resdir.game) then
	texture_src = resdir.engineraw .. "/Assets/Textures"
	texture_dst = resdir.gameraw .. "/Assets/Textures"

	os.copyfile(os.realpath(texture_src .. "/Texture.dds"), os.realpath(texture_dst .. "/Texture.dds"))
	os.copyfile(os.realpath(texture_src .. "/Texture2.dds"), os.realpath(texture_dst .. "/Texture2.dds"))
	--os.copyfile(resdir.engine .. "/Textures/Texture2.dds", texture_path)
--end

if not os.isdir(settingsdir.engine) or not os.isfile(settingsdir.engine .. "/engine-settings.json") then
	os.mkdir(settingsdir.engine)
	settings = {
		engine_resources_path = path.translate(resdir.engine, "/"),
		game_resources_path = path.translate(resdir.game, "/"),
		window_settings = "/window-settings.json"
	}
	io.writefile(
		settingsdir.engine .. "/engine-settings.json",
		json.encode(settings)
	)
end


include ("source/External")
include ("source")