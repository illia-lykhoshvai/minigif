cmake_minimum_required(VERSION 3.12)
project(minigifTest)

set(CMAKE_CXX_STANDARD 11)

# Dependencies
set(RAYLIB_VERSION 5.5)
find_package(raylib ${RAYLIB_VERSION} QUIET) # QUIET or REQUIRED
if (NOT raylib_FOUND) # If there's none, fetch and build raylib
  include(FetchContent)
  if(UNIX AND NOT APPLE)
    set(RAYLIB_URL https://github.com/raysan5/raylib/releases/download/${RAYLIB_VERSION}/raylib-${RAYLIB_VERSION}_linux_amd64.tar.gz)
  elseif(WIN32)
    set(RAYLIB_URL https://github.com/raysan5/raylib/releases/download/${RAYLIB_VERSION}/raylib-${RAYLIB_VERSION}_win64_mingw-w64.zip)
  else()
    message(FATAL_ERROR "Unsupported platform")
  endif()
  FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL ${RAYLIB_URL}
  )
  FetchContent_GetProperties(raylib)
  if (NOT raylib_POPULATED) # Have we downloaded raylib yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_MakeAvailable(raylib)
  endif()
endif()

# Raygui Dependency
set(RAYGUI_VERSION 4.0)
find_package(raygui ${RAYGUI_VERSION} QUIET) # QUIET or REQUIRED
if (NOT raygui_FOUND) # If there's none, fetch and build raygui
  include(FetchContent)
  FetchContent_Declare(
    raygui
    URL https://raw.githubusercontent.com/raysan5/raygui/refs/heads/master/src/raygui.h
    DOWNLOAD_NO_EXTRACT TRUE # Ensures it's treated as a single file
  )
  FetchContent_GetProperties(raygui)
  if (NOT raygui_POPULATED) # Have we downloaded raygui yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_MakeAvailable(raygui)
    include_directories(${raygui_SOURCE_DIR})
  endif()
endif()

include(../CMakeLists.txt) # minigif 

add_executable(${PROJECT_NAME} main.c ${MINIGIF_SOURCES})
include_directories("${raylib_SOURCE_DIR}/include" ${MINIGIF_INCLUDE})

# Platform-specific library linking
if(UNIX AND NOT APPLE) # Linux
  # Assuming raylib.so is available after extraction from tar.gz
  set(RAYLIB_LIB_PATH "${raylib_SOURCE_DIR}/lib/libraylib.so")
  target_link_libraries(${PROJECT_NAME} PRIVATE ${RAYLIB_LIB_PATH} m)
elseif(WIN32) # Windows
  set(RAYLIB_LIB_PATH "${raylib_SOURCE_DIR}/lib/libraylibdll.a")
  set(RAYLIB_LIB_DLL "${raylib_SOURCE_DIR}/lib/raylib.dll")
  target_link_libraries(${PROJECT_NAME} ${RAYLIB_LIB_PATH})
elseif(APPLE) # macOS
  target_link_libraries(${PROJECT_NAME} "-framework IOKit")
  target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
  target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
else()
  message(FATAL_ERROR "Unsupported platform")
endif()

# Post-build: Copy executable to 'out' folder
if(WIN32)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/out
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/out
    COMMAND ${CMAKE_COMMAND} -E copy "${RAYLIB_LIB_DLL}" ${CMAKE_CURRENT_SOURCE_DIR}/out
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/test.gif" ${CMAKE_CURRENT_SOURCE_DIR}/out
  )
else()
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/out
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR}/out
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/test.gif" ${CMAKE_CURRENT_SOURCE_DIR}/out
  )
endif()
