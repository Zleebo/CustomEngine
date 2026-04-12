project "Editor"
	location "%{dirs.localdir}"

	kind "WindowedApp"
	language "C++"
	cppdialect "C++17"

	targetdir ("%{dirs.outdir}")
	targetname("%{prj.name}_%{cfg.buildcfg}")
	objdir ("%{dirs.intdir}")

	debugdir ("%{dirs.outdir}")

	files { "**.h", "**.cpp", }
	print (dirs.srcdir)

	includedirs {
		".", "./Tools/**",
		dirs.srcdir .. "/Engine/",
		dirs.srcdir .. "/Projects/%{game.name}/",
		dirs.extdir .. "/dearimgui/",
		dirs.extdir .. "/imguizmo/",
		dirs.extdir .. "/simpleson/",
	}

	libdirs { "%{dirs.libdir}" }

	links { 
		"Engine",
		"%{game.name}",
		"dearimgui",
	}