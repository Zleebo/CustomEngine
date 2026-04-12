project "ExampleGame"
	location "%{dirs.localdir}"

	kind "StaticLib"
	language "C++"
	cppdialect "C++17"

	targetdir ("%{dirs.libdir}")
	targetname("%{prj.name}_%{cfg.buildcfg}")
	objdir ("%{dirs.intdir}")

	debugdir ("%{dirs.outdir}")

	files { "**.h", "**.cpp", }
	
	includedirs {
		".",
		dirs.srcdir .. "/Engine/",
		dirs.extdir .. "/dearimgui/",
	}

	libdirs { "%{dirs.libdir}" }

	links { 
		"Engine",
	}