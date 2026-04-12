project "GameLauncher"
	location "%{dirs.localdir}"

	kind "WindowedApp"
	language "C++"
	cppdialect "C++17"
	

	targetdir ("%{dirs.outdir}")
	targetname("%{prj.name}_%{cfg.buildcfg}")
	objdir ("%{dirs.intdir}")

	debugdir ("%{dirs.outdir}") --needed?

	files { "**.h", "**.cpp", }
	print (dirs.srcdir)

	includedirs {
		dirs.srcdir .. "/Engine/",
		dirs.srcdir .. "/Projects/%{game.name}/",
		"%{dirs.extdir}/rapidjson/include/",
		dirs.extdir .. "/dearimgui/",
	}

	libdirs { "%{dirs.libdir}" }

	links {
		"Engine",
		"%{game.name}",
		"dearimgui",
	}