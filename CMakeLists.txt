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
