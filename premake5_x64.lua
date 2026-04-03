PROJECT_GENERATOR_VERSION = 3

newoption({
    trigger = "gmcommon",
    description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
    value = "../garrysmod_common"
})

local gmcommon = assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
    "you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")
include(gmcommon)

CreateWorkspace({name = "voice_packets", abi_compatible = false, path = "projects/windows/" .. _ACTION .. "/x64"})
    CreateProject({serverside = false, source_path = "source", manual_files = true})
        staticruntime "Off"
        architecture "x64"
        
        filter "configurations:Debug"
            symbols "On"
            defines { "DEBUG", "_DEBUG" }
            optimize "Off"
            runtime "Debug"
            
        filter "configurations:Release"
            optimize "Speed"
            defines { "NDEBUG" }
            runtime "Release"
            
        filter {}
            includedirs {
                "deps/minhook/include",
                "deps/opus/include",
                "deps/steam/include"
            }
            
            libdirs {
                "deps/minhook/lib/x64",
                "deps/opus/lib/x64",
                "deps/steam/lib/x64"
            }
            
            links {
                "minhook",
                "opus",
                "sdkencryptedappticket",
                "steam_api"
            }
            
            files({ "source/*.cpp", "source/*.h" })
            
            IncludeLuaShared()
            IncludeScanning()
            IncludeDetouring()