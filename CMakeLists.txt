cmake_minimum_required(VERSION 3.26)
project(VulkanHPP VERSION 0.1.0 LANGUAGES C CXX)
set(project_dir ${CMAKE_SOURCE_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
#include(CTest)
#enable_testing()

# Укажем стандарт C++
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)


#SHADERS------------------------------------------------------------------------------------------------------------
function (add_slang_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})

  set(OUTPUT_SHADER_NAME "shader.spv")
  set(SHADERS_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src/shaders)
  set(SHADERS_OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/assets/shaders)
  set(FINAL_SHADER_PATH ${SHADERS_OUTPUT_DIR}/${OUTPUT_SHADER_NAME})
  set(ENTRY_POINTS -entry vertMain -entry fragMain)

  file(MAKE_DIRECTORY ${SHADERS_OUTPUT_DIR})
# -fvk-b-shift 0 0 -fvk-t-shift 0 0 -fvk-s-shift 0 0 
  add_custom_command(
    OUTPUT  ${FINAL_SHADER_PATH}

    COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${FINAL_SHADER_PATH}
    
    DEPENDS ${SHADER_SOURCES}
    
    COMMENT "Compiling Slang Shaders to ${FINAL_SHADER_PATH}"
    VERBATIM
  )

  add_custom_target(${TARGET} DEPENDS ${FINAL_SHADER_PATH})
endfunction()

set(SHADERS_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/shader.slang
)

add_slang_shader_target(compile_shaders SOURCES ${SHADERS_SRC})
#SHADERS------------------------------------------------------------------------------------------------------------


#COMPILER------------------------------------------------------------------------------------------------------------
if (MSVC)
  set(CMAKE_CXX_FLAGS_DEBUG           "/W1 /EHsc /FC /permissive- /MDd /Zi /Od" CACHE STRING "Flags for Debug builds" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE         "/W1 /EHsc /FC /permissive- /MD /O2 /DNDEBUG" CACHE STRING "Flags for Release builds" FORCE)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "/W1 /EHsc /FC /permissive- /MD /O2 /Zi /DNDEBUG" CACHE STRING "Flags for RelWithDebInfo builds" FORCE)
  
  add_compile_definitions(
    $<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=2>
    $<$<NOT:$<CONFIG:Debug>>:_ITERATOR_DEBUG_LEVEL=0>
  )

elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(CMAKE_CXX_FLAGS_DEBUG          "-g -Wall -Wextra -Wpedantic")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG -Wall -Wextra -Wpedantic")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG -Wall -Wextra -Wpedantic")
endif()


message(STATUS "CMake version: ${CMAKE_VERSION}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ compiler version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ compiler flags: ${CMAKE_CXX_FLAGS}")
#COMPILER------------------------------------------------------------------------------------------------------------


#FetchContent------------------------------------------------------------------------------------------------------------
# include(FetchContent)

# FetchContent_Declare(
    # fmt
    # GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    # GIT_TAG 11.1.0
# )
# FetchContent_MakeAvailable(fmt)
#FetchContent------------------------------------------------------------------------------------------------------------


if (NOT CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake$")
  message(FATAL_ERROR "\n"
    "----------------------------------------------------------------------------------\n"
    "ОШИБКА: vcpkg toolchain file не был предоставлен.\n"
    "----------------------------------------------------------------------------------\n"
  )
endif()


# list(APPEND CMAKE_PREFIX_PATH "C:/Progs/vcpkg/packages/imgui_x64-windows")

# find_package(imgui CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
#find_package(unofficial-shaderc CONFIG REQUIRED)
find_package(Stb REQUIRED) #NO CONFIG
# find_package(glm CONFIG REQUIRED)
# find_package(glad CONFIG REQUIRED)

set(VCPKG_CMAKE_PREFIX_PATH_BACKUP "${CMAKE_PREFIX_PATH}")
set(CMAKE_PREFIX_PATH "")
find_package(Vulkan REQUIRED MODULE)
find_program(SLANGC_EXECUTABLE slangc REQUIRED)
message(STATUS "Found Slang compiler: ${SLANGC_EXECUTABLE}")
set(CMAKE_PREFIX_PATH "${VCPKG_CMAKE_PREFIX_PATH_BACKUP}")
message(STATUS "Vulkan_INCLUDE_DIRS = ${Vulkan_INCLUDE_DIRS}")

#find_program(GLSLC_COMPILER glslc HINTS ${VULKAN_SDK}/bin)
#if(NOT GLSLC_COMPILER)
#  message(FATAL_ERROR "glslc not found! Make sure Vulkan SDK is installed correctly.")
#endif()

add_subdirectory(src)


#------------------------------------------------------------------------------------------------------------
# add_executable(${PROJECT_NAME} main.cpp ${SRCC})
# target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)
#------------------------------------------------------------------------------------------------------------
# ИКОНКА из ресурсного файла
#target_sources(${PROJECT_NAME} PRIVATE src/app.rc)

# Условное связывание библиотек в зависимости от конфигурации
#target_link_libraries(${PROJECT_NAME} PRIVATE 
    #$<$<CONFIG:Debug>:sfml-main-d sfml-graphics-s-d sfml-window-s-d sfml-system-s-d>
    #$<$<CONFIG:Release>:sfml-main sfml-graphics-s sfml-window-s sfml-system-s>
		
#		engine
#)

# Указываем флаг подсистемы Windows для компоновщика чтоб запускалось графическое приложение без консольного окна
#target_link_options(${PROJECT_NAME} PRIVATE  -Xlinker /SUBSYSTEM:CONSOLE) #WINDOWS


# set(CPACK_PROJECT_NAME ${PROJECT_NAME})
# set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
# include(CPack)

# #настройка метаданных проекта для упаковки
# set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
# set(CPACK_PACKAGE_VERSION "1.0.0")
# set(CPACK_PACKAGE_DESCRIPTION "Yea! My first text game")
# set(CPACK_PACKAGE_VENDOR "Wreck Co")
# set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)

# # Указать тип пакета (NSIS для создания установщика .exe)
# #set(CPACK_GENERATOR ZIP)
# set(CPACK_GENERATOR "NSIS64")
# # Указать иконку для установщика (если нужно)
# #set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/1.ico")

# # Установка исполняемого файла
# install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# # Установка дополнительных файлов
# install(DIRECTORY ${CMAKE_SOURCE_DIR}/src DESTINATION resources)
# install(FILES ${CMAKE_SOURCE_DIR}/CMakeLists.txt DESTINATION config)