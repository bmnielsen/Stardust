include_directories(
  ../include 
)

set(CMAKE_SHARED_LIBRARY_PREFIX "")

add_library(ExampleAIModule SHARED
  Source/Dll.cpp
  Source/ExampleAIModule.cpp
  )

target_link_libraries(ExampleAIModule BWAPILIB)
