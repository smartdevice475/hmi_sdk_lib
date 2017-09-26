set(GSTPLAYER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(GSTPLAYER_INCLUDE_DIR ${GSTPLAYER_INCLUDE_DIR} PARENT_SCOPE)

set(TARGET gstplayer)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    /usr/include/glib-2.0
    /usr/include/gstreamer-1.0
    /usr/lib/x86_64-linux-gnu/
    /usr/lib/x86_64-linux-gnu/glib-2.0/include
    /usr/lib/x86_64-linux-gnu/gstreamer-1.0/include
)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux_armhf")
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    /usr/arm-linux-gnueabihf/include
    /usr/arm-linux-gnueabihf/include/gstreamer-1.0
    /usr/arm-linux-gnueabihf/include/glib-2.0
    /usr/arm-linux-gnueabihf/lib/glib-2.0/include
)
link_directories(
    /usr/arm-linux-gnueabihf/lib
)
endif()

set(SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/gst_player.cc
)

set(LIBRARIES
  gstreamer-1.0
  gobject-2.0
  glib-2.0
)

add_library(${TARGET} ${SOURCES})

target_link_libraries(${TARGET} ${LIBRARIES})
