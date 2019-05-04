
option(USE_OPENBW "Use OpenBW" ON)

include_directories(include)

if (BUILD_COMPAT)
	if (NOT MSVC)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
	endif()
endif()

if (USE_OPENBW)
  include_directories(OpenBWData)
  add_subdirectory(OpenBWData)
  add_library(BWData ALIAS OpenBWData)
endif()

add_subdirectory(BWAPICore)
add_subdirectory(BWAPILIB)
add_subdirectory(BWAPI)
add_subdirectory(BWAPILauncher)
add_subdirectory(BWAPIClient)

add_subdirectory(ExampleAIModule)
add_subdirectory(TestAIModule)

if (BUILD_COMPAT)
	add_subdirectory(compat)
endif()

install(FILES "./include/BWAPI.h" DESTINATION include)
install(DIRECTORY "./include/BWAPI" DESTINATION include)
