cmake_minimum_required(VERSION 3.24)

project(OpenEmperor VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OPENEMPEROR_USE_SYSTEM_SDL3 "Use an already installed SDL3 instead of the pinned source dependency" OFF)

if(OPENEMPEROR_USE_SYSTEM_SDL3)
    find_package(SDL3 3.4.10 CONFIG REQUIRED)
else()
    include(FetchContent)
    # SDL 3.4.10 release commit. A commit ID keeps the dependency fixed even if a tag moves.
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG 8e37db5e797b6167f3a00d697d816a684bd259c7
    )
    FetchContent_MakeAvailable(SDL3)
endif()

include(cmake/OpenEmperorWarnings.cmake)

add_executable(openemperor
    src/app/main.cpp
    src/app/Application.cpp
    src/renderer/TitleScreen.cpp
    src/renderer/ImagePreview.cpp
)
target_include_directories(openemperor PRIVATE src)
target_link_libraries(openemperor PRIVATE SDL3::SDL3)
openemperor_enable_warnings(openemperor)

add_executable(openemperor-inspect
    tools/asset-inspector/main.cpp
    tools/asset-inspector/Sg3Inspect.cpp
)
add_library(openemperor-sg3 STATIC
    src/assets/Sg3Archive.cpp
    src/assets/Sg3RgbaDecoder.cpp
    src/assets/Sg3ImageLoader.cpp
    src/assets/RgbaPngEncoder.cpp
    src/assets/RgbaPngReader.cpp
)
target_include_directories(openemperor-sg3 PUBLIC src)
openemperor_enable_warnings(openemperor-sg3)
target_include_directories(openemperor-inspect PRIVATE tools/asset-inspector)
target_link_libraries(openemperor-inspect PRIVATE openemperor-sg3)
openemperor_enable_warnings(openemperor-inspect)
target_link_libraries(openemperor PRIVATE openemperor-sg3)

include(CTest)
if(BUILD_TESTING)
    add_executable(openemperor-sg3-tests tests/Sg3ParserTests.cpp)
    target_link_libraries(openemperor-sg3-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-sg3-tests)
    add_test(NAME sg3-parser-bounds COMMAND openemperor-sg3-tests)

    add_executable(openemperor-sg3-rgba-tests tests/Sg3RgbaTests.cpp)
    target_link_libraries(openemperor-sg3-rgba-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-sg3-rgba-tests)
    add_test(NAME sg3-rgba-decode COMMAND openemperor-sg3-rgba-tests)

    add_executable(openemperor-sg3-loader-tests tests/Sg3ImageLoaderTests.cpp)
    target_link_libraries(openemperor-sg3-loader-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-sg3-loader-tests)
    add_test(NAME sg3-image-loader COMMAND openemperor-sg3-loader-tests)

    add_executable(openemperor-png-preview-tests tests/RgbaPngReaderTests.cpp)
    target_link_libraries(openemperor-png-preview-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-png-preview-tests)
    add_test(NAME png-preview-bounds COMMAND openemperor-png-preview-tests)
endif()
