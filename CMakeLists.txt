cmake_minimum_required(VERSION 3.8)
project(harbour-sailqemu VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SAILFISHAPP REQUIRED sailfishapp)
pkg_check_modules(GIO REQUIRED gio-2.0 gio-unix-2.0)
pkg_check_modules(PIXMAN REQUIRED pixman-1)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Qml Quick Network DBus)

link_directories(${SAILFISHAPP_LIBRARY_DIRS})
add_executable(harbour-sailqemu
    src/main.cpp
    src/vmcontroller.cpp
    src/vmcontroller.h
    src/diskmanager.cpp
    src/diskmanager.h
    src/runtimediagnostics.cpp
    src/runtimediagnostics.h
)
target_include_directories(harbour-sailqemu PRIVATE ${SAILFISHAPP_INCLUDE_DIRS})
target_compile_options(harbour-sailqemu PRIVATE ${SAILFISHAPP_CFLAGS_OTHER})
target_link_libraries(harbour-sailqemu PRIVATE
    Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick Qt5::Network Qt5::DBus
    ${SAILFISHAPP_LIBRARIES}
)

# The listener is a separate process so decoding/PNG writes cannot block the UI.
add_executable(harbour-sailqemu-display-listener src/dbusdisplayhelper.cpp)
# Prevent Qt's `signals` macro from corrupting GLib GDBusSignalInfo::signals.
# This helper does not use Qt signals/slots, so the setting is scoped here.
target_compile_definitions(harbour-sailqemu-display-listener PRIVATE QT_NO_KEYWORDS)
target_include_directories(harbour-sailqemu-display-listener PRIVATE
    ${GIO_INCLUDE_DIRS} ${PIXMAN_INCLUDE_DIRS})
target_compile_options(harbour-sailqemu-display-listener PRIVATE
    ${GIO_CFLAGS_OTHER} ${PIXMAN_CFLAGS_OTHER})
target_link_libraries(harbour-sailqemu-display-listener PRIVATE
    Qt5::Core Qt5::Gui ${GIO_LIBRARIES} ${PIXMAN_LIBRARIES})

install(TARGETS harbour-sailqemu harbour-sailqemu-display-listener RUNTIME DESTINATION bin)
install(DIRECTORY qml/ DESTINATION share/harbour-sailqemu/qml)
install(PROGRAMS scripts/sailqemu-runner.sh DESTINATION share/harbour-sailqemu/bin)
install(PROGRAMS scripts/m16-release-check.sh DESTINATION share/harbour-sailqemu/bin)
install(FILES harbour-sailqemu.desktop DESTINATION share/applications)
install(FILES icons/86x86/harbour-sailqemu.png DESTINATION share/icons/hicolor/86x86/apps)

# Experimental Sailjail permission for QEMU display and helper executable.
install(FILES sailjail/SailQemuDbus.permission DESTINATION /etc/sailjail/permissions)
