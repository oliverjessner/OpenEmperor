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

find_package(nlohmann_json 3.12.0 EXACT CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz
        URL_HASH SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(ZLIB REQUIRED)

include(cmake/OpenEmperorWarnings.cmake)

add_executable(openemperor
    src/app/main.cpp
    src/app/Application.cpp
    src/app/AssetBrowser.cpp
    src/app/SceneView.cpp
    src/app/MapDebugView.cpp
    src/app/MapBrowser.cpp
    src/app/MapRenderCheck.cpp
    src/renderer/TitleScreen.cpp
    src/renderer/ImagePreview.cpp
    src/renderer/SceneRenderer.cpp
    src/renderer/TerrainPreviewRenderer.cpp
    src/renderer/StoredGraphicsRenderer.cpp
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
    src/assets/Sg3OmegaDecoder.cpp
    src/assets/Sg3AlphaDecoder.cpp
    src/assets/Sg3AlphaAudit.cpp
    src/assets/Sg3PayloadLayout.cpp
    src/assets/Sg3IsometricDecoder.cpp
    src/assets/Sg3ImageLoader.cpp
    src/assets/AssetCatalog.cpp
    src/assets/RgbaPngEncoder.cpp
    src/assets/RgbaPngReader.cpp
)
target_include_directories(openemperor-sg3 PUBLIC src)
openemperor_enable_warnings(openemperor-sg3)
target_include_directories(openemperor-inspect PRIVATE tools/asset-inspector)
target_link_libraries(openemperor-inspect PRIVATE openemperor-sg3)
openemperor_enable_warnings(openemperor-inspect)
add_library(openemperor-scene STATIC src/scene/IsoProjection.cpp src/scene/Scene.cpp)
target_include_directories(openemperor-scene PUBLIC src)
target_link_libraries(openemperor-scene PUBLIC openemperor-sg3 nlohmann_json::nlohmann_json)
openemperor_enable_warnings(openemperor-scene)

add_library(openemperor-maps STATIC
    src/maps/EmperorContainer.cpp
    src/maps/EmperorMap.cpp
    src/maps/MapGraphicCandidates.cpp
    src/maps/DirectGraphicCandidate.cpp
    src/maps/GraphicsIdHypothesis.cpp
    src/maps/RuntimeArchiveLayout.cpp
    src/maps/ResourceGroupLookup.cpp
    src/maps/StorageGridView.cpp
    src/maps/MapGeometry.cpp
    src/maps/TerrainInterpretation.cpp
    src/maps/MapVisualization.cpp
    src/maps/TerrainBindings.cpp
    src/maps/TerrainRenderPlan.cpp
    src/maps/StoredGraphicsPlan.cpp
    src/maps/MapSubtileMetadata.cpp
    src/maps/MapCatalog.cpp
    src/maps/StoredMapSession.cpp
)
target_include_directories(openemperor-maps PUBLIC src)
target_link_libraries(openemperor-maps PUBLIC ZLIB::ZLIB openemperor-sg3 nlohmann_json::nlohmann_json openemperor-scene)
openemperor_enable_warnings(openemperor-maps)
target_link_libraries(openemperor PRIVATE openemperor-maps)

add_executable(openemperor-map-inspect tools/map-inspector/main.cpp)
target_link_libraries(openemperor-map-inspect PRIVATE openemperor-maps nlohmann_json::nlohmann_json)
openemperor_enable_warnings(openemperor-map-inspect)

add_executable(openemperor-map-graphics tools/map-graphics/main.cpp)
target_link_libraries(openemperor-map-graphics PRIVATE openemperor-maps nlohmann_json::nlohmann_json)
openemperor_enable_warnings(openemperor-map-graphics)

add_executable(openemperor-assets tools/asset-catalog/main.cpp)
target_link_libraries(openemperor-assets PRIVATE openemperor-sg3)
openemperor_enable_warnings(openemperor-assets)

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME map-corpus-runner COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_SOURCE_DIR}/tests -p test_verify_map_corpus.py)
        set_tests_properties(map-corpus-runner PROPERTIES TIMEOUT 20
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
    endif()
    add_executable(openemperor-resource-group-tests tests/ResourceGroupLookupTests.cpp)
    target_link_libraries(openemperor-resource-group-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-resource-group-tests)
    add_test(NAME resource-group-lookup COMMAND openemperor-resource-group-tests)
    add_executable(openemperor-map-semantic-tests tests/MapSemanticTests.cpp)
    target_link_libraries(openemperor-map-semantic-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-map-semantic-tests)
    add_test(NAME emperor-map-semantic-geometry COMMAND openemperor-map-semantic-tests)

    add_executable(openemperor-map-tests tests/EmperorMapTests.cpp)
    target_link_libraries(openemperor-map-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-map-tests)
    add_test(NAME emperor-map-reader COMMAND openemperor-map-tests)

    add_executable(openemperor-map-view-tests tests/MapDebugViewTests.cpp src/app/MapDebugView.cpp src/renderer/TerrainPreviewRenderer.cpp src/renderer/StoredGraphicsRenderer.cpp)
    target_link_libraries(openemperor-map-view-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-map-view-tests)
    add_test(NAME emperor-map-debug-view COMMAND openemperor-map-view-tests)
    add_executable(openemperor-stored-graphics-tests tests/StoredGraphicsTests.cpp
        src/app/MapBrowser.cpp src/app/MapRenderCheck.cpp src/app/MapDebugView.cpp src/renderer/TerrainPreviewRenderer.cpp
        src/renderer/StoredGraphicsRenderer.cpp)
    target_link_libraries(openemperor-stored-graphics-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-stored-graphics-tests)
    add_test(NAME stored-graphics-map-preview COMMAND openemperor-stored-graphics-tests)
    set_tests_properties(stored-graphics-map-preview PROPERTIES TIMEOUT 30)
    add_executable(openemperor-terrain-preview-tests tests/TerrainPreviewTests.cpp src/renderer/TerrainPreviewRenderer.cpp)
    target_link_libraries(openemperor-terrain-preview-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-terrain-preview-tests)
    add_test(NAME terrain-preview-bindings-geometry-render COMMAND openemperor-terrain-preview-tests)
    add_executable(openemperor-scene-tests tests/SceneTests.cpp)
    target_link_libraries(openemperor-scene-tests PRIVATE openemperor-scene)
    openemperor_enable_warnings(openemperor-scene-tests)
    add_test(NAME scene-math-manifest COMMAND openemperor-scene-tests)

    add_executable(openemperor-scene-renderer-tests
        tests/SceneRendererTests.cpp src/renderer/SceneRenderer.cpp src/app/SceneView.cpp)
    target_link_libraries(openemperor-scene-renderer-tests PRIVATE openemperor-scene SDL3::SDL3)
    openemperor_enable_warnings(openemperor-scene-renderer-tests)
    add_test(NAME scene-renderer COMMAND openemperor-scene-renderer-tests)
    add_executable(openemperor-sg3-tests tests/Sg3ParserTests.cpp)
    target_link_libraries(openemperor-sg3-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-sg3-tests)
    add_test(NAME sg3-parser-bounds COMMAND openemperor-sg3-tests)

    add_executable(openemperor-sg3-rgba-tests tests/Sg3RgbaTests.cpp)
    target_link_libraries(openemperor-sg3-rgba-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-sg3-rgba-tests)
    add_test(NAME sg3-rgba-decode COMMAND openemperor-sg3-rgba-tests)

    add_executable(openemperor-sg3-omega-tests tests/Sg3OmegaTests.cpp)
    target_link_libraries(openemperor-sg3-omega-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-sg3-omega-tests)
    add_test(NAME sg3-omega-decode COMMAND openemperor-sg3-omega-tests)

    add_executable(openemperor-sg3-alpha-tests tests/Sg3AlphaTests.cpp)
    target_link_libraries(openemperor-sg3-alpha-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-sg3-alpha-tests)
    add_test(NAME sg3-alpha-decode COMMAND openemperor-sg3-alpha-tests)

    add_executable(openemperor-sg3-alpha-audit-tests tests/Sg3AlphaAuditTests.cpp)
    target_link_libraries(openemperor-sg3-alpha-audit-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-sg3-alpha-audit-tests)
    add_test(NAME sg3-alpha-audit COMMAND openemperor-sg3-alpha-audit-tests)

    add_executable(openemperor-sg3-isometric-tests tests/Sg3IsometricTests.cpp)
    target_link_libraries(openemperor-sg3-isometric-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-sg3-isometric-tests)
    add_test(NAME sg3-isometric-decode COMMAND openemperor-sg3-isometric-tests)

    add_executable(openemperor-sg3-loader-tests tests/Sg3ImageLoaderTests.cpp)
    target_link_libraries(openemperor-sg3-loader-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-sg3-loader-tests)
    add_test(NAME sg3-image-loader COMMAND openemperor-sg3-loader-tests)

    add_executable(openemperor-png-preview-tests tests/RgbaPngReaderTests.cpp)
    target_link_libraries(openemperor-png-preview-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-png-preview-tests)
    add_test(NAME png-preview-bounds COMMAND openemperor-png-preview-tests)

    add_executable(openemperor-asset-catalog-tests
        tests/AssetCatalogTests.cpp
        tools/asset-inspector/Sg3Inspect.cpp)
    target_include_directories(openemperor-asset-catalog-tests PRIVATE tools/asset-inspector)
    target_link_libraries(openemperor-asset-catalog-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-asset-catalog-tests)
    add_test(NAME asset-catalog COMMAND openemperor-asset-catalog-tests)

    add_executable(openemperor-asset-browser-tests
        tests/AssetBrowserTests.cpp
        src/app/AssetBrowser.cpp
    )
    target_link_libraries(openemperor-asset-browser-tests PRIVATE openemperor-sg3 SDL3::SDL3)
    openemperor_enable_warnings(openemperor-asset-browser-tests)
    add_test(NAME asset-browser-cache COMMAND openemperor-asset-browser-tests)
endif()
