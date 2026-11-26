cmake_minimum_required(VERSION 3.25)
project(eqapo VERSION 0.1.5 LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OBJCXX_STANDARD 20)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "14.2" CACHE STRING "Minimum macOS version")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)
if(MSVC)
  set(EQAPO_WARNINGS /W4 /wd4100)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_definitions(_USE_MATH_DEFINES)
else()
  set(EQAPO_WARNINGS -Wall -Wextra -Wno-unused-parameter)
endif()
if(MINGW)
  add_compile_definitions(__USE_MINGW_ANSI_STDIO=1)
endif()

# ---------------------------------------------------------------- engine
file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS
  src/engine/*.cpp
  src/engine/filters/*.cpp
  src/engine/factories/*.cpp
  src/engine/dsp/*.cpp
  src/engine/expr/*.cpp)
add_library(eqapo_engine STATIC ${ENGINE_SOURCES})
target_include_directories(eqapo_engine PUBLIC src)
target_compile_options(eqapo_engine PRIVATE ${EQAPO_WARNINGS})
target_link_libraries(eqapo_engine PUBLIC Threads::Threads)
if(APPLE)
  target_link_libraries(eqapo_engine PUBLIC "-framework Accelerate")
endif()

# ---------------------------------------------------------------- audio file I/O
if(APPLE)
  add_library(eqapo_audio STATIC src/audio/AudioFile.mm)
else()
  add_library(eqapo_audio STATIC src/audio/AudioFile.cpp)
endif()
target_include_directories(eqapo_audio PUBLIC src)
target_compile_options(eqapo_audio PRIVATE ${EQAPO_WARNINGS})
if(APPLE)
  target_link_libraries(eqapo_audio PUBLIC "-framework AudioToolbox" "-framework CoreFoundation")
endif()

# ---------------------------------------------------------------- tests
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(eqapo_tests ${TEST_SOURCES})
target_compile_options(eqapo_tests PRIVATE ${EQAPO_WARNINGS})
target_link_libraries(eqapo_tests PRIVATE eqapo_engine eqapo_audio)
enable_testing()
add_test(NAME unit COMMAND eqapo_tests)

# ---------------------------------------------------------------- eqapo binary
if(APPLE)
  set(CLI_SOURCES
    src/cli/main.cpp
    src/cli/OfflineCommands.cpp
    src/cli/PresetCommands.cpp
    src/cli/DaemonCommands.mm
    src/cli/SelftestCommand.mm
    src/daemon/AudioPipeline.mm
    src/daemon/ConfigWatcher.mm
    src/daemon/CoreAudioUtil.mm
    src/daemon/Daemon.mm
    src/daemon/DeviceMonitor.mm
    src/daemon/Paths.mm
    src/daemon/StatusFile.mm)
else()
  set(CLI_SOURCES
    src/cli/main.cpp
    src/cli/OfflineCommands.cpp
    src/cli/PresetCommands.cpp
    src/daemon/Paths.cpp)
endif()
add_executable(eqapo ${CLI_SOURCES})
target_compile_options(eqapo PRIVATE ${EQAPO_WARNINGS})
target_compile_definitions(eqapo PRIVATE
  EQAPO_VERSION="${PROJECT_VERSION}"
  EQAPO_SOURCE_PRESETS_DIR="${CMAKE_SOURCE_DIR}/presets")
target_link_libraries(eqapo PRIVATE eqapo_engine eqapo_audio)
if(APPLE)
  target_compile_options(eqapo PRIVATE -fobjc-arc)
  target_link_libraries(eqapo PRIVATE
    "-framework CoreAudio" "-framework AudioToolbox" "-framework Foundation" "-framework CoreServices")
  # Embedded Info.plist: gives the CLI a bundle identifier and the system-audio usage string for the TCC prompt.
  target_link_options(eqapo PRIVATE "LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_SOURCE_DIR}/src/cli/Info.plist")
  set_property(TARGET eqapo APPEND PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/src/cli/Info.plist")
  # Ad-hoc signature with a stable identifier so the privacy grant survives rebuilds of the same binary.
  add_custom_command(TARGET eqapo POST_BUILD
    COMMAND codesign --force --sign - --identifier com.eqapo.daemon "$<TARGET_FILE:eqapo>"
    COMMENT "Ad-hoc code signing eqapo")
elseif(MINGW)
  target_link_options(eqapo PRIVATE -static)
  target_link_options(eqapo_tests PRIVATE -static)
endif()

install(TARGETS eqapo RUNTIME DESTINATION bin)
install(DIRECTORY presets/ DESTINATION share/eqapo/presets)

set(CPACK_PACKAGE_VENDOR "PDGamerSG")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Offline Equalizer APO-compatible WAV processing and presets")
if(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|X86_64)$" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  install(FILES README.md COPYING DESTINATION .)
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
  set(CPACK_GENERATOR NSIS)
  set(CPACK_PACKAGE_INSTALL_DIRECTORY eqapo)
  set(CPACK_PACKAGE_FILE_NAME "eqapo-${PROJECT_VERSION}-windows-x64-setup")
  set(CPACK_NSIS_DISPLAY_NAME "eqapo offline equalizer")
  set(CPACK_NSIS_MODIFY_PATH ON)
  # Select the native x64 NSIS stub and plugins for the x64 installer.
  set(CPACK_NSIS_DEFINES "Target amd64-unicode")
  include(CPack)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  install(FILES README.md packaging/debian/copyright DESTINATION share/doc/eqapo)
  set(CPACK_GENERATOR DEB)
  set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
  set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "PDGamerSG <177869559+PDGamerSG@users.noreply.github.com>")
  set(CPACK_DEBIAN_PACKAGE_SECTION sound)
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  include(CPack)
endif()
