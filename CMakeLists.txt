cmake_minimum_required(VERSION 3.25)
project(eqapo LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OSX_DEPLOYMENT_TARGET "14.2" CACHE STRING "Minimum macOS version")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(EQAPO_WARNINGS -Wall -Wextra -Wno-unused-parameter)

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
target_link_libraries(eqapo_engine PUBLIC "-framework Accelerate")

# ---------------------------------------------------------------- tests
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(eqapo_tests ${TEST_SOURCES})
target_compile_options(eqapo_tests PRIVATE ${EQAPO_WARNINGS})
target_link_libraries(eqapo_tests PRIVATE eqapo_engine)
enable_testing()
add_test(NAME unit COMMAND eqapo_tests)

# ---------------------------------------------------------------- eqapo binary
file(GLOB CLI_SOURCES CONFIGURE_DEPENDS src/cli/*.mm src/daemon/*.mm src/audio/*.mm)
add_executable(eqapo ${CLI_SOURCES})
target_compile_options(eqapo PRIVATE ${EQAPO_WARNINGS} -fobjc-arc)
target_compile_definitions(eqapo PRIVATE
  EQAPO_VERSION="0.1.1"
  EQAPO_SOURCE_PRESETS_DIR="${CMAKE_SOURCE_DIR}/presets")
target_link_libraries(eqapo PRIVATE eqapo_engine
  "-framework CoreAudio" "-framework AudioToolbox" "-framework Foundation" "-framework CoreServices")
# Embedded Info.plist: gives the CLI a bundle identifier and the system-audio usage string for the TCC prompt.
target_link_options(eqapo PRIVATE "LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_SOURCE_DIR}/src/cli/Info.plist")
set_property(TARGET eqapo APPEND PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/src/cli/Info.plist")
# Ad-hoc signature with a stable identifier so the privacy grant survives rebuilds of the same binary.
add_custom_command(TARGET eqapo POST_BUILD
  COMMAND codesign --force --sign - --identifier com.eqapo.daemon "$<TARGET_FILE:eqapo>"
  COMMENT "Ad-hoc code signing eqapo")

install(TARGETS eqapo RUNTIME DESTINATION bin)
install(DIRECTORY presets/ DESTINATION share/eqapo/presets)
