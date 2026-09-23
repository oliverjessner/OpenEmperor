cmake_minimum_required(VERSION 3.24)

project(OpenEmperor VERSION 0.1.0 LANGUAGES C CXX)

# CMake's project version remains numeric for platform metadata. This display
# version is the single source used by the executable and release packaging.
set(OPENEMPEROR_DISPLAY_VERSION "0.1.0-alpha.1" CACHE STRING
    "Human-readable OpenEmperor release version")

execute_process(COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE OPENEMPEROR_GIT_RESULT
    OUTPUT_VARIABLE OPENEMPEROR_GIT_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT OPENEMPEROR_GIT_RESULT EQUAL 0)
    set(OPENEMPEROR_GIT_REVISION "unknown")
endif()
execute_process(COMMAND git status --porcelain
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE OPENEMPEROR_GIT_STATUS_RESULT
    OUTPUT_VARIABLE OPENEMPEROR_GIT_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(OPENEMPEROR_GIT_STATUS_RESULT EQUAL 0 AND OPENEMPEROR_GIT_STATUS STREQUAL "")
    set(OPENEMPEROR_GIT_DIRTY false)
else()
    set(OPENEMPEROR_GIT_DIRTY true)
endif()
if(CMAKE_BUILD_TYPE)
    set(OPENEMPEROR_CONFIGURED_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
else()
    set(OPENEMPEROR_CONFIGURED_BUILD_TYPE "multi-config")
endif()
if(APPLE AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    set(OPENEMPEROR_BUILD_TARGET "macOS arm64")
else()
    set(OPENEMPEROR_BUILD_TARGET "${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(CMAKE_OSX_ARCHITECTURES)
    set(OPENEMPEROR_BUILD_ARCHITECTURE "${CMAKE_OSX_ARCHITECTURES}")
else()
    set(OPENEMPEROR_BUILD_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if(CMAKE_OSX_DEPLOYMENT_TARGET)
    set(OPENEMPEROR_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
else()
    set(OPENEMPEROR_DEPLOYMENT_TARGET "unspecified")
endif()
configure_file(src/core/Version.h.in generated/core/Version.h @ONLY)
configure_file(cmake/OpenEmperorBuildInfo.json.in generated/BuildInfo.json @ONLY)
add_library(openemperor-version INTERFACE)
target_include_directories(openemperor-version INTERFACE
    "${CMAKE_CURRENT_BINARY_DIR}/generated")

option(OPENEMPEROR_BUILD_MACOS_APP "Build the opt-in macOS application bundle" OFF)
option(OPENEMPEROR_ENABLE_SANITIZERS
    "Instrument OpenEmperor targets with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(OPENEMPEROR_ENABLE_SANITIZERS AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "OPENEMPEROR_ENABLE_SANITIZERS requires a Clang/GCC-compatible compiler")
endif()
if(OPENEMPEROR_BUILD_MACOS_APP AND NOT APPLE)
    message(FATAL_ERROR "OPENEMPEROR_BUILD_MACOS_APP requires macOS")
endif()
if(OPENEMPEROR_BUILD_MACOS_APP AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    message(FATAL_ERROR "Set CMAKE_OSX_DEPLOYMENT_TARGET explicitly for the macOS app")
endif()

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
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

include(cmake/OpenEmperorWarnings.cmake)

add_executable(openemperor
    src/app/main.cpp
    src/app/Application.cpp
    src/app/AssetBrowser.cpp
    src/app/SceneView.cpp
    src/app/MapDebugView.cpp
    src/app/MapBrowser.cpp
    src/app/MapRenderCheck.cpp
    src/app/SandboxView.cpp
    src/app/WalkerPose.cpp
    src/app/MenuStorage.cpp src/app/MenuDialog.cpp src/app/MenuSession.cpp
    src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
    src/app/SandboxCheck.cpp
    src/app/SandboxRoutingCheck.cpp
    src/app/MenuCheck.cpp
    src/app/ResourceLocator.cpp
    src/app/VisualSelection.cpp
    src/renderer/TitleScreen.cpp
    src/renderer/ImagePreview.cpp
    src/renderer/SceneRenderer.cpp
    src/renderer/TerrainPreviewRenderer.cpp
    src/renderer/StoredGraphicsRenderer.cpp
    src/renderer/StoredCamera.cpp
    src/renderer/WalkerSpriteSet.cpp
    src/renderer/BuildingSprite.cpp
    src/renderer/RoadSpriteSet.cpp
)
target_include_directories(openemperor PRIVATE src)
target_link_libraries(openemperor PRIVATE SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor)
set(OPENEMPEROR_COMPATIBILITY_PACK_ID "gog-derived-2.0.0.2-en-assetset-1")
set(OPENEMPEROR_COMPATIBILITY_FILES
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/manifest.json"
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/walkers.json"
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/buildings.json"
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/roads.json")
if(OPENEMPEROR_BUILD_MACOS_APP)
    set_source_files_properties(${OPENEMPEROR_COMPATIBILITY_FILES} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources/Compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}")
    target_sources(openemperor PRIVATE ${OPENEMPEROR_COMPATIBILITY_FILES})
    set_target_properties(openemperor PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME OpenEmperor
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/OpenEmperorInfo.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.openemperor.OpenEmperor"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    install(TARGETS openemperor BUNDLE DESTINATION .)
    set(OPENEMPEROR_TEST_RESOURCE_ROOT "$<TARGET_BUNDLE_CONTENT_DIR:openemperor>/Resources")
else()
    add_custom_command(TARGET openemperor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:openemperor>/resources/Compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENEMPEROR_COMPATIBILITY_FILES}
            "$<TARGET_FILE_DIR:openemperor>/resources/Compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}"
        VERBATIM)
    set(OPENEMPEROR_TEST_RESOURCE_ROOT "$<TARGET_FILE_DIR:openemperor>/resources")
endif()

add_executable(openemperor-inspect
    tools/asset-inspector/main.cpp
    tools/asset-inspector/Sg3Inspect.cpp
    tools/asset-inspector/Sg3RedAudit.cpp
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
    src/assets/Sg3ShadowComposition.cpp
    src/assets/AssetCatalog.cpp
    src/assets/WalkerVisualProfile.cpp
    src/assets/BuildingVisualProfile.cpp
    src/assets/RoadVisualProfile.cpp
    src/assets/CompatibilityProfile.cpp
    src/assets/RgbaPngEncoder.cpp
    src/assets/RgbaPngReader.cpp
)
target_include_directories(openemperor-sg3 PUBLIC src)
target_link_libraries(openemperor-sg3 PUBLIC nlohmann_json::nlohmann_json OpenSSL::Crypto)
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
    src/maps/StoredArchiveRegistrations.cpp
    src/maps/MapSubtileMetadata.cpp
    src/maps/MapCatalog.cpp
    src/maps/StoredMapSession.cpp
    src/maps/SandboxPlacement.cpp
)
target_include_directories(openemperor-maps PUBLIC src)
target_link_libraries(openemperor-maps PUBLIC ZLIB::ZLIB openemperor-sg3 nlohmann_json::nlohmann_json openemperor-scene)
openemperor_enable_warnings(openemperor-maps)
add_library(openemperor-simulation STATIC src/simulation/World.cpp)
target_include_directories(openemperor-simulation PUBLIC src)
openemperor_enable_warnings(openemperor-simulation)
add_library(openemperor-persistence STATIC src/persistence/SandboxSave.cpp
    src/platform/AtomicReplace.cpp)
target_include_directories(openemperor-persistence PUBLIC src)
target_link_libraries(openemperor-persistence PUBLIC openemperor-maps openemperor-simulation
    nlohmann_json::nlohmann_json OpenSSL::Crypto)
openemperor_enable_warnings(openemperor-persistence)
target_link_libraries(openemperor PRIVATE openemperor-persistence)

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
    add_executable(openemperor-sandbox-simulation-tests tests/SandboxSimulationTests.cpp)
    target_link_libraries(openemperor-sandbox-simulation-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-sandbox-simulation-tests)
    add_test(NAME sandbox-simulation COMMAND openemperor-sandbox-simulation-tests)
    add_executable(openemperor-sandbox-production-tests tests/SandboxProductionTests.cpp)
    target_link_libraries(openemperor-sandbox-production-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-sandbox-production-tests)
    add_test(NAME sandbox-production-chain COMMAND openemperor-sandbox-production-tests)
    set_tests_properties(sandbox-production-chain PROPERTIES TIMEOUT 20)
    add_executable(openemperor-sandbox-view-tests tests/SandboxViewTests.cpp
        src/app/SandboxView.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
        src/app/VisualSelection.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp)
    target_sources(openemperor-sandbox-view-tests PRIVATE src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp)
    target_link_libraries(openemperor-sandbox-view-tests PRIVATE openemperor-persistence SDL3::SDL3
        openemperor-version)
    openemperor_enable_warnings(openemperor-sandbox-view-tests)
    add_test(NAME sandbox-software-view COMMAND openemperor-sandbox-view-tests)
    set_tests_properties(sandbox-software-view PROPERTIES TIMEOUT 45)
    add_executable(openemperor-menu-session-tests tests/MenuSessionTests.cpp
        src/app/MenuSession.cpp src/app/MenuStorage.cpp src/app/MenuDialog.cpp
        src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
        src/app/SandboxView.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp)
    target_sources(openemperor-menu-session-tests PRIVATE src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp)
    target_link_libraries(openemperor-menu-session-tests PRIVATE openemperor-persistence SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-menu-session-tests)
    add_test(NAME menu-session-flow COMMAND openemperor-menu-session-tests)
    set_tests_properties(menu-session-flow PROPERTIES TIMEOUT 30)
    add_executable(openemperor-sandbox-save-tests tests/SandboxSaveTests.cpp)
    target_link_libraries(openemperor-sandbox-save-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-save-tests)
    add_test(NAME sandbox-save-resume COMMAND openemperor-sandbox-save-tests)
    set_tests_properties(sandbox-save-resume PROPERTIES TIMEOUT 30)
    add_executable(openemperor-sandbox-routing-tests tests/SandboxRoutingTests.cpp)
    target_link_libraries(openemperor-sandbox-routing-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-routing-tests)
    add_test(NAME sandbox-live-rerouting COMMAND openemperor-sandbox-routing-tests)
    set_tests_properties(sandbox-live-rerouting PROPERTIES TIMEOUT 30)
    add_executable(openemperor-sandbox-household-tests tests/SandboxHouseholdTests.cpp)
    target_link_libraries(openemperor-sandbox-household-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-household-tests)
    add_test(NAME sandbox-household-supply COMMAND openemperor-sandbox-household-tests)
    set_tests_properties(sandbox-household-supply PROPERTIES TIMEOUT 30)
    add_executable(openemperor-sandbox-settlement-tests tests/SandboxSettlementTests.cpp)
    target_link_libraries(openemperor-sandbox-settlement-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-settlement-tests)
    add_test(NAME sandbox-settlement-distribution COMMAND openemperor-sandbox-settlement-tests)
    set_tests_properties(sandbox-settlement-distribution PROPERTIES TIMEOUT 30)
    add_executable(openemperor-sandbox-industry-tests tests/SandboxIndustryTests.cpp)
    target_link_libraries(openemperor-sandbox-industry-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-industry-tests)
    add_test(NAME sandbox-industry-production COMMAND openemperor-sandbox-industry-tests)
    set_tests_properties(sandbox-industry-production PROPERTIES TIMEOUT 30)
    add_executable(openemperor-sandbox-city-tests tests/SandboxCityTests.cpp)
    target_link_libraries(openemperor-sandbox-city-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-city-tests)
    add_test(NAME sandbox-city-economy COMMAND openemperor-sandbox-city-tests)
    set_tests_properties(sandbox-city-economy PROPERTIES TIMEOUT 30)
    add_executable(openemperor-sandbox-courier-triage-tests
        tests/SandboxCourierTriageTests.cpp)
    target_link_libraries(openemperor-sandbox-courier-triage-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-sandbox-courier-triage-tests)
    add_test(NAME sandbox-courier-triage COMMAND openemperor-sandbox-courier-triage-tests)
    set_tests_properties(sandbox-courier-triage PROPERTIES TIMEOUT 20)
    # Deliberately not registered with CTest: this 100k-tick executable belongs to
    # tools/alpha_check.py so the normal regression suite remains quick.
    add_executable(openemperor-alpha-endurance tests/AlphaEnduranceTests.cpp)
    target_link_libraries(openemperor-alpha-endurance PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-alpha-endurance)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME map-corpus-runner COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_SOURCE_DIR}/tests -p test_verify_map_corpus.py)
        set_tests_properties(map-corpus-runner PROPERTIES TIMEOUT 20
            RUN_SERIAL TRUE
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

    add_executable(openemperor-map-view-tests tests/MapDebugViewTests.cpp src/app/MapDebugView.cpp src/renderer/TerrainPreviewRenderer.cpp src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
    target_link_libraries(openemperor-map-view-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-map-view-tests)
    add_test(NAME emperor-map-debug-view COMMAND openemperor-map-view-tests)
    add_executable(openemperor-stored-graphics-tests tests/StoredGraphicsTests.cpp
        src/app/MapBrowser.cpp src/app/MapRenderCheck.cpp src/app/MapDebugView.cpp src/renderer/TerrainPreviewRenderer.cpp
        src/renderer/StoredGraphicsRenderer.cpp)
    target_sources(openemperor-stored-graphics-tests PRIVATE src/renderer/StoredCamera.cpp)
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
    add_executable(openemperor-walker-visual-tests tests/WalkerVisualTests.cpp
        src/app/WalkerPose.cpp src/renderer/WalkerSpriteSet.cpp)
    target_link_libraries(openemperor-walker-visual-tests PRIVATE
        openemperor-sg3 openemperor-simulation SDL3::SDL3)
    openemperor_enable_warnings(openemperor-walker-visual-tests)
    add_test(NAME walker-visual-profile-pose-render COMMAND openemperor-walker-visual-tests)
    set_tests_properties(walker-visual-profile-pose-render PROPERTIES TIMEOUT 30)

    add_executable(openemperor-building-visual-tests tests/BuildingVisualTests.cpp
        src/renderer/BuildingSprite.cpp)
    target_link_libraries(openemperor-building-visual-tests PRIVATE openemperor-sg3 SDL3::SDL3)
    openemperor_enable_warnings(openemperor-building-visual-tests)
    add_test(NAME building-visual-profile-depth COMMAND openemperor-building-visual-tests)

    add_executable(openemperor-road-topology-tests tests/RoadTopologyTests.cpp
        src/app/RoadTopology.cpp src/app/RoadDrag.cpp)
    target_link_libraries(openemperor-road-topology-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-road-topology-tests)
    add_test(NAME road-topology-live-preview COMMAND openemperor-road-topology-tests)

    add_executable(openemperor-road-visual-tests tests/RoadVisualTests.cpp
        src/renderer/RoadSpriteSet.cpp)
    target_link_libraries(openemperor-road-visual-tests PRIVATE openemperor-sg3 SDL3::SDL3)
    openemperor_enable_warnings(openemperor-road-visual-tests)
    add_test(NAME road-visual-profile-pixels COMMAND openemperor-road-visual-tests)
    add_executable(openemperor-compatibility-tests tests/CompatibilityProfileTests.cpp
        src/app/VisualSelection.cpp)
    target_link_libraries(openemperor-compatibility-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-compatibility-tests)
    add_test(NAME compatibility-profile-fingerprints COMMAND openemperor-compatibility-tests
        "${OPENEMPEROR_TEST_RESOURCE_ROOT}")
    add_executable(openemperor-unified-painter-tests tests/UnifiedPainterTests.cpp
        src/renderer/StoredGraphicsRenderer.cpp)
    target_link_libraries(openemperor-unified-painter-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-unified-painter-tests)
    add_test(NAME unified-map-sandbox-pixels COMMAND openemperor-unified-painter-tests)

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

    add_executable(openemperor-version-tests tests/VersionTests.cpp)
    target_link_libraries(openemperor-version-tests PRIVATE openemperor-version)
    openemperor_enable_warnings(openemperor-version-tests)
    add_test(NAME release-version COMMAND openemperor-version-tests)
endif()
