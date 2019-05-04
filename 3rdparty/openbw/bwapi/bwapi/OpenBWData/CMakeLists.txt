
option(OPENBW_ENABLE_UI "Enables support for the graphical user interface, requires SDL2")

find_path(OPENBW_DIR bwgame.h
  PATHS
  ENV OPENBW_DIR
)

if (NOT OPENBW_DIR)
  message(FATAL_ERROR "Couldn't find openbw. Specify the path to it with the OPENBW_DIR variable")
endif()

if (OPENBW_ENABLE_UI)
  add_subdirectory(${OPENBW_DIR}/ui openbw_ui)
  add_definitions(-DOPENBW_ENABLE_UI)
endif()

include_directories(
  ../Util/Source
  ../BWAPI/openbw
  ../BWAPI/Source
  ${OPENBW_DIR}
)

add_library(OpenBWData SHARED
  BW/BWData.cpp
)

set_property(TARGET OpenBWData PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET OpenBWData PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)

if (OPENBW_ENABLE_UI)
  target_link_libraries(OpenBWData openbw_ui)
endif()

if (NOT WIN32)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()
find_package(Threads REQUIRED)
target_link_libraries(OpenBWData Threads::Threads)

if (WIN32)
  target_link_libraries(OpenBWData "ws2_32;mswsock")
endif()

install(TARGETS OpenBWData
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
