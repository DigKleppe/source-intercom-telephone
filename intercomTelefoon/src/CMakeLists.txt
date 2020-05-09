
add_executable(telefoon main.cpp videoThread.cpp audioTransmitThread.cpp audioReceiveThread.cpp connections.cpp updateThread.cpp io.cpp testThread.cpp timerThread.cpp keys.cpp)   

target_include_directories(telefoon PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           "${SYSROOTPATH}usr/include/gstreamer-1.0"
                           "${SYSROOTPATH}usr/include/glib-2.0"
                           "${SYSROOTPATH}usr/lib/glib-2.0/include"
                           )

list(APPEND EXTRA_LIBS gstreamer-1.0 pthread gstreamer-1.0  glib-2.0 gobject-2.0 ssl crypto)
target_link_libraries(telefoon PUBLIC ${EXTRA_LIBS}) 

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_BUILD_TYPE Debug)



# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h


