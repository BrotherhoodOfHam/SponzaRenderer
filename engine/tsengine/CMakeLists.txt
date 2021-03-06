#
#	tsengine
#

SET(tsengine_inc

	inc/tsengine.h
	inc/tsengine/abi.h

	inc/tsengine/event/event.h
	inc/tsengine/event/messenger.h
	
	inc/tsengine/input/inputdevice.h
	inc/tsengine/input/keycodes.h
	inc/tsengine/input/inputmodule.h
	
	inc/tsengine/platform/window.h
	
	inc/tsengine/cmdargs.h
	inc/tsengine/cvar.h
	inc/tsengine/configfile.h
)

SET(tsengine_src
	
	src/input/inputdevice.cpp
	src/input/keycodes.cpp
	src/input/inputmodule.cpp
	
	src/event/event.cpp
	
	src/platform/console.h
	src/platform/console.cpp
	src/platform/window.cpp
	
	src/cmdargs.cpp
	src/configfile.cpp
	src/cvar.cpp
	
	src/tsengine.cpp
)

SET(tsengine_files ${tsengine_src} ${tsengine_inc})

add_library_tse(
	tsengine
	"${tsengine_files}"
)

target_link_libraries(tsengine PUBLIC tscore tsgraphics)

target_include_directories(
	tsengine
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

assign_source_groups(${tsengine_files})
