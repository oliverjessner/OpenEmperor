cmake_minimum_required(VERSION 3.24)

project(OpenEmperor VERSION 0.1.0 LANGUAGES C CXX)

# CMake's project version remains numeric for platform metadata. This display
# version is the single source used by the executable and release packaging.
set(OPENEMPEROR_DISPLAY_VERSION "0.1.0-alpha.2")

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
find_package(Threads REQUIRED)

include(cmake/OpenEmperorWarnings.cmake)

add_executable(openemperor
    src/app/main.cpp
    src/app/Application.cpp
    src/app/InputDiagnostics.cpp
    src/app/AssetBrowser.cpp
    src/app/SceneView.cpp
    src/app/MapDebugView.cpp
    src/app/MapBrowser.cpp
    src/app/MapRenderCheck.cpp
    src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp
    src/app/WalkerPose.cpp
    src/app/MenuStorage.cpp src/app/MenuDialog.cpp src/app/MenuSession.cpp src/app/MapRulesPreflight.cpp
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
    src/renderer/WellFallbackRenderer.cpp
    src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp
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
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/original-houses.json"
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/original-buildings.json"
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/roads.json"
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/fire.json"
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/fire-inspector.json"
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/market-walkers.json"
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/service-walker.json"
    "${CMAKE_SOURCE_DIR}/resources/compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}/health-walker.json")
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
    # Refresh the CLI's resource copy when only presentation metadata changes.
    set_property(TARGET openemperor APPEND PROPERTY LINK_DEPENDS ${OPENEMPEROR_COMPATIBILITY_FILES})
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
    # Shared archive registration/index logic is also required by optional
    # asset profiles. Keep one implementation, below the map preparation layer.
    src/maps/RuntimeArchiveLayout.cpp
    src/maps/ResourceGroupLookup.cpp
    src/assets/Sg3Archive.cpp
    src/assets/Sg3RgbaDecoder.cpp
    src/assets/Sg3OmegaDecoder.cpp
    src/assets/Sg3AlphaDecoder.cpp
    src/assets/Sg3AlphaAudit.cpp
    src/assets/Sg3PayloadLayout.cpp
    src/assets/Sg3IsometricDecoder.cpp
    src/assets/Sg3ImageLoader.cpp
    src/assets/GreatWallDependencyPaths.cpp
    src/assets/Sg3ShadowComposition.cpp
    src/assets/AssetCatalog.cpp
    src/assets/WalkerVisualProfile.cpp
    src/assets/FireVisualProfile.cpp
    src/assets/BuildingVisualProfile.cpp
    src/assets/OriginalHouseVisualProfile.cpp
    src/assets/OriginalBuildingVisualProfile.cpp
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
    src/maps/StorageGridView.cpp
    src/maps/MapGeometry.cpp
    src/maps/TerrainInterpretation.cpp
    src/maps/MapVisualization.cpp
    src/maps/TerrainBindings.cpp
    src/maps/TerrainRenderPlan.cpp
    src/maps/LandscapeProvenance.cpp
    src/maps/LandscapeSelectors.cpp
    src/maps/LandscapeInstances.cpp
    src/maps/WallTopology.cpp
    src/maps/PinnacleSelector.cpp
    src/maps/RegeneratedMapRenderPlan.cpp
    src/maps/OriginalMapEntities.cpp
    src/maps/OrdinaryGateMapPresentation.cpp
    src/maps/OrdinaryTowerMapPresentation.cpp
    src/maps/GreatWallModels.cpp
    src/maps/GreatWallSelector.cpp
    src/maps/GreatWallMapPresentation.cpp
    src/maps/StoredGraphicsPlan.cpp
    src/maps/StoredArchiveRegistrations.cpp
    src/maps/MapSubtileMetadata.cpp
    src/maps/MapCatalog.cpp
    src/maps/StoredMapSession.cpp
    src/maps/SandboxPlacement.cpp
    src/maps/MapRulesCheck.cpp
)
target_include_directories(openemperor-maps PUBLIC src)
target_link_libraries(openemperor-maps PUBLIC ZLIB::ZLIB openemperor-sg3 nlohmann_json::nlohmann_json openemperor-scene)
openemperor_enable_warnings(openemperor-maps)
add_library(openemperor-simulation STATIC
    src/simulation/MapPermissions.cpp
    src/simulation/RoadConnectivity.cpp
    src/simulation/World.cpp
    src/simulation/WorldBridge.cpp
    src/simulation/WorldSourceCellRamp.cpp
    src/simulation/WorldLandTrade.cpp
    src/simulation/WorldRamp.cpp
    src/simulation/WorldFire.cpp
    src/simulation/WorldHealth.cpp
    src/simulation/WorldMaintenance.cpp
    src/simulation/WorldDesirability.cpp
    src/simulation/WorldOriginalInfluence.cpp
    src/simulation/WorldOriginalGoods.cpp
    src/simulation/WorldOriginalProduction.cpp
    src/simulation/WorldOriginalDemolition.cpp
    src/simulation/WorldOriginalFinance.cpp
    src/simulation/WorldOriginalMarket.cpp
    src/simulation/OriginalMarketPolicy.cpp
    src/simulation/WorldOriginalHemp.cpp
    src/simulation/WorldOriginalFood.cpp
    src/simulation/WorldWater.cpp
    src/simulation/CityStartGuidance.cpp)
target_sources(openemperor-simulation PRIVATE src/simulation/OriginalEconomyStarter.cpp
    src/simulation/OriginalTerrainFacts.cpp)
target_include_directories(openemperor-simulation PUBLIC src)
openemperor_enable_warnings(openemperor-simulation)
# Read-only model parser shared by bounded preparation and source fingerprints.
add_library(openemperor-original-model STATIC
    src/simulation/original/ModelData.cpp src/simulation/original/ModelSchema.cpp)
target_include_directories(openemperor-original-model PUBLIC src)
target_link_libraries(openemperor-original-model PRIVATE OpenSSL::Crypto)
openemperor_enable_warnings(openemperor-original-model)
add_executable(openemperor-model-inspect tools/model-inspector/main.cpp)
target_link_libraries(openemperor-model-inspect PRIVATE
    openemperor-original-model nlohmann_json::nlohmann_json)
openemperor_enable_warnings(openemperor-model-inspect)
# Numerical house rules; Rule7 preparation validates immutable model values.
# Active evolution/needs processing remains a later World contract.
add_library(openemperor-original-house-evolution STATIC
    src/simulation/original/HouseEvolution.cpp)
target_include_directories(openemperor-original-house-evolution PUBLIC src)
target_link_libraries(openemperor-original-house-evolution PUBLIC openemperor-original-model)
openemperor_enable_warnings(openemperor-original-house-evolution)
add_library(openemperor-original-rules-model STATIC
    src/simulation/original/OriginalRulesModel.cpp)
target_include_directories(openemperor-original-rules-model PUBLIC src)
target_link_libraries(openemperor-original-rules-model PUBLIC openemperor-original-house-evolution)
openemperor_enable_warnings(openemperor-original-rules-model)
target_link_libraries(openemperor-simulation PUBLIC openemperor-original-rules-model openemperor-original-desirability openemperor-original-goods-ledger)
# Pure visit/countdown/spawn rules; Rule10 binds actual arrival/phase35 only.
add_library(openemperor-original-service-rules STATIC
    src/simulation/original/ServiceRules.cpp)
target_include_directories(openemperor-original-service-rules PUBLIC src)
target_link_libraries(openemperor-original-service-rules PUBLIC openemperor-original-house-evolution)
openemperor_enable_warnings(openemperor-original-service-rules)
target_sources(openemperor-simulation PRIVATE src/simulation/WorldOriginalIndustry.cpp
    src/simulation/WorldOriginalConsumption.cpp src/simulation/WorldOriginalHousingDiagnosis.cpp
    src/simulation/WorldOriginalServices.cpp
    src/simulation/WorldOriginalWater.cpp src/simulation/WorldOriginalHousing.cpp)
target_link_libraries(openemperor-simulation PUBLIC openemperor-original-service-rules)
# Pure goods transfers/consumption/quality observations; no World/app linkage.
add_library(openemperor-original-goods-rules STATIC
    src/simulation/original/GoodsRules.cpp)
target_include_directories(openemperor-original-goods-rules PUBLIC src)
target_link_libraries(openemperor-original-goods-rules PUBLIC openemperor-original-house-evolution)
openemperor_enable_warnings(openemperor-original-goods-rules)
# Authored transaction accounting; no World, movement or source-generation API.
add_library(openemperor-original-goods-ledger STATIC src/simulation/OriginalGoodsLedger.cpp)
target_include_directories(openemperor-original-goods-ledger PUBLIC src)
target_link_libraries(openemperor-original-goods-ledger PUBLIC openemperor-original-goods-rules)
openemperor_enable_warnings(openemperor-original-goods-ledger)
# Independent preparation kernel; no production World or app link.
add_library(openemperor-original-desirability STATIC
    src/simulation/original/Desirability.cpp)
target_include_directories(openemperor-original-desirability PUBLIC src)
target_link_libraries(openemperor-original-desirability PUBLIC openemperor-original-model)
openemperor_enable_warnings(openemperor-original-desirability)
add_executable(openemperor-desirability-compare tools/desirability-compare/main.cpp)
target_link_libraries(openemperor-desirability-compare PRIVATE
    openemperor-simulation nlohmann_json::nlohmann_json)
target_compile_definitions(openemperor-desirability-compare PRIVATE
    OPENEMPEROR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
openemperor_enable_warnings(openemperor-desirability-compare)
target_link_libraries(openemperor-maps PUBLIC openemperor-simulation)
add_library(openemperor-persistence STATIC src/persistence/SandboxSave.cpp
    src/platform/AtomicReplace.cpp)
target_include_directories(openemperor-persistence PUBLIC src)
target_link_libraries(openemperor-persistence PUBLIC openemperor-maps openemperor-simulation
    nlohmann_json::nlohmann_json OpenSSL::Crypto openemperor-original-house-evolution)
openemperor_enable_warnings(openemperor-persistence)
add_library(openemperor-recovery STATIC
    src/persistence/RecoveryStore.cpp
    src/app/AutosaveController.cpp)
target_include_directories(openemperor-recovery PUBLIC src)
target_link_libraries(openemperor-recovery PUBLIC openemperor-persistence)
openemperor_enable_warnings(openemperor-recovery)
target_link_libraries(openemperor PRIVATE openemperor-recovery Threads::Threads)

add_executable(openemperor-original-starter-acceptance EXCLUDE_FROM_ALL
    tools/OriginalStarterAcceptance.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
target_link_libraries(openemperor-original-starter-acceptance PRIVATE
    openemperor-persistence SDL3::SDL3)
openemperor_enable_warnings(openemperor-original-starter-acceptance)

add_executable(openemperor-original-recovery-acceptance EXCLUDE_FROM_ALL
    tools/OriginalRecoveryAcceptance.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
target_link_libraries(openemperor-original-recovery-acceptance PRIVATE
    openemperor-recovery SDL3::SDL3)
openemperor_enable_warnings(openemperor-original-recovery-acceptance)

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
    add_executable(openemperor-original-starter-tests tests/OriginalStarterTests.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
    target_link_libraries(openemperor-original-starter-tests PRIVATE
        openemperor-persistence SDL3::SDL3)
    openemperor_enable_warnings(openemperor-original-starter-tests)
    add_test(NAME original-paid-starter-synthetic COMMAND openemperor-original-starter-tests --synthetic)
    add_test(NAME original-paid-starter-real COMMAND openemperor-original-starter-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-paid-starter-synthetic original-paid-starter-real PROPERTIES TIMEOUT 180)
endif()
if(BUILD_TESTING)
    add_executable(openemperor-original-save-adversarial-tests tests/OriginalSaveAdversarialTests.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
    target_link_libraries(openemperor-original-save-adversarial-tests PRIVATE
        openemperor-persistence SDL3::SDL3)
    openemperor_enable_warnings(openemperor-original-save-adversarial-tests)
    add_test(NAME original-save-adversarial-synthetic COMMAND openemperor-original-save-adversarial-tests)
    add_test(NAME original-save-adversarial-real COMMAND openemperor-original-save-adversarial-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-save-adversarial-synthetic original-save-adversarial-real PROPERTIES TIMEOUT 240)
    add_executable(openemperor-original-model-data-tests tests/OriginalModelDataTests.cpp)
    target_link_libraries(openemperor-original-model-data-tests PRIVATE
        openemperor-original-model OpenSSL::Crypto)
    openemperor_enable_warnings(openemperor-original-model-data-tests)
    add_test(NAME original-model-data-synthetic COMMAND openemperor-original-model-data-tests)
    set(OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT "${CMAKE_SOURCE_DIR}/.local/gog-extracted/app"
        CACHE PATH "Optional read-only original installation for ModelData CTest")
    add_test(NAME original-model-data-real COMMAND openemperor-original-model-data-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-model-data-synthetic original-model-data-real
        PROPERTIES TIMEOUT 30)
    set_tests_properties(original-model-data-real PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(openemperor-original-model-schema-tests tests/OriginalModelSchemaTests.cpp)
    target_link_libraries(openemperor-original-model-schema-tests PRIVATE
        openemperor-original-model OpenSSL::Crypto)
    openemperor_enable_warnings(openemperor-original-model-schema-tests)
    add_test(NAME original-model-schema-synthetic COMMAND openemperor-original-model-schema-tests)
    add_test(NAME original-model-schema-real COMMAND openemperor-original-model-schema-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-model-schema-synthetic original-model-schema-real
        PROPERTIES TIMEOUT 30)
    set_tests_properties(original-model-schema-real PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(openemperor-original-rules-model-tests tests/OriginalRulesModelTests.cpp)
    target_link_libraries(openemperor-original-rules-model-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-rules-model-tests)
    add_test(NAME original-rules-model-synthetic COMMAND openemperor-original-rules-model-tests)
    add_test(NAME original-rules-model-real COMMAND openemperor-original-rules-model-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-rules-model-synthetic original-rules-model-real PROPERTIES TIMEOUT 60)
    set_tests_properties(original-rules-model-real PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(openemperor-original-production-model-tests tests/OriginalProductionModelTests.cpp)
    target_link_libraries(openemperor-original-production-model-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-production-model-tests)
    add_test(NAME original-production-model-synthetic COMMAND openemperor-original-production-model-tests)
    add_test(NAME original-production-model-real COMMAND openemperor-original-production-model-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-production-model-synthetic original-production-model-real PROPERTIES TIMEOUT 60)
    set_tests_properties(original-production-model-real PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(openemperor-original-desirability-tests tests/OriginalDesirabilityTests.cpp)
    target_link_libraries(openemperor-original-desirability-tests PRIVATE
        openemperor-original-desirability OpenSSL::Crypto)
    openemperor_enable_warnings(openemperor-original-desirability-tests)
    add_test(NAME original-desirability-synthetic COMMAND openemperor-original-desirability-tests)
    add_test(NAME original-desirability-real COMMAND openemperor-original-desirability-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-desirability-synthetic original-desirability-real
        PROPERTIES TIMEOUT 30)
    set_tests_properties(original-desirability-real PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(openemperor-original-house-evolution-tests tests/OriginalHouseEvolutionTests.cpp)
    target_link_libraries(openemperor-original-house-evolution-tests PRIVATE
        openemperor-original-house-evolution OpenSSL::Crypto)
    openemperor_enable_warnings(openemperor-original-house-evolution-tests)
    add_test(NAME original-house-evolution-synthetic COMMAND openemperor-original-house-evolution-tests)
    add_test(NAME original-house-evolution-real COMMAND openemperor-original-house-evolution-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-house-evolution-synthetic original-house-evolution-real
        PROPERTIES TIMEOUT 30)
    set_tests_properties(original-house-evolution-real PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(openemperor-original-service-rules-tests tests/OriginalServiceRulesTests.cpp)
    target_link_libraries(openemperor-original-service-rules-tests PRIVATE
        openemperor-original-service-rules OpenSSL::Crypto)
    openemperor_enable_warnings(openemperor-original-service-rules-tests)
    add_test(NAME original-service-rules-synthetic COMMAND openemperor-original-service-rules-tests)
    add_test(NAME original-service-rules-real COMMAND openemperor-original-service-rules-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-service-rules-synthetic original-service-rules-real
        PROPERTIES TIMEOUT 30)
    set_tests_properties(original-service-rules-real PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(openemperor-original-goods-rules-tests tests/OriginalGoodsRulesTests.cpp)
    target_link_libraries(openemperor-original-goods-rules-tests PRIVATE
        openemperor-original-goods-rules OpenSSL::Crypto)
    openemperor_enable_warnings(openemperor-original-goods-rules-tests)
    add_test(NAME original-goods-rules-synthetic COMMAND openemperor-original-goods-rules-tests)
    add_test(NAME original-goods-rules-real COMMAND openemperor-original-goods-rules-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-goods-rules-synthetic original-goods-rules-real
        PROPERTIES TIMEOUT 30)
    set_tests_properties(original-goods-rules-real PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(openemperor-great-wall-dependency-paths-tests tests/GreatWallDependencyPathsTests.cpp)
    target_link_libraries(openemperor-great-wall-dependency-paths-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-great-wall-dependency-paths-tests)
    add_test(NAME great-wall-dependency-paths COMMAND openemperor-great-wall-dependency-paths-tests)
    foreach(test_name IN ITEMS OriginalMapEntities GreatWallModels GreatWallSelector GreatWallPresentation)
        string(TOLOWER ${test_name} test_target)
        add_executable(openemperor-${test_target}-tests tests/${test_name}Tests.cpp)
        target_link_libraries(openemperor-${test_target}-tests PRIVATE openemperor-maps)
        openemperor_enable_warnings(openemperor-${test_target}-tests)
        add_test(NAME ${test_target} COMMAND openemperor-${test_target}-tests)
    endforeach()
    add_executable(openemperor-landscape-selector-tests tests/LandscapeSelectorTests.cpp)
    target_link_libraries(openemperor-landscape-selector-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-landscape-selector-tests)
    add_test(NAME landscape-selectors COMMAND openemperor-landscape-selector-tests)
    add_executable(openemperor-pinnacle-selector-tests tests/PinnacleSelectorTests.cpp)
    target_link_libraries(openemperor-pinnacle-selector-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-pinnacle-selector-tests)
    add_test(NAME pinnacle-selectors COMMAND openemperor-pinnacle-selector-tests)
    add_executable(openemperor-wall-topology-tests tests/WallTopologyTests.cpp)
    target_link_libraries(openemperor-wall-topology-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-wall-topology-tests)
    add_test(NAME wall-topology COMMAND openemperor-wall-topology-tests)
    add_executable(openemperor-landscape-instance-tests tests/LandscapeInstanceTests.cpp)
    target_link_libraries(openemperor-landscape-instance-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-landscape-instance-tests)
    add_test(NAME landscape-instances COMMAND openemperor-landscape-instance-tests)
    add_executable(openemperor-sandbox-simulation-tests tests/SandboxSimulationTests.cpp)
    target_link_libraries(openemperor-sandbox-simulation-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-sandbox-simulation-tests)
    add_test(NAME sandbox-simulation COMMAND openemperor-sandbox-simulation-tests)
    add_executable(openemperor-sandbox-production-tests tests/SandboxProductionTests.cpp)
    target_link_libraries(openemperor-sandbox-production-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-sandbox-production-tests)
    add_test(NAME sandbox-production-chain COMMAND openemperor-sandbox-production-tests)
    set_tests_properties(sandbox-production-chain PROPERTIES TIMEOUT 20)
    add_executable(openemperor-health-fallback-tests tests/HealthPostFallbackRendererTests.cpp
        src/renderer/HealthPostFallbackRenderer.cpp)
    target_include_directories(openemperor-health-fallback-tests PRIVATE src)
    target_link_libraries(openemperor-health-fallback-tests PRIVATE SDL3::SDL3)
    openemperor_enable_warnings(openemperor-health-fallback-tests)
    add_test(NAME health-post-fallback-presentation COMMAND openemperor-health-fallback-tests)
    add_executable(openemperor-well-fallback-tests tests/WellFallbackRendererTests.cpp
        src/renderer/WellFallbackRenderer.cpp)
    target_include_directories(openemperor-well-fallback-tests PRIVATE src)
    target_link_libraries(openemperor-well-fallback-tests PRIVATE SDL3::SDL3)
    openemperor_enable_warnings(openemperor-well-fallback-tests)
    add_test(NAME well-fallback-pixels COMMAND openemperor-well-fallback-tests)
    set_tests_properties(well-fallback-pixels PROPERTIES TIMEOUT 20)
    add_executable(openemperor-original-elite-house-fallback-tests tests/OriginalEliteHouseFallbackRendererTests.cpp
        src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_include_directories(openemperor-original-elite-house-fallback-tests PRIVATE src)
    target_link_libraries(openemperor-original-elite-house-fallback-tests PRIVATE SDL3::SDL3)
    openemperor_enable_warnings(openemperor-original-elite-house-fallback-tests)
    add_test(NAME original-elite-house-fallback COMMAND openemperor-original-elite-house-fallback-tests)
    set_tests_properties(original-elite-house-fallback PROPERTIES TIMEOUT 20)
    add_executable(openemperor-inspector-layout-tests tests/InspectorLayoutTests.cpp
        src/app/SandboxUiLayout.cpp)
    target_include_directories(openemperor-inspector-layout-tests PRIVATE src)
    openemperor_enable_warnings(openemperor-inspector-layout-tests)
    add_test(NAME inspector-layout COMMAND openemperor-inspector-layout-tests)
    set_tests_properties(inspector-layout PROPERTIES TIMEOUT 20)
    add_executable(openemperor-sandbox-view-tests tests/SandboxViewTests.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
        src/app/VisualSelection.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp)
    target_sources(openemperor-sandbox-view-tests PRIVATE src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-sandbox-view-tests PRIVATE openemperor-recovery SDL3::SDL3
        openemperor-version)
    openemperor_enable_warnings(openemperor-sandbox-view-tests)
    add_executable(openemperor-original-economy-save-view-tests tests/OriginalEconomySaveViewTests.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
        src/app/VisualSelection.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp)
    target_sources(openemperor-original-economy-save-view-tests PRIVATE src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-original-economy-save-view-tests PRIVATE openemperor-recovery SDL3::SDL3
        openemperor-version)
    openemperor_enable_warnings(openemperor-original-economy-save-view-tests)
    target_compile_definitions(openemperor-original-economy-save-view-tests PRIVATE
        OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE=1)
    add_test(NAME original-economy-save-view COMMAND openemperor-original-economy-save-view-tests)
    set_tests_properties(original-economy-save-view PROPERTIES TIMEOUT 60)

    add_executable(openemperor-sandbox-gate-budget-view-tests
        tests/SandboxGateBudgetViewTests.cpp tests/GateBudgetPreflightShim.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp
        src/app/RoadDrag.cpp src/app/RoadTopology.cpp src/app/VisualSelection.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp
        src/renderer/RoadSpriteSet.cpp src/renderer/WellFallbackRenderer.cpp
        src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-sandbox-gate-budget-view-tests PRIVATE
        openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-sandbox-gate-budget-view-tests)
    add_test(NAME sandbox-gate-budget-view COMMAND openemperor-sandbox-gate-budget-view-tests)
    set_tests_properties(sandbox-gate-budget-view PROPERTIES TIMEOUT 60)
    add_test(NAME sandbox-city-v15-view COMMAND openemperor-sandbox-view-tests --health-only)
    add_test(NAME sandbox-service-coverage-view COMMAND openemperor-sandbox-view-tests --service-only)
    set_tests_properties(sandbox-service-coverage-view PROPERTIES TIMEOUT 60)
    add_test(NAME sandbox-road-connectivity-view COMMAND openemperor-sandbox-view-tests --connectivity-only)
    add_test(NAME sandbox-original-roads-view COMMAND openemperor-sandbox-view-tests --original-roads-only)
    add_test(NAME sandbox-bridges-view COMMAND openemperor-sandbox-view-tests --bridges-only)
    set_tests_properties(sandbox-road-connectivity-view PROPERTIES TIMEOUT 60)
    add_test(NAME road-continuity-production-pixels COMMAND openemperor-sandbox-view-tests --road-continuity-only)
    set_tests_properties(road-continuity-production-pixels PROPERTIES TIMEOUT 60)
    add_test(NAME sandbox-city-v16-geometry-view COMMAND openemperor-sandbox-view-tests --geometry16-only)
    add_test(NAME sandbox-city-v16-view COMMAND openemperor-sandbox-view-tests --maintenance-only)
    set_tests_properties(sandbox-city-v15-view PROPERTIES TIMEOUT 240)
    add_test(NAME sandbox-city-v14-view COMMAND openemperor-sandbox-view-tests --water-only)
    set_tests_properties(sandbox-city-v14-view PROPERTIES TIMEOUT 240)
    add_test(NAME sandbox-software-view COMMAND openemperor-sandbox-view-tests)
    set_tests_properties(sandbox-software-view PROPERTIES TIMEOUT 240)
    add_executable(openemperor-fire-visual-tests tests/FireVisualTests.cpp)
    target_link_libraries(openemperor-fire-visual-tests PRIVATE openemperor-sg3 SDL3::SDL3)
    openemperor_enable_warnings(openemperor-fire-visual-tests)
    add_test(NAME animated-fire-clip-pixels COMMAND openemperor-fire-visual-tests)
    set_tests_properties(animated-fire-clip-pixels PROPERTIES TIMEOUT 45)
    add_executable(openemperor-fire-overlay-pixel-tests tests/FireOverlayPixelTests.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp
        src/app/RoadDrag.cpp src/app/RoadTopology.cpp src/app/VisualSelection.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-fire-overlay-pixel-tests PRIVATE
        openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-fire-overlay-pixel-tests)
    add_test(NAME fire-overlay-production-pixels COMMAND openemperor-fire-overlay-pixel-tests)
    set_tests_properties(fire-overlay-production-pixels PROPERTIES TIMEOUT 45)
    add_executable(openemperor-fire-inspector-view-tests tests/FireInspectorViewTests.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp
        src/app/RoadDrag.cpp src/app/RoadTopology.cpp src/app/VisualSelection.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-fire-inspector-view-tests PRIVATE
        openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-fire-inspector-view-tests)
    add_test(NAME fire-inspector-production-view COMMAND openemperor-fire-inspector-view-tests)
    set_tests_properties(fire-inspector-production-view PROPERTIES TIMEOUT 90)
    add_executable(openemperor-menu-session-tests tests/MenuSessionTests.cpp
        src/app/MenuSession.cpp src/app/MenuStorage.cpp src/app/MenuDialog.cpp src/app/MapRulesPreflight.cpp
        src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp)
    target_sources(openemperor-menu-session-tests PRIVATE src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-menu-session-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version Threads::Threads)
    openemperor_enable_warnings(openemperor-menu-session-tests)
    add_test(NAME menu-session-flow COMMAND openemperor-menu-session-tests)
    set_tests_properties(menu-session-flow PROPERTIES TIMEOUT 30)
    get_target_property(original_menu_test_sources openemperor-menu-session-tests SOURCES)
    list(REMOVE_ITEM original_menu_test_sources tests/MenuSessionTests.cpp)
    add_executable(openemperor-original-menu-session-tests
        tests/OriginalMenuSessionTests.cpp ${original_menu_test_sources})
    add_executable(openemperor-original-safe-demolition-menu-tests
        tests/OriginalSafeDemolitionMenuTests.cpp ${original_menu_test_sources})
    target_link_libraries(openemperor-original-safe-demolition-menu-tests PRIVATE
        openemperor-recovery SDL3::SDL3 openemperor-version Threads::Threads)
    openemperor_enable_warnings(openemperor-original-safe-demolition-menu-tests)
    add_test(NAME original-safe-demolition-menu COMMAND openemperor-original-safe-demolition-menu-tests)
    set_tests_properties(original-safe-demolition-menu PROPERTIES TIMEOUT 120)
    add_executable(openemperor-original-starter-menu-acceptance EXCLUDE_FROM_ALL
        tools/OriginalStarterMenuAcceptance.cpp ${original_menu_test_sources})
    target_link_libraries(openemperor-original-starter-menu-acceptance PRIVATE
        openemperor-recovery SDL3::SDL3 openemperor-version Threads::Threads)
    openemperor_enable_warnings(openemperor-original-starter-menu-acceptance)
    # Explicit OE41 diagnostic; it drives the actual menu/view via SDL events.
    add_executable(openemperor-system-playtest-acceptance EXCLUDE_FROM_ALL
        tools/SystemPlaytestAcceptance.cpp ${original_menu_test_sources})
    target_link_libraries(openemperor-system-playtest-acceptance PRIVATE
        openemperor-recovery SDL3::SDL3 openemperor-version Threads::Threads)
    openemperor_enable_warnings(openemperor-system-playtest-acceptance)
    target_link_libraries(openemperor-original-menu-session-tests PRIVATE
        openemperor-recovery SDL3::SDL3 openemperor-version Threads::Threads)
    openemperor_enable_warnings(openemperor-original-menu-session-tests)
    add_test(NAME original-player-mode-menu COMMAND openemperor-original-menu-session-tests --player-mode-menu)
    set_tests_properties(original-player-mode-menu PROPERTIES TIMEOUT 120)
    add_test(NAME original-menu-session-flow COMMAND openemperor-original-menu-session-tests --core-menu)
    set_tests_properties(original-menu-session-flow PROPERTIES TIMEOUT 120)
    add_test(NAME original-menu-successor-flow COMMAND openemperor-original-menu-session-tests --legacy-successor-menu)
    set_tests_properties(original-menu-successor-flow PROPERTIES TIMEOUT 120)
    add_test(NAME original-deadline-service-menu COMMAND openemperor-original-menu-session-tests --rule17-menu)
    set_tests_properties(original-deadline-service-menu PROPERTIES TIMEOUT 120)
    add_test(NAME original-deadline-food-menu COMMAND openemperor-original-menu-session-tests --rule18-menu)
    set_tests_properties(original-deadline-food-menu PROPERTIES TIMEOUT 120)
    add_executable(openemperor-sandbox-economy-guidance-view-tests tests/SandboxEconomyGuidanceViewTests.cpp
        src/app/MenuSession.cpp src/app/MenuStorage.cpp src/app/MenuDialog.cpp src/app/MapRulesPreflight.cpp
        src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp)
    target_sources(openemperor-sandbox-economy-guidance-view-tests PRIVATE src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-sandbox-economy-guidance-view-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version Threads::Threads)
    openemperor_enable_warnings(openemperor-sandbox-economy-guidance-view-tests)
    add_test(NAME sandbox-economy-guidance-view COMMAND openemperor-sandbox-economy-guidance-view-tests)
    set_tests_properties(sandbox-economy-guidance-view PROPERTIES TIMEOUT 90)

    add_executable(openemperor-menu-map-rules-tests tests/MenuMapRulesTests.cpp
        src/app/MenuSession.cpp src/app/MenuStorage.cpp src/app/MenuDialog.cpp src/app/MapRulesPreflight.cpp
        src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp
        src/app/RoadDrag.cpp src/app/RoadTopology.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-menu-map-rules-tests PRIVATE
        openemperor-recovery SDL3::SDL3 openemperor-version Threads::Threads)
    openemperor_enable_warnings(openemperor-menu-map-rules-tests)
    add_test(NAME menu-map-rules-selection COMMAND openemperor-menu-map-rules-tests)
    set_tests_properties(menu-map-rules-selection PROPERTIES TIMEOUT 45)

    add_executable(openemperor-input-coordinate-tests tests/InputCoordinateTests.cpp src/scene/IsoProjection.cpp)
    target_include_directories(openemperor-input-coordinate-tests PRIVATE src)
    target_link_libraries(openemperor-input-coordinate-tests PRIVATE SDL3::SDL3)
    openemperor_enable_warnings(openemperor-input-coordinate-tests)
    add_test(NAME input-coordinate-contract COMMAND openemperor-input-coordinate-tests)
    set_tests_properties(input-coordinate-contract PROPERTIES TIMEOUT 20)

    add_executable(openemperor-input-diagnostics-tests tests/InputDiagnosticsTests.cpp src/app/InputDiagnostics.cpp)
    target_include_directories(openemperor-input-diagnostics-tests PRIVATE src)
    target_link_libraries(openemperor-input-diagnostics-tests PRIVATE SDL3::SDL3 nlohmann_json::nlohmann_json)
    openemperor_enable_warnings(openemperor-input-diagnostics-tests)
    add_test(NAME input-diagnostics-bounded COMMAND openemperor-input-diagnostics-tests)
    set_tests_properties(input-diagnostics-bounded PROPERTIES TIMEOUT 20)

    add_executable(openemperor-menu-input-tests tests/MenuInputTests.cpp
        src/app/MenuSession.cpp src/app/MenuStorage.cpp src/app/MenuDialog.cpp src/app/MapRulesPreflight.cpp
        src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp
        src/app/RoadDrag.cpp src/app/RoadTopology.cpp src/renderer/StoredGraphicsRenderer.cpp
        src/renderer/StoredCamera.cpp src/renderer/WalkerSpriteSet.cpp
        src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-menu-input-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version Threads::Threads)
    openemperor_enable_warnings(openemperor-menu-input-tests)
    add_test(NAME menu-input-boundaries COMMAND openemperor-menu-input-tests)
    set_tests_properties(menu-input-boundaries PROPERTIES TIMEOUT 30)

    add_executable(openemperor-map-rules-check-tests tests/MapRulesCheckTests.cpp)
    target_link_libraries(openemperor-map-rules-check-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-map-rules-check-tests)
    add_test(NAME map-rules-selected-service COMMAND openemperor-map-rules-check-tests)
    set_tests_properties(map-rules-selected-service PROPERTIES TIMEOUT 30)
    add_executable(openemperor-map-rules-preflight-tests tests/MapRulesPreflightTests.cpp
        src/app/MapRulesPreflight.cpp)
    target_link_libraries(openemperor-map-rules-preflight-tests PRIVATE openemperor-maps Threads::Threads)
    openemperor_enable_warnings(openemperor-map-rules-preflight-tests)
    add_test(NAME map-rules-selected-worker COMMAND openemperor-map-rules-preflight-tests)
    set_tests_properties(map-rules-selected-worker PROPERTIES TIMEOUT 30)

    add_executable(openemperor-map-input-tests tests/MapViewInputTests.cpp
        src/app/MapBrowser.cpp tests/MapDebugViewInputAdapter.cpp src/renderer/TerrainPreviewRenderer.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
    target_link_libraries(openemperor-map-input-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-map-input-tests)
    add_test(NAME map-input-boundaries COMMAND openemperor-map-input-tests)
    set_tests_properties(map-input-boundaries PROPERTIES TIMEOUT 30)

    add_test(NAME sandbox-input-reliability COMMAND openemperor-sandbox-view-tests --input-only)
    set_tests_properties(sandbox-input-reliability PROPERTIES TIMEOUT 60)
    add_executable(openemperor-sandbox-save-tests tests/SandboxSaveTests.cpp)
    add_executable(openemperor-gate-passage-save-tests tests/GatePassageSaveTests.cpp)
    add_executable(openemperor-gate-passage-road-view-tests tests/GatePassageRoadViewTests.cpp
        src/app/RoadDrag.cpp src/app/RoadTopology.cpp)
    add_executable(openemperor-sandbox-gate-passage-tests tests/SandboxGatePassageTests.cpp)
    add_executable(openemperor-sandbox-map-permissions-tests tests/SandboxMapPermissionsTests.cpp)
    target_link_libraries(openemperor-sandbox-map-permissions-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-sandbox-map-permissions-tests)
    add_test(NAME sandbox-map-permissions COMMAND openemperor-sandbox-map-permissions-tests)
    target_link_libraries(openemperor-sandbox-gate-passage-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-sandbox-gate-passage-tests)
    add_test(NAME sandbox-fixed-gate-passages COMMAND openemperor-sandbox-gate-passage-tests)
    target_link_libraries(openemperor-gate-passage-road-view-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-gate-passage-road-view-tests)
    add_test(NAME gate-passage-road-preview COMMAND openemperor-gate-passage-road-view-tests)
    target_link_libraries(openemperor-gate-passage-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-gate-passage-save-tests)
    add_test(NAME gate-passage-save-recovery COMMAND openemperor-gate-passage-save-tests)
    set_tests_properties(gate-passage-save-recovery PROPERTIES TIMEOUT 120)
    target_link_libraries(openemperor-sandbox-save-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-save-tests)
    add_test(NAME sandbox-save-resume COMMAND openemperor-sandbox-save-tests)
    set_tests_properties(sandbox-save-resume PROPERTIES TIMEOUT 30)
    add_executable(openemperor-recovery-tests tests/RecoveryTests.cpp)
    target_link_libraries(openemperor-recovery-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-recovery-tests)
    add_test(NAME autosave-recovery-history COMMAND openemperor-recovery-tests)
    set_tests_properties(autosave-recovery-history PROPERTIES TIMEOUT 60)
    add_executable(openemperor-original-recovery-tests tests/OriginalRecoveryTests.cpp)
    target_link_libraries(openemperor-original-recovery-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-recovery-tests)
    add_test(NAME original-recovery-capability COMMAND openemperor-original-recovery-tests)
    set_tests_properties(original-recovery-capability PROPERTIES TIMEOUT 180)
    add_executable(openemperor-sandbox-routing-tests tests/SandboxRoutingTests.cpp)
    target_link_libraries(openemperor-sandbox-routing-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-routing-tests)
    add_test(NAME sandbox-live-rerouting COMMAND openemperor-sandbox-routing-tests)

    add_executable(openemperor-road-connectivity-tests tests/RoadConnectivityTests.cpp)
    target_link_libraries(openemperor-road-connectivity-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-road-connectivity-tests)
    add_test(NAME road-connectivity-diagnostics COMMAND openemperor-road-connectivity-tests)
    add_executable(openemperor-original-road-transport-tests tests/OriginalRoadTransportTests.cpp)
    target_link_libraries(openemperor-original-road-transport-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-road-transport-tests)
    add_test(NAME original-road-transport COMMAND openemperor-original-road-transport-tests)
    add_executable(openemperor-bridge-transport-tests tests/BridgeTransportTests.cpp)
    target_link_libraries(openemperor-bridge-transport-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-bridge-transport-tests)
    add_test(NAME bridge-transport COMMAND openemperor-bridge-transport-tests)
    add_executable(openemperor-bridge-rendering-tests tests/BridgeRenderingTests.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp)
    target_link_libraries(openemperor-bridge-rendering-tests PRIVATE openemperor-scene SDL3::SDL3)
    openemperor_enable_warnings(openemperor-bridge-rendering-tests)
    add_test(NAME bridge-rendering-pixels COMMAND openemperor-bridge-rendering-tests)
    add_executable(openemperor-original-road-classification-tests tests/OriginalRoadClassificationTests.cpp)
    target_link_libraries(openemperor-original-road-classification-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-original-road-classification-tests)
    add_test(NAME original-road-classification COMMAND openemperor-original-road-classification-tests)
    add_executable(openemperor-original-road-save-tests tests/OriginalRoadSaveTests.cpp)
    target_link_libraries(openemperor-original-road-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-road-save-tests)
    add_test(NAME original-road-save-recovery COMMAND openemperor-original-road-save-tests)
    set_tests_properties(original-road-transport original-road-classification original-road-save-recovery PROPERTIES TIMEOUT 120)
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
    set_tests_properties(sandbox-city-economy PROPERTIES TIMEOUT 60)

    add_executable(openemperor-sandbox-city-v7-tests tests/SandboxCityV7Tests.cpp)
    target_link_libraries(openemperor-sandbox-city-v7-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-city-v7-tests)
    add_test(NAME sandbox-city-v7-food COMMAND openemperor-sandbox-city-v7-tests)
    set_tests_properties(sandbox-city-v7-food PROPERTIES TIMEOUT 60)
    add_executable(openemperor-sandbox-city-v8-tests tests/SandboxCityV8Tests.cpp)
    target_link_libraries(openemperor-sandbox-city-v8-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-city-v8-tests)
    add_test(NAME sandbox-city-v8-service COMMAND openemperor-sandbox-city-v8-tests)
    set_tests_properties(sandbox-city-v8-service PROPERTIES TIMEOUT 90)

    add_executable(openemperor-sandbox-city-v9-tests tests/SandboxCityV9Tests.cpp)
    target_link_libraries(openemperor-sandbox-city-v9-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-city-v9-tests)
    add_test(NAME sandbox-city-v9-population COMMAND openemperor-sandbox-city-v9-tests)
    set_tests_properties(sandbox-city-v9-population PROPERTIES TIMEOUT 120)
    add_executable(openemperor-sandbox-city-v10-tests tests/SandboxCityV10Tests.cpp)
    target_link_libraries(openemperor-sandbox-city-v10-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-city-v10-tests)
    add_test(NAME sandbox-city-v10-scaling COMMAND openemperor-sandbox-city-v10-tests)
    set_tests_properties(sandbox-city-v10-scaling PROPERTIES TIMEOUT 600)
    add_executable(openemperor-sandbox-city-v11-tests tests/SandboxCityV11Tests.cpp)
    target_link_libraries(openemperor-sandbox-city-v11-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-city-v11-tests)
    add_test(NAME sandbox-city-v11-markets COMMAND openemperor-sandbox-city-v11-tests)
    set_tests_properties(sandbox-city-v11-markets PROPERTIES TIMEOUT 180)

    add_executable(openemperor-sandbox-city-v11-balance-tests
        tests/SandboxCityV11BalanceTests.cpp)
    target_link_libraries(openemperor-sandbox-city-v11-balance-tests
        PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-city-v11-balance-tests)
    add_test(NAME sandbox-city-v11-balance
        COMMAND openemperor-sandbox-city-v11-balance-tests)
    set_tests_properties(sandbox-city-v11-balance PROPERTIES TIMEOUT 180)

    add_executable(openemperor-sandbox-city-v11-operations-tests
        tests/SandboxCityV11OperationsTests.cpp)
    target_link_libraries(openemperor-sandbox-city-v11-operations-tests
        PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-city-v11-operations-tests)
    add_test(NAME sandbox-city-v11-operation-controls
        COMMAND openemperor-sandbox-city-v11-operations-tests)
    set_tests_properties(sandbox-city-v11-operation-controls PROPERTIES TIMEOUT 180)
    add_executable(openemperor-sandbox-city-v11-demolition-tests
        tests/SandboxCityV11DemolitionTests.cpp)
    target_link_libraries(openemperor-sandbox-city-v11-demolition-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-sandbox-city-v11-demolition-tests)
    add_test(NAME sandbox-city-v11-demolition COMMAND openemperor-sandbox-city-v11-demolition-tests)
    set_tests_properties(sandbox-city-v11-demolition PROPERTIES TIMEOUT 180)
    add_executable(openemperor-sandbox-city-v12-tests tests/SandboxCityV12Tests.cpp)
    target_link_libraries(openemperor-sandbox-city-v12-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-sandbox-city-v12-tests)
    foreach(fire_test base persistence determinism endurance)
        add_test(NAME sandbox-city-v12-${fire_test}
            COMMAND openemperor-sandbox-city-v12-tests ${fire_test})
        set_tests_properties(sandbox-city-v12-${fire_test} PROPERTIES TIMEOUT 300)
    endforeach()
    set_tests_properties(sandbox-city-v12-endurance PROPERTIES TIMEOUT 900)

    add_executable(openemperor-sandbox-city-v13-tests tests/SandboxCityV13Tests.cpp)
    target_link_libraries(openemperor-sandbox-city-v13-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-sandbox-city-v13-tests)
    foreach(quality_test base persistence districts determinism endurance)
        add_test(NAME sandbox-city-v13-${quality_test}
            COMMAND openemperor-sandbox-city-v13-tests ${quality_test})
        set_tests_properties(sandbox-city-v13-${quality_test} PROPERTIES TIMEOUT 900)
    endforeach()

    add_executable(openemperor-sandbox-city-v14-tests tests/SandboxCityV14Tests.cpp)
    target_link_libraries(openemperor-sandbox-city-v14-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-sandbox-city-v14-tests)
    foreach(water_test base persistence districts determinism endurance)
        add_test(NAME sandbox-city-v14-${water_test}
            COMMAND openemperor-sandbox-city-v14-tests ${water_test})
        set_tests_properties(sandbox-city-v14-${water_test} PROPERTIES TIMEOUT 900)
    endforeach()

    add_executable(openemperor-sandbox-city-v15-tests tests/SandboxCityV15Tests.cpp)
    target_link_libraries(openemperor-sandbox-city-v15-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-sandbox-city-v15-tests)
    foreach(health_test base persistence districts determinism endurance)
        add_test(NAME sandbox-city-v15-${health_test}
            COMMAND openemperor-sandbox-city-v15-tests ${health_test})
        set_tests_properties(sandbox-city-v15-${health_test} PROPERTIES TIMEOUT 900)
    endforeach()

    add_executable(openemperor-sandbox-city-v16-tests tests/SandboxCityV16Tests.cpp)
    target_link_libraries(openemperor-sandbox-city-v16-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-sandbox-city-v16-tests)
    foreach(maintenance_test base persistence balance determinism endurance geometry determinism-v2 endurance-v2)
        add_test(NAME sandbox-city-v16-${maintenance_test}
            COMMAND openemperor-sandbox-city-v16-tests ${maintenance_test})
        set_tests_properties(sandbox-city-v16-${maintenance_test} PROPERTIES TIMEOUT 900)
    endforeach()

    add_executable(openemperor-original-scheduler-tests tests/OriginalSchedulerTests.cpp)
    target_include_directories(openemperor-original-scheduler-tests PRIVATE src)
    openemperor_enable_warnings(openemperor-original-scheduler-tests)
    add_test(NAME original-scheduler COMMAND openemperor-original-scheduler-tests)
    set_tests_properties(original-scheduler PROPERTIES TIMEOUT 30)

    add_executable(openemperor-original-scheduler-world-tests tests/OriginalSchedulerWorldTests.cpp)
    target_link_libraries(openemperor-original-scheduler-world-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-original-scheduler-world-tests)
    add_test(NAME original-scheduler-world COMMAND openemperor-original-scheduler-world-tests)
    set_tests_properties(original-scheduler-world PROPERTIES TIMEOUT 120)

    add_executable(openemperor-original-economy-save-tests tests/OriginalEconomySaveTests.cpp)
    target_link_libraries(openemperor-original-economy-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-economy-save-tests)
    add_test(NAME original-economy-save COMMAND openemperor-original-economy-save-tests)
    set_tests_properties(original-economy-save PROPERTIES TIMEOUT 30)

    add_executable(openemperor-original-influence-world-tests tests/OriginalInfluenceWorldTests.cpp)
    target_link_libraries(openemperor-original-influence-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-influence-world-tests)
    add_test(NAME original-influence-world COMMAND openemperor-original-influence-world-tests)
    set_tests_properties(original-influence-world PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-influence-save-tests tests/OriginalInfluenceSaveTests.cpp)
    target_link_libraries(openemperor-original-influence-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-influence-save-tests)
    add_test(NAME original-influence-save COMMAND openemperor-original-influence-save-tests)
    set_tests_properties(original-influence-save PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-goods-ledger-tests tests/OriginalGoodsLedgerTests.cpp)
    target_link_libraries(openemperor-original-goods-ledger-tests PRIVATE openemperor-original-goods-ledger)
    openemperor_enable_warnings(openemperor-original-goods-ledger-tests)
    add_test(NAME original-goods-ledger COMMAND openemperor-original-goods-ledger-tests)
    set_tests_properties(original-goods-ledger PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-goods-world-tests tests/OriginalGoodsWorldTests.cpp)
    target_link_libraries(openemperor-original-goods-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-goods-world-tests)
    add_test(NAME original-goods-world COMMAND openemperor-original-goods-world-tests)
    set_tests_properties(original-goods-world PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-goods-save-tests tests/OriginalGoodsSaveTests.cpp)
    target_link_libraries(openemperor-original-goods-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-goods-save-tests)
    add_test(NAME original-goods-save COMMAND openemperor-original-goods-save-tests)
    set_tests_properties(original-goods-save PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-production-world-tests tests/OriginalProductionWorldTests.cpp)
    target_link_libraries(openemperor-original-production-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-production-world-tests)
    add_test(NAME original-production-world COMMAND openemperor-original-production-world-tests)
    set_tests_properties(original-production-world PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-finance-world-tests tests/OriginalFinanceWorldTests.cpp)
    target_link_libraries(openemperor-original-finance-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-finance-world-tests)
    add_test(NAME original-finance-world COMMAND openemperor-original-finance-world-tests)
    add_test(NAME original-finance-world-real COMMAND openemperor-original-finance-world-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-finance-world original-finance-world-real PROPERTIES TIMEOUT 300)

    add_executable(openemperor-original-finance-save-tests tests/OriginalFinanceSaveTests.cpp)
    target_link_libraries(openemperor-original-finance-save-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-finance-save-tests)
    add_test(NAME original-finance-save COMMAND openemperor-original-finance-save-tests)
    add_test(NAME original-finance-save-real COMMAND openemperor-original-finance-save-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-finance-save original-finance-save-real PROPERTIES TIMEOUT 300)

    add_executable(openemperor-original-house-inspection-tests tests/OriginalHouseInspectionTests.cpp)
    target_link_libraries(openemperor-original-house-inspection-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-house-inspection-tests)
    add_test(NAME original-house-inspection COMMAND openemperor-original-house-inspection-tests)
    add_test(NAME original-house-inspection-real COMMAND openemperor-original-house-inspection-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-house-inspection original-house-inspection-real PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-production-save-tests tests/OriginalProductionSaveTests.cpp)
    target_link_libraries(openemperor-original-production-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-production-save-tests)
    add_test(NAME original-production-save COMMAND openemperor-original-production-save-tests)
    set_tests_properties(original-production-save PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-market-transport-tests tests/OriginalMarketTransportTests.cpp)
    target_link_libraries(openemperor-original-market-transport-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-market-transport-tests)
    add_test(NAME original-market-transport COMMAND openemperor-original-market-transport-tests)
    # The full20k snapshot comparison is substantially slower without optimizer.
    set_tests_properties(original-market-transport PROPERTIES TIMEOUT 300)

    add_executable(openemperor-original-market-save-tests tests/OriginalMarketSaveTests.cpp)
    target_link_libraries(openemperor-original-market-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-market-save-tests)
    add_test(NAME original-market-save COMMAND openemperor-original-market-save-tests)
    set_tests_properties(original-market-save PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-hemp-world-tests tests/OriginalHempWorldTests.cpp)
    target_link_libraries(openemperor-original-hemp-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-hemp-world-tests)
    add_test(NAME original-hemp-world COMMAND openemperor-original-hemp-world-tests)
    set_tests_properties(original-hemp-world PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-hemp-save-tests tests/OriginalHempSaveTests.cpp)
    target_link_libraries(openemperor-original-hemp-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-hemp-save-tests)
    add_test(NAME original-hemp-save COMMAND openemperor-original-hemp-save-tests)
    set_tests_properties(original-hemp-save PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-consumption-world-tests tests/OriginalConsumptionWorldTests.cpp)
    target_link_libraries(openemperor-original-consumption-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-consumption-world-tests)
    add_test(NAME original-consumption-world COMMAND openemperor-original-consumption-world-tests)
    set_tests_properties(original-consumption-world PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-consumption-save-tests tests/OriginalConsumptionSaveTests.cpp)
    target_link_libraries(openemperor-original-consumption-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-consumption-save-tests)
    add_test(NAME original-consumption-save COMMAND openemperor-original-consumption-save-tests)
    set_tests_properties(original-consumption-save PROPERTIES TIMEOUT 120)

    add_executable(openemperor-original-food-save-tests tests/OriginalFoodSaveTests.cpp)
    target_link_libraries(openemperor-original-food-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-food-save-tests)
    add_test(NAME original-food-save COMMAND openemperor-original-food-save-tests)
    set_tests_properties(original-food-save PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-food-world-tests tests/OriginalFoodWorldTests.cpp)
    target_link_libraries(openemperor-original-food-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-food-world-tests)
    add_test(NAME original-food-world COMMAND openemperor-original-food-world-tests)
    set_tests_properties(original-food-world PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-services-world-tests tests/OriginalServicesWorldTests.cpp)
    target_link_libraries(openemperor-original-services-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-services-world-tests)
    add_test(NAME original-services-world COMMAND openemperor-original-services-world-tests)
    set_tests_properties(original-services-world PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-services-save-tests tests/OriginalServicesSaveTests.cpp)
    target_link_libraries(openemperor-original-services-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-services-save-tests)
    add_test(NAME original-services-save COMMAND openemperor-original-services-save-tests)
    set_tests_properties(original-services-save PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-ancestor-world-tests tests/OriginalAncestorWorldTests.cpp)
    target_link_libraries(openemperor-original-ancestor-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-ancestor-world-tests)
    add_test(NAME original-ancestor-world COMMAND openemperor-original-ancestor-world-tests)
    set_tests_properties(original-ancestor-world PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-ancestor-save-tests tests/OriginalAncestorSaveTests.cpp)
    target_link_libraries(openemperor-original-ancestor-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-ancestor-save-tests)
    add_test(NAME original-ancestor-save COMMAND openemperor-original-ancestor-save-tests)
    set_tests_properties(original-ancestor-save PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-religion-world-tests tests/OriginalReligionWorldTests.cpp)
    target_link_libraries(openemperor-original-religion-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-religion-world-tests)
    add_test(NAME original-religion-world COMMAND openemperor-original-religion-world-tests)
    if(OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT)
        add_test(NAME original-religion-world-real COMMAND openemperor-original-religion-world-tests
            --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
        set_tests_properties(original-religion-world-real PROPERTIES TIMEOUT 300)
    endif()
    set_tests_properties(original-religion-world PROPERTIES TIMEOUT 300)

    add_executable(openemperor-original-religion-save-tests tests/OriginalReligionSaveTests.cpp)
    target_link_libraries(openemperor-original-religion-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-religion-save-tests)
    add_test(NAME original-religion-save COMMAND openemperor-original-religion-save-tests)
    set_tests_properties(original-religion-save PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-housing-stages-world-tests tests/OriginalHousingStagesWorldTests.cpp)
    target_link_libraries(openemperor-original-housing-stages-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-housing-stages-world-tests)
    add_test(NAME original-housing-stages-world COMMAND openemperor-original-housing-stages-world-tests)
    set_tests_properties(original-housing-stages-world PROPERTIES TIMEOUT 300)
    if(OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT)
        add_test(NAME original-housing-stages-world-real COMMAND openemperor-original-housing-stages-world-tests
            --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
        set_tests_properties(original-housing-stages-world-real PROPERTIES TIMEOUT 300)
    endif()

    add_executable(openemperor-original-housing-stages-save-tests tests/OriginalHousingStagesSaveTests.cpp)
    target_link_libraries(openemperor-original-housing-stages-save-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-housing-stages-save-tests)
    add_test(NAME original-housing-stages-save COMMAND openemperor-original-housing-stages-save-tests)
    set_tests_properties(original-housing-stages-save PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-elite-footprint-world-tests tests/OriginalEliteFootprintWorldTests.cpp)
    target_link_libraries(openemperor-original-elite-footprint-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-elite-footprint-world-tests)
    add_test(NAME original-elite-footprint-world COMMAND openemperor-original-elite-footprint-world-tests)
    set_tests_properties(original-elite-footprint-world PROPERTIES TIMEOUT 300)

    add_executable(openemperor-original-elite-footprint-save-tests tests/OriginalEliteFootprintSaveTests.cpp)
    target_link_libraries(openemperor-original-elite-footprint-save-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-elite-footprint-save-tests)
    add_test(NAME original-elite-footprint-save COMMAND openemperor-original-elite-footprint-save-tests)
    set_tests_properties(original-elite-footprint-save PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-industry-world-tests tests/OriginalIndustryWorldTests.cpp)
    target_link_libraries(openemperor-original-industry-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-industry-world-tests)
    add_test(NAME original-industry-world COMMAND openemperor-original-industry-world-tests)
    if(OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT)
        add_test(NAME original-industry-world-real COMMAND openemperor-original-industry-world-tests
            --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
        set_tests_properties(original-industry-world-real PROPERTIES TIMEOUT 300)
    endif()
    set_tests_properties(original-industry-world PROPERTIES TIMEOUT 300)

    add_executable(openemperor-original-industry-save-tests tests/OriginalIndustrySaveTests.cpp)
    target_link_libraries(openemperor-original-industry-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-industry-save-tests)
    add_test(NAME original-industry-save COMMAND openemperor-original-industry-save-tests)
    set_tests_properties(original-industry-save PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-culture-world-tests tests/OriginalCultureWorldTests.cpp)
    target_link_libraries(openemperor-original-culture-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-culture-world-tests)
    add_test(NAME original-culture-world COMMAND openemperor-original-culture-world-tests)
    if(OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT)
        add_test(NAME original-culture-world-real COMMAND openemperor-original-culture-world-tests
            --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
        set_tests_properties(original-culture-world-real PROPERTIES TIMEOUT 300)
    endif()
    set_tests_properties(original-culture-world PROPERTIES TIMEOUT 300)

    add_executable(openemperor-original-culture-save-tests tests/OriginalCultureSaveTests.cpp)
    target_link_libraries(openemperor-original-culture-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-culture-save-tests)
    add_test(NAME original-culture-save COMMAND openemperor-original-culture-save-tests)
    set_tests_properties(original-culture-save PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-water-world-tests tests/OriginalWaterWorldTests.cpp)
    target_link_libraries(openemperor-original-water-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-water-world-tests)
    add_test(NAME original-water-world COMMAND openemperor-original-water-world-tests)
    set_tests_properties(original-water-world PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-housing-world-tests tests/OriginalHousingWorldTests.cpp)
    target_link_libraries(openemperor-original-housing-world-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-housing-world-tests)
    add_test(NAME original-housing-world COMMAND openemperor-original-housing-world-tests)
    if(OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT)
        add_test(NAME original-housing-world-real COMMAND openemperor-original-housing-world-tests
            --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
        set_tests_properties(original-housing-world-real PROPERTIES TIMEOUT 300)
    endif()
    set_tests_properties(original-housing-world PROPERTIES TIMEOUT 300)

    add_executable(openemperor-original-housing-save-tests tests/OriginalHousingSaveTests.cpp)
    target_link_libraries(openemperor-original-housing-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-housing-save-tests)
    add_test(NAME original-housing-save COMMAND openemperor-original-housing-save-tests)
    set_tests_properties(original-housing-save PROPERTIES TIMEOUT 180)

    add_executable(openemperor-original-economy-rule-gate-tests
        tests/OriginalEconomyRuleGateTests.cpp)
    target_link_libraries(openemperor-original-economy-rule-gate-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-original-economy-rule-gate-tests)
    add_test(NAME original-economy-rule-gate COMMAND openemperor-original-economy-rule-gate-tests)
    set_tests_properties(original-economy-rule-gate PROPERTIES TIMEOUT 30)

    add_executable(openemperor-city-start-guidance-tests
        tests/CityStartGuidanceTests.cpp)
    target_link_libraries(openemperor-city-start-guidance-tests
        PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-city-start-guidance-tests)
    add_test(NAME city-v11-start-guidance
        COMMAND openemperor-city-start-guidance-tests)
    set_tests_properties(city-v11-start-guidance PROPERTIES TIMEOUT 60)
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
        add_test(NAME original-model-inspector-cli COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/test_model_inspector_cli.py
            --executable $<TARGET_FILE:openemperor-model-inspect>)
        set_tests_properties(original-model-inspector-cli PROPERTIES
            TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
        add_test(NAME original-player-mode-restart COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/test_player_mode_restart.py
            $<TARGET_FILE:openemperor-original-menu-session-tests>)
        set_tests_properties(original-player-mode-restart PROPERTIES TIMEOUT 120
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME macos-acceptance-harness COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_SOURCE_DIR}/tests -p test_macos_acceptance_harness.py)
        set_tests_properties(macos-acceptance-harness PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME original-economy-cli COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/test_original_economy_cli.py --executable $<TARGET_FILE:openemperor>)
        set_tests_properties(original-economy-cli PROPERTIES TIMEOUT 60
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME original-rules-parity-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_SOURCE_DIR}/tests -p test_original_rules_parity.py)
        set_tests_properties(original-rules-parity-contract PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME original-influence-runner-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_SOURCE_DIR}/tests -p test_original_influence_runner.py)
        set_tests_properties(original-influence-runner-contract PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME original-goods-runner-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_SOURCE_DIR}/tests -p test_original_goods_runner.py)
        set_tests_properties(original-goods-runner-contract PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME original-production-runner-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_SOURCE_DIR}/tests -p test_original_production_runner.py)
        set_tests_properties(original-production-runner-contract PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME original-finance-runner-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s ${CMAKE_SOURCE_DIR}/tests -p test_original_finance_runner.py)
        set_tests_properties(original-finance-runner-contract PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
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
    add_executable(openemperor-stored-shore-graphics-tests tests/StoredShoreGraphicsTests.cpp
        src/renderer/StoredGraphicsRenderer.cpp)
    target_link_libraries(openemperor-stored-shore-graphics-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-stored-shore-graphics-tests)
    add_test(NAME stored-shore-landing-visuals COMMAND openemperor-stored-shore-graphics-tests)
    set_tests_properties(stored-shore-landing-visuals PROPERTIES TIMEOUT 30)
    add_executable(openemperor-ordinary-wall-gate-tests tests/OrdinaryWallGatePresentationTests.cpp
        src/renderer/StoredGraphicsRenderer.cpp)
    target_link_libraries(openemperor-ordinary-wall-gate-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-ordinary-wall-gate-tests)
    add_test(NAME ordinary-wall-gate-presentation COMMAND openemperor-ordinary-wall-gate-tests)
    set_tests_properties(ordinary-wall-gate-presentation PROPERTIES TIMEOUT 30)
    add_executable(openemperor-ordinary-gate-presentation-tests tests/OrdinaryGateMapPresentationTests.cpp
        src/renderer/StoredGraphicsRenderer.cpp)
    target_link_libraries(openemperor-ordinary-gate-presentation-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-ordinary-gate-presentation-tests)
    add_test(NAME ordinary-gate-map-presentation COMMAND openemperor-ordinary-gate-presentation-tests)
    set_tests_properties(ordinary-gate-map-presentation PROPERTIES TIMEOUT 30)
    add_executable(openemperor-ordinary-tower-join-tests tests/OrdinaryTowerJoinTests.cpp
        src/renderer/StoredGraphicsRenderer.cpp)
    target_link_libraries(openemperor-ordinary-tower-join-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-ordinary-tower-join-tests)
    add_test(NAME ordinary-tower-joins-production-pixels COMMAND openemperor-ordinary-tower-join-tests)
    set_tests_properties(ordinary-tower-joins-production-pixels PROPERTIES TIMEOUT 30)
    add_executable(openemperor-ordinary-tower-selector-tests tests/OrdinaryTowerSelectorTests.cpp)
    target_link_libraries(openemperor-ordinary-tower-selector-tests PRIVATE openemperor-maps)
    openemperor_enable_warnings(openemperor-ordinary-tower-selector-tests)
    add_test(NAME ordinary-tower-selector COMMAND openemperor-ordinary-tower-selector-tests)
    set_tests_properties(ordinary-tower-selector PROPERTIES TIMEOUT 30)
    add_executable(openemperor-ordinary-tower-presentation-tests tests/OrdinaryTowerPresentationTests.cpp
        src/renderer/StoredGraphicsRenderer.cpp)
    target_link_libraries(openemperor-ordinary-tower-presentation-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-ordinary-tower-presentation-tests)
    add_test(NAME ordinary-tower-map-presentation COMMAND openemperor-ordinary-tower-presentation-tests)
    set_tests_properties(ordinary-tower-map-presentation PROPERTIES TIMEOUT 30)
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
    add_executable(openemperor-fire-inspector-walker-tests tests/FireInspectorWalkerTests.cpp
        src/app/WalkerPose.cpp)
    target_link_libraries(openemperor-fire-inspector-walker-tests PRIVATE
        openemperor-sg3 openemperor-simulation SDL3::SDL3)
    openemperor_enable_warnings(openemperor-fire-inspector-walker-tests)
    add_test(NAME fire-inspector-walker-profile-pose-render COMMAND openemperor-fire-inspector-walker-tests)
    set_tests_properties(fire-inspector-walker-profile-pose-render PROPERTIES TIMEOUT 60)
    add_executable(openemperor-market-walker-profile-tests tests/MarketWalkerProfileTests.cpp)
    target_link_libraries(openemperor-market-walker-profile-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-market-walker-profile-tests)
    add_test(NAME market-walker-profile COMMAND openemperor-market-walker-profile-tests)
    add_executable(openemperor-market-walker-flip-tests tests/MarketWalkerFlipTests.cpp)
    target_link_libraries(openemperor-market-walker-flip-tests PRIVATE openemperor-sg3 SDL3::SDL3)
    openemperor_enable_warnings(openemperor-market-walker-flip-tests)
    add_test(NAME market-walker-display-flip-pixels COMMAND openemperor-market-walker-flip-tests)
    add_executable(openemperor-market-walker-pose-tests tests/MarketWalkerPoseTests.cpp
        src/app/WalkerPose.cpp)
    target_link_libraries(openemperor-market-walker-pose-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-market-walker-pose-tests)
    add_test(NAME market-walker-pose COMMAND openemperor-market-walker-pose-tests)
    add_executable(openemperor-market-walker-resource-tests tests/MarketWalkerResourceTests.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp
        src/app/RoadDrag.cpp src/app/RoadTopology.cpp src/app/VisualSelection.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-market-walker-resource-tests PRIVATE
        openemperor-recovery openemperor-version SDL3::SDL3)
    openemperor_enable_warnings(openemperor-market-walker-resource-tests)
    add_test(NAME market-walker-optional-resources COMMAND openemperor-market-walker-resource-tests)
    add_executable(openemperor-service-walker-resource-tests tests/ServiceWalkerResourceTests.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp
        src/app/RoadDrag.cpp src/app/RoadTopology.cpp src/app/VisualSelection.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-service-walker-resource-tests PRIVATE
        openemperor-recovery openemperor-version SDL3::SDL3)
    openemperor_enable_warnings(openemperor-service-walker-resource-tests)
    add_test(NAME service-walker-optional-resources COMMAND openemperor-service-walker-resource-tests)
    add_executable(openemperor-health-walker-resource-tests tests/HealthWalkerResourceTests.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp
        src/app/RoadDrag.cpp src/app/RoadTopology.cpp src/app/VisualSelection.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-health-walker-resource-tests PRIVATE
        openemperor-recovery openemperor-version SDL3::SDL3)
    openemperor_enable_warnings(openemperor-health-walker-resource-tests)
    add_test(NAME health-walker-optional-resources COMMAND openemperor-health-walker-resource-tests)
    add_executable(openemperor-market-walker-view-tests tests/MarketWalkerViewTests.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp
        src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
        src/app/VisualSelection.cpp src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-market-walker-view-tests PRIVATE
        openemperor-recovery openemperor-version SDL3::SDL3)
    openemperor_enable_warnings(openemperor-market-walker-view-tests)
    add_test(NAME market-walker-production-view COMMAND openemperor-market-walker-view-tests)
    add_executable(openemperor-service-walker-view-tests tests/ServiceWalkerViewTests.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp
        src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
        src/app/VisualSelection.cpp src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-service-walker-view-tests PRIVATE
        openemperor-recovery openemperor-version SDL3::SDL3)
    openemperor_enable_warnings(openemperor-service-walker-view-tests)
    add_test(NAME service-walker-production-view COMMAND openemperor-service-walker-view-tests)
    add_executable(openemperor-health-walker-view-tests tests/HealthWalkerViewTests.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp
        src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
        src/app/VisualSelection.cpp src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-health-walker-view-tests PRIVATE
        openemperor-recovery openemperor-version SDL3::SDL3)
    openemperor_enable_warnings(openemperor-health-walker-view-tests)
    add_test(NAME health-walker-production-view COMMAND openemperor-health-walker-view-tests)
    add_executable(openemperor-road-ground-occlusion-tests tests/RoadGroundOcclusionTests.cpp
        src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp
        src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp
        src/app/RoadDrag.cpp src/app/RoadTopology.cpp src/app/VisualSelection.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-road-ground-occlusion-tests PRIVATE
        openemperor-recovery openemperor-version SDL3::SDL3)
    openemperor_enable_warnings(openemperor-road-ground-occlusion-tests)
    add_test(NAME road-ground-occlusion-production-pixels COMMAND openemperor-road-ground-occlusion-tests)
    add_test(NAME original-road-ground-pixels COMMAND openemperor-road-ground-occlusion-tests --original-roads-only)
    add_test(NAME original-road-asset-pixels COMMAND openemperor-road-ground-occlusion-tests --original-road-asset)
    add_test(NAME original-road-mixed-pixels COMMAND openemperor-road-ground-occlusion-tests --original-road-mixed)
    add_test(NAME original-road-stale-ground-pixels COMMAND openemperor-road-ground-occlusion-tests --original-road-stale)
    add_test(NAME original-road-axis-height-pixels COMMAND openemperor-road-ground-occlusion-tests --original-road-axis-height)
    add_executable(openemperor-walker-visual-tests tests/WalkerVisualTests.cpp
        src/app/WalkerPose.cpp src/renderer/WalkerSpriteSet.cpp)
    target_link_libraries(openemperor-walker-visual-tests PRIVATE
        openemperor-sg3 openemperor-simulation SDL3::SDL3)
    openemperor_enable_warnings(openemperor-walker-visual-tests)
    add_test(NAME walker-visual-profile-pose-render COMMAND openemperor-walker-visual-tests)
    set_tests_properties(walker-visual-profile-pose-render PROPERTIES TIMEOUT 30)

    add_executable(openemperor-service-walker-tests tests/ServiceWalkerTests.cpp
        src/app/WalkerPose.cpp src/renderer/WalkerSpriteSet.cpp)
    target_link_libraries(openemperor-service-walker-tests PRIVATE
        openemperor-sg3 openemperor-simulation SDL3::SDL3)
    openemperor_enable_warnings(openemperor-service-walker-tests)
    add_test(NAME service-walker-profile-pose-render COMMAND openemperor-service-walker-tests)
    set_tests_properties(service-walker-profile-pose-render PROPERTIES TIMEOUT 30)

    add_executable(openemperor-health-walker-tests tests/HealthWalkerTests.cpp
        src/app/WalkerPose.cpp src/renderer/WalkerSpriteSet.cpp)
    target_link_libraries(openemperor-health-walker-tests PRIVATE
        openemperor-sg3 openemperor-simulation SDL3::SDL3)
    openemperor_enable_warnings(openemperor-health-walker-tests)
    add_test(NAME health-walker-profile-pose-render COMMAND openemperor-health-walker-tests)
    set_tests_properties(health-walker-profile-pose-render PROPERTIES TIMEOUT 30)

    add_executable(openemperor-building-visual-tests tests/BuildingVisualTests.cpp
        src/renderer/BuildingSprite.cpp)
    target_link_libraries(openemperor-building-visual-tests PRIVATE openemperor-sg3 SDL3::SDL3)
    openemperor_enable_warnings(openemperor-building-visual-tests)
    add_test(NAME building-visual-profile-depth COMMAND openemperor-building-visual-tests)

    add_executable(openemperor-original-house-visual-profile-tests tests/OriginalHouseVisualProfileTests.cpp)
    target_link_libraries(openemperor-original-house-visual-profile-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-original-house-visual-profile-tests)
    add_dependencies(openemperor-original-house-visual-profile-tests openemperor)
    add_test(NAME original-house-visual-profile COMMAND openemperor-original-house-visual-profile-tests)
    add_test(NAME original-house-visual-profile-real COMMAND openemperor-original-house-visual-profile-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}" "${OPENEMPEROR_TEST_RESOURCE_ROOT}")
    set_tests_properties(original-house-visual-profile original-house-visual-profile-real PROPERTIES TIMEOUT 60)

    add_executable(openemperor-original-building-visual-profile-tests tests/OriginalBuildingVisualProfileTests.cpp)
    target_link_libraries(openemperor-original-building-visual-profile-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-original-building-visual-profile-tests)
    add_dependencies(openemperor-original-building-visual-profile-tests openemperor)
    add_test(NAME original-building-visual-profile COMMAND openemperor-original-building-visual-profile-tests)
    add_test(NAME original-building-visual-profile-real COMMAND openemperor-original-building-visual-profile-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}" "${OPENEMPEROR_TEST_RESOURCE_ROOT}")
    set_tests_properties(original-building-visual-profile original-building-visual-profile-real PROPERTIES TIMEOUT 60)

    add_executable(openemperor-road-topology-tests tests/RoadTopologyTests.cpp
        src/app/RoadTopology.cpp src/app/RoadDrag.cpp)
    target_link_libraries(openemperor-road-topology-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-road-topology-tests)
    add_test(NAME road-topology-live-preview COMMAND openemperor-road-topology-tests)

    add_executable(openemperor-road-responsiveness-tests tests/RoadResponsivenessTests.cpp
        src/app/RoadDrag.cpp)
    target_link_libraries(openemperor-road-responsiveness-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-road-responsiveness-tests)
    add_test(NAME road-responsive-preview-batch COMMAND openemperor-road-responsiveness-tests)

    add_executable(openemperor-frame-pacing-tests tests/FramePacingTests.cpp)
    target_include_directories(openemperor-frame-pacing-tests PRIVATE src)
    openemperor_enable_warnings(openemperor-frame-pacing-tests)
    add_test(NAME frame-pacing-remainder COMMAND openemperor-frame-pacing-tests)

    add_executable(openemperor-road-visual-tests tests/RoadVisualTests.cpp
        src/renderer/RoadSpriteSet.cpp src/app/RoadTopology.cpp)
    target_link_libraries(openemperor-road-visual-tests PRIVATE openemperor-sg3
        openemperor-simulation SDL3::SDL3)
    openemperor_enable_warnings(openemperor-road-visual-tests)
    add_test(NAME road-visual-profile-pixels COMMAND openemperor-road-visual-tests)
    add_executable(openemperor-compatibility-tests tests/CompatibilityProfileTests.cpp
        src/app/VisualSelection.cpp)
    target_link_libraries(openemperor-compatibility-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-compatibility-tests)
    add_test(NAME compatibility-profile-fingerprints COMMAND openemperor-compatibility-tests
        "${OPENEMPEROR_TEST_RESOURCE_ROOT}")
    add_executable(openemperor-market-walker-compatibility-tests tests/MarketWalkerCompatibilityTests.cpp
        src/app/VisualSelection.cpp)
    target_link_libraries(openemperor-market-walker-compatibility-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-market-walker-compatibility-tests)
    add_test(NAME market-walker-compatibility-fingerprints COMMAND openemperor-market-walker-compatibility-tests
        "${OPENEMPEROR_TEST_RESOURCE_ROOT}")
    add_executable(openemperor-service-walker-compatibility-tests tests/ServiceWalkerCompatibilityTests.cpp
        src/app/VisualSelection.cpp)
    target_link_libraries(openemperor-service-walker-compatibility-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-service-walker-compatibility-tests)
    add_test(NAME service-walker-compatibility-fingerprints COMMAND openemperor-service-walker-compatibility-tests
        "${OPENEMPEROR_TEST_RESOURCE_ROOT}")
    add_executable(openemperor-health-walker-compatibility-tests tests/HealthWalkerCompatibilityTests.cpp
        src/app/VisualSelection.cpp)
    target_link_libraries(openemperor-health-walker-compatibility-tests PRIVATE openemperor-sg3)
    openemperor_enable_warnings(openemperor-health-walker-compatibility-tests)
    add_test(NAME health-walker-compatibility-fingerprints COMMAND openemperor-health-walker-compatibility-tests
        "${OPENEMPEROR_TEST_RESOURCE_ROOT}")
    add_executable(openemperor-unified-painter-tests tests/UnifiedPainterTests.cpp
        src/renderer/StoredGraphicsRenderer.cpp)
    target_link_libraries(openemperor-unified-painter-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-unified-painter-tests)
    add_test(NAME unified-map-sandbox-pixels COMMAND openemperor-unified-painter-tests)

    add_executable(openemperor-scene-composition-tests tests/SceneCompositionTests.cpp
        src/app/MapDebugView.cpp src/renderer/TerrainPreviewRenderer.cpp
        src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp src/app/SandboxUiLayout.cpp
        src/app/RoadDrag.cpp src/app/RoadTopology.cpp src/app/VisualSelection.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
        src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
        src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
    target_link_libraries(openemperor-scene-composition-tests PRIVATE openemperor-recovery
        openemperor-version SDL3::SDL3)
    openemperor_enable_warnings(openemperor-scene-composition-tests)
    add_test(NAME scene-composition-production-pixels COMMAND openemperor-scene-composition-tests)
    set_tests_properties(scene-composition-production-pixels PROPERTIES TIMEOUT 60)

    add_executable(openemperor-texture-compatibility-tests tests/TextureCompatibilityTests.cpp
        src/renderer/BuildingSprite.cpp src/renderer/WalkerSpriteSet.cpp
        src/renderer/RoadSpriteSet.cpp src/renderer/StoredGraphicsRenderer.cpp)
    target_link_libraries(openemperor-texture-compatibility-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-texture-compatibility-tests)
    add_test(NAME texture-compatibility-production-pixels COMMAND openemperor-texture-compatibility-tests)
    set_tests_properties(texture-compatibility-production-pixels PROPERTIES TIMEOUT 300)

    add_executable(openemperor-texture-compatibility-sdl-tests tests/TextureCompatibilitySdlTests.cpp)
    target_include_directories(openemperor-texture-compatibility-sdl-tests PRIVATE src)
    target_link_libraries(openemperor-texture-compatibility-sdl-tests PRIVATE SDL3::SDL3)
    openemperor_enable_warnings(openemperor-texture-compatibility-sdl-tests)
    add_test(NAME texture-compatibility-sdl-pixels COMMAND openemperor-texture-compatibility-sdl-tests)
    set_tests_properties(texture-compatibility-sdl-pixels PROPERTIES TIMEOUT 300)

    add_executable(openemperor-texture-preview-failure-tests tests/TextureCompatibilityFailureTests.cpp)
    target_include_directories(openemperor-texture-preview-failure-tests PRIVATE src)
    target_link_libraries(openemperor-texture-preview-failure-tests PRIVATE SDL3::SDL3)
    openemperor_enable_warnings(openemperor-texture-preview-failure-tests)
    add_test(NAME texture-preview-failure-alpha COMMAND openemperor-texture-preview-failure-tests)
    set_tests_properties(texture-preview-failure-alpha PROPERTIES TIMEOUT 20)

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

# Explicit local diagnostic starter; not a gameplay command or shipped app mode.
add_executable(openemperor-fire-visual-fixture EXCLUDE_FROM_ALL tools/FireVisualFixture.cpp
    src/app/MenuStorage.cpp src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
    src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp
    src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
    src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
    src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
target_link_libraries(openemperor-fire-visual-fixture PRIVATE
    openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-fire-visual-fixture)

add_executable(openemperor-fire-inspector-fixture EXCLUDE_FROM_ALL tools/FireInspectorFixture.cpp
    src/app/MenuStorage.cpp src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
    src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp
    src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
    src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
    src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
target_link_libraries(openemperor-fire-inspector-fixture PRIVATE
    openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-fire-inspector-fixture)

add_executable(openemperor-economy-start-fixture EXCLUDE_FROM_ALL tools/EconomyStartFixture.cpp
    src/app/MenuStorage.cpp src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
    src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp
    src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
    src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
    src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
target_link_libraries(openemperor-economy-start-fixture PRIVATE
    openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-economy-start-fixture)

add_executable(openemperor-market-walker-fixture EXCLUDE_FROM_ALL tools/MarketWalkerFixture.cpp
    src/app/MenuStorage.cpp src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
    src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp
    src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
    src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
    src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
target_link_libraries(openemperor-market-walker-fixture PRIVATE
    openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-market-walker-fixture)

add_executable(openemperor-service-walker-fixture EXCLUDE_FROM_ALL tools/ServiceWalkerFixture.cpp
    src/app/MenuStorage.cpp src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
    src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp
    src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
    src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
    src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
target_link_libraries(openemperor-service-walker-fixture PRIVATE
    openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-service-walker-fixture)

add_executable(openemperor-health-walker-fixture EXCLUDE_FROM_ALL tools/HealthWalkerFixture.cpp
    src/app/MenuStorage.cpp src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
    src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp
    src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
    src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
    src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
target_link_libraries(openemperor-health-walker-fixture PRIVATE
    openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-health-walker-fixture)

# On-demand original-data integration check and isolated player starter. Original
# Xia bytes and generated saves never enter the authored CTest fixtures/bundle.
add_executable(openemperor-city-v16-playthrough-fixture EXCLUDE_FROM_ALL tools/CityV16PlaythroughFixture.cpp
    src/app/MenuStorage.cpp src/app/ResourceLocator.cpp src/app/VisualSelection.cpp
    src/app/SandboxView.cpp src/renderer/BridgeRenderer.cpp src/renderer/RampRenderer.cpp src/renderer/SourceCellRampRenderer.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/FireSpriteSet.cpp src/app/WalkerPose.cpp
    src/app/SandboxUiLayout.cpp src/app/RoadDrag.cpp src/app/RoadTopology.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp
    src/renderer/WalkerSpriteSet.cpp src/renderer/BuildingSprite.cpp src/renderer/RoadSpriteSet.cpp
    src/renderer/WellFallbackRenderer.cpp src/renderer/HealthPostFallbackRenderer.cpp src/renderer/OriginalEliteHouseFallbackRenderer.cpp)
target_link_libraries(openemperor-city-v16-playthrough-fixture PRIVATE
    openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-city-v16-playthrough-fixture)

# Explicit original-data long-play adapter; excluded from normal CTest/package.
get_target_property(city_v16_fixture_sources openemperor-city-v16-playthrough-fixture SOURCES)
list(REMOVE_ITEM city_v16_fixture_sources tools/CityV16PlaythroughFixture.cpp)
add_executable(openemperor-city-v16-longplay-fixture EXCLUDE_FROM_ALL
    tools/CityV16LongplayFixture.cpp ${city_v16_fixture_sources})
# Same World sources, a test-only selection hook, and ordinary movement/demand.
# Direct objects satisfy simulation symbols before the normal static dependencies;
# no alternate World layout, Snapshot fields, or second simulation is introduced.
get_target_property(service_experiment_simulation_sources openemperor-simulation SOURCES)
target_sources(openemperor-city-v16-longplay-fixture PRIVATE
    ${service_experiment_simulation_sources} tools/ServiceSchedulerExperiment.cpp)
target_compile_definitions(openemperor-city-v16-longplay-fixture PRIVATE
    OPENEMPEROR_SERVICE_SCHEDULER_EXPERIMENT=1)
target_include_directories(openemperor-city-v16-longplay-fixture PRIVATE tools)
target_link_libraries(openemperor-city-v16-longplay-fixture PRIVATE
    openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-city-v16-longplay-fixture)

# On-demand read-only original-map corpus diagnosis. Reports and proprietary
# inputs stay under the user's ignored local workspace, outside test fixtures.
add_executable(openemperor-audit-original-occupancy EXCLUDE_FROM_ALL
    tools/AuditOriginalOccupancy.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
target_link_libraries(openemperor-audit-original-occupancy PRIVATE
    openemperor-persistence SDL3::SDL3)
openemperor_enable_warnings(openemperor-audit-original-occupancy)

add_executable(openemperor-original-bridge-audit EXCLUDE_FROM_ALL tools/OriginalBridgeAudit.cpp)
target_link_libraries(openemperor-original-bridge-audit PRIVATE openemperor-persistence)
openemperor_enable_warnings(openemperor-original-bridge-audit)

# Paid technical scenarios on actual original maps; excluded from default tests.
add_executable(openemperor-map-playability-fixture EXCLUDE_FROM_ALL
    tools/MapPlayabilityFixture.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
target_link_libraries(openemperor-map-playability-fixture PRIVATE
    openemperor-persistence SDL3::SDL3)
openemperor_enable_warnings(openemperor-map-playability-fixture)

# Real normally paid rule-4 trips on immutable original roads. Explicitly
# separate from the historical rule-3 A-E fixtures above.
add_executable(openemperor-original-roads-fixture EXCLUDE_FROM_ALL
    tools/OriginalRoadsFixture.cpp
    src/app/RoadTopology.cpp src/app/VisualSelection.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
target_link_libraries(openemperor-original-roads-fixture PRIVATE
    openemperor-persistence SDL3::SDL3)
openemperor_enable_warnings(openemperor-original-roads-fixture)

get_target_property(bridge_fixture_sources openemperor-city-v16-playthrough-fixture SOURCES)
list(REMOVE_ITEM bridge_fixture_sources tools/CityV16PlaythroughFixture.cpp)
# OE32 on-demand measurements use unchanged production simulation/View code.
# Share these presentation objects only between the new QA executables.
add_executable(openemperor-original-longplay-acceptance EXCLUDE_FROM_ALL
    tests/OriginalLongplayAcceptance.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
target_link_libraries(openemperor-original-longplay-acceptance PRIVATE
    openemperor-recovery SDL3::SDL3)
openemperor_enable_warnings(openemperor-original-longplay-acceptance)
# OE35 is diagnostic QA only; it uses the unchanged production World and readers.
add_executable(openemperor-economy-fire-diagnosis
    tools/EconomyFireDiagnosis.cpp
    src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
target_link_libraries(openemperor-economy-fire-diagnosis PRIVATE openemperor-recovery SDL3::SDL3)
openemperor_enable_warnings(openemperor-economy-fire-diagnosis)
if(BUILD_TESTING)
    add_test(NAME original-economy-fire-diagnosis-real COMMAND openemperor-economy-fire-diagnosis
        --mode check --variant supplied --data "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-economy-fire-diagnosis-real PROPERTIES TIMEOUT 600)
    if(Python3_Interpreter_FOUND)
        add_test(NAME original-economy-fire-diagnosis-runner-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/tests" -p EconomyFireDiagnosisRunnerTests.py)
        set_tests_properties(original-economy-fire-diagnosis-runner-contract PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
    endif()
    add_executable(openemperor-original-longplay-tests tests/OriginalLongplayTests.cpp
        src/renderer/StoredGraphicsRenderer.cpp src/renderer/StoredCamera.cpp)
    target_link_libraries(openemperor-original-longplay-tests PRIVATE
        openemperor-recovery SDL3::SDL3)
    openemperor_enable_warnings(openemperor-original-longplay-tests)
    add_test(NAME original-longplay-budgets-synthetic COMMAND openemperor-original-longplay-tests --synthetic)
    add_test(NAME original-longplay-budgets-real COMMAND openemperor-original-longplay-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-longplay-budgets-synthetic original-longplay-budgets-real
        PROPERTIES TIMEOUT 180)
    add_library(openemperor-original-longplay-view-support STATIC ${bridge_fixture_sources})
    target_link_libraries(openemperor-original-longplay-view-support PUBLIC
        openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-original-longplay-view-support)
    add_executable(openemperor-original-longplay-view-tests tests/OriginalLongplayViewTests.cpp)
    target_link_libraries(openemperor-original-longplay-view-tests PRIVATE openemperor-original-longplay-view-support)
    openemperor_enable_warnings(openemperor-original-longplay-view-tests)
    add_dependencies(openemperor-original-longplay-view-tests openemperor)
    # OE34 observes ordinary paid progression; no alternative World or save authority.
    add_executable(openemperor-paid-progression-acceptance tools/PaidProgressionAcceptance.cpp)
    target_link_libraries(openemperor-paid-progression-acceptance PRIVATE openemperor-original-longplay-view-support)
    openemperor_enable_warnings(openemperor-paid-progression-acceptance)
    add_dependencies(openemperor-paid-progression-acceptance openemperor)
    add_executable(openemperor-original-fire-protection-tests tests/OriginalFireProtectionTests.cpp)
    target_link_libraries(openemperor-original-fire-protection-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-fire-protection-tests)
    add_test(NAME original-fire-protection-world COMMAND openemperor-original-fire-protection-tests)
    set_tests_properties(original-fire-protection-world PROPERTIES TIMEOUT 180)
    add_executable(openemperor-original-fire-protection-save-tests tests/OriginalFireProtectionSaveTests.cpp)
    target_link_libraries(openemperor-original-fire-protection-save-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-fire-protection-save-tests)
    add_test(NAME original-fire-protection-save COMMAND openemperor-original-fire-protection-save-tests)
    set_tests_properties(original-fire-protection-save PROPERTIES TIMEOUT 180)
    add_executable(openemperor-fire-protection-acceptance tools/FireProtectionAcceptance.cpp)
    target_link_libraries(openemperor-fire-protection-acceptance PRIVATE openemperor-original-longplay-view-support)
    openemperor_enable_warnings(openemperor-fire-protection-acceptance)
    add_dependencies(openemperor-fire-protection-acceptance openemperor)
    add_test(NAME original-fire-protection-real COMMAND openemperor-fire-protection-acceptance
        --mode check --rule 11 --data "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}" --ticks 6000)
    set_tests_properties(original-fire-protection-real PROPERTIES TIMEOUT 240)
    add_executable(openemperor-sustainable-economy-tests tests/SustainableEconomyTests.cpp)
    target_link_libraries(openemperor-sustainable-economy-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-sustainable-economy-tests)
    add_test(NAME original-sustainable-economy-world COMMAND openemperor-sustainable-economy-tests)
    set_tests_properties(original-sustainable-economy-world PROPERTIES TIMEOUT 240)
    add_executable(openemperor-sustainable-economy-acceptance tools/SustainableEconomyAcceptance.cpp)
    target_link_libraries(openemperor-sustainable-economy-acceptance PRIVATE openemperor-original-longplay-view-support)
    openemperor_enable_warnings(openemperor-sustainable-economy-acceptance)
    add_dependencies(openemperor-sustainable-economy-acceptance openemperor)
    add_test(NAME original-sustainable-economy-real COMMAND openemperor-sustainable-economy-acceptance
        --mode check --rule 12 --variant balanced-three --data "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}" --ticks 6000)
    set_tests_properties(original-sustainable-economy-real PROPERTIES TIMEOUT 600)
    add_executable(openemperor-elite-desirability-acceptance tools/EliteDesirabilityAcceptance.cpp)
    target_link_libraries(openemperor-elite-desirability-acceptance PRIVATE openemperor-original-longplay-view-support)
    openemperor_enable_warnings(openemperor-elite-desirability-acceptance)
    add_dependencies(openemperor-elite-desirability-acceptance openemperor)
    add_test(NAME original-elite-desirability-cli COMMAND openemperor-elite-desirability-acceptance --help)
    if(Python3_Interpreter_FOUND)
        add_test(NAME original-elite-desirability-guards COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/tests/test_elite_desirability_cli.py
            --executable $<TARGET_FILE:openemperor-elite-desirability-acceptance>)
        set_tests_properties(original-elite-desirability-guards PROPERTIES TIMEOUT 60
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
    endif()
    add_executable(openemperor-high-tier-progression-acceptance tools/HighTierProgressionAcceptance.cpp)
    target_link_libraries(openemperor-high-tier-progression-acceptance PRIVATE openemperor-original-longplay-view-support)
    openemperor_enable_warnings(openemperor-high-tier-progression-acceptance)
    add_dependencies(openemperor-high-tier-progression-acceptance openemperor)
    add_executable(openemperor-market-distribution-acceptance tools/MarketDistributionAcceptance.cpp)
    target_link_libraries(openemperor-market-distribution-acceptance PRIVATE openemperor-original-longplay-view-support)
    openemperor_enable_warnings(openemperor-market-distribution-acceptance)
    add_dependencies(openemperor-market-distribution-acceptance openemperor)
    add_executable(openemperor-market-needs-policy-tests tests/MarketNeedsPolicyTests.cpp)
    target_link_libraries(openemperor-market-needs-policy-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-market-needs-policy-tests)
    add_test(NAME original-market-needs-policy COMMAND openemperor-market-needs-policy-tests)
    add_executable(openemperor-market-logistics-version-tests tests/MarketLogisticsVersionTests.cpp)
    target_link_libraries(openemperor-market-logistics-version-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-market-logistics-version-tests)
    add_test(NAME original-market-logistics-version-save COMMAND openemperor-market-logistics-version-tests)
    set_tests_properties(original-market-logistics-version-save PROPERTIES TIMEOUT 240)
    add_executable(openemperor-market-logistics-transport-tests tests/MarketLogisticsTransportTests.cpp)
    target_link_libraries(openemperor-market-logistics-transport-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-market-logistics-transport-tests)
    add_test(NAME original-market-logistics-transport COMMAND openemperor-market-logistics-transport-tests)
    set_tests_properties(original-market-logistics-transport PROPERTIES TIMEOUT 600)
    if(Python3_Interpreter_FOUND)
        add_test(NAME original-market-distribution-runner-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/tests" -p MarketDistributionRunnerTests.py)
        set_tests_properties(original-market-distribution-runner-contract PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
    endif()
    add_executable(openemperor-high-tier-progression-matrix-tests tests/HighTierProgressionMatrixTests.cpp)
    target_link_libraries(openemperor-high-tier-progression-matrix-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-high-tier-progression-matrix-tests)
    add_test(NAME original-high-tier-progression-matrix-synthetic COMMAND openemperor-high-tier-progression-matrix-tests)
    add_test(NAME original-high-tier-progression-matrix-real COMMAND openemperor-high-tier-progression-matrix-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-high-tier-progression-matrix-synthetic original-high-tier-progression-matrix-real PROPERTIES TIMEOUT 180)
    set_tests_properties(original-high-tier-progression-matrix-real PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(openemperor-high-tier-progression-authority-tests tests/HighTierProgressionAuthorityTests.cpp)
    target_link_libraries(openemperor-high-tier-progression-authority-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-high-tier-progression-authority-tests)
    add_test(NAME original-high-tier-progression-authority-stage-a COMMAND openemperor-high-tier-progression-authority-tests --stage-a)
    set_tests_properties(original-high-tier-progression-authority-stage-a PROPERTIES TIMEOUT 180)
    add_test(NAME original-high-tier-progression-authority-stage-b COMMAND openemperor-high-tier-progression-authority-tests --stage-b)
    set_tests_properties(original-high-tier-progression-authority-stage-b PROPERTIES TIMEOUT 300)
    add_executable(openemperor-high-tier-des-bound-tests tests/HighTierDesirabilityBoundTests.cpp)
    target_link_libraries(openemperor-high-tier-des-bound-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-high-tier-des-bound-tests)
    add_test(NAME original-high-tier-des-bound-real COMMAND openemperor-high-tier-des-bound-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-high-tier-des-bound-real PROPERTIES TIMEOUT 180 SKIP_RETURN_CODE 77)
    add_executable(openemperor-cardinal-water-real-tests tests/CardinalWaterRealTests.cpp)
    target_link_libraries(openemperor-cardinal-water-real-tests PRIVATE openemperor-original-longplay-view-support)
    openemperor_enable_warnings(openemperor-cardinal-water-real-tests)
    add_test(NAME original-cardinal-water-paid-xia COMMAND openemperor-cardinal-water-real-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-cardinal-water-paid-xia PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    add_executable(openemperor-cardinal-water-tests tests/CardinalWaterTests.cpp)
    target_link_libraries(openemperor-cardinal-water-tests PRIVATE openemperor-recovery ZLIB::ZLIB)
    openemperor_enable_warnings(openemperor-cardinal-water-tests)
    add_test(NAME original-cardinal-water-world-save COMMAND openemperor-cardinal-water-tests)
    add_test(NAME original-cardinal-water-real COMMAND openemperor-cardinal-water-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-cardinal-water-world-save original-cardinal-water-real PROPERTIES TIMEOUT 300)
    add_executable(openemperor-inspector-coordination-world-tests tests/InspectorCoordinationWorldTests.cpp)
    target_link_libraries(openemperor-inspector-coordination-world-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-inspector-coordination-world-tests)
    add_test(NAME original-inspector-coordination-world COMMAND openemperor-inspector-coordination-world-tests)
    set_tests_properties(original-inspector-coordination-world PROPERTIES TIMEOUT 300)
    add_executable(openemperor-inspector-coordination-save-tests tests/InspectorCoordinationSaveTests.cpp)
    target_link_libraries(openemperor-inspector-coordination-save-tests PRIVATE openemperor-recovery ZLIB::ZLIB)
    openemperor_enable_warnings(openemperor-inspector-coordination-save-tests)
    add_test(NAME original-inspector-coordination-save COMMAND openemperor-inspector-coordination-save-tests)
    set_tests_properties(original-inspector-coordination-save PROPERTIES TIMEOUT 300)
    add_executable(openemperor-deadline-inspector-world-tests tests/DeadlineInspectorWorldTests.cpp)
    target_link_libraries(openemperor-deadline-inspector-world-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-deadline-inspector-world-tests)
    add_test(NAME original-deadline-inspector-world COMMAND openemperor-deadline-inspector-world-tests)
    set_tests_properties(original-deadline-inspector-world PROPERTIES TIMEOUT 300)
    add_executable(openemperor-deadline-inspector-save-tests tests/DeadlineInspectorSaveTests.cpp)
    target_link_libraries(openemperor-deadline-inspector-save-tests PRIVATE openemperor-recovery ZLIB::ZLIB)
    openemperor_enable_warnings(openemperor-deadline-inspector-save-tests)
    add_test(NAME original-deadline-inspector-save COMMAND openemperor-deadline-inspector-save-tests)
    set_tests_properties(original-deadline-inspector-save PROPERTIES TIMEOUT 300)
    add_executable(openemperor-deadline-service-world-tests tests/DeadlineServiceWorldTests.cpp)
    target_link_libraries(openemperor-deadline-service-world-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-deadline-service-world-tests)
    add_test(NAME original-deadline-service-world COMMAND openemperor-deadline-service-world-tests)
    set_tests_properties(original-deadline-service-world PROPERTIES TIMEOUT 240)
    add_executable(openemperor-deadline-service-save-tests tests/DeadlineServiceSaveTests.cpp)
    target_link_libraries(openemperor-deadline-service-save-tests PRIVATE openemperor-recovery ZLIB::ZLIB)
    openemperor_enable_warnings(openemperor-deadline-service-save-tests)
    add_test(NAME original-deadline-service-save COMMAND openemperor-deadline-service-save-tests)
    set_tests_properties(original-deadline-service-save PROPERTIES TIMEOUT 300)
    add_executable(openemperor-deadline-food-world-tests tests/DeadlineFoodWorldTests.cpp)
    target_link_libraries(openemperor-deadline-food-world-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-deadline-food-world-tests)
    add_test(NAME original-deadline-food-world COMMAND openemperor-deadline-food-world-tests)
    set_tests_properties(original-deadline-food-world PROPERTIES TIMEOUT 240)
    add_executable(openemperor-deadline-food-save-tests tests/DeadlineFoodSaveTests.cpp)
    target_link_libraries(openemperor-deadline-food-save-tests PRIVATE openemperor-recovery ZLIB::ZLIB)
    openemperor_enable_warnings(openemperor-deadline-food-save-tests)
    add_test(NAME original-deadline-food-save COMMAND openemperor-deadline-food-save-tests)
    set_tests_properties(original-deadline-food-save PROPERTIES TIMEOUT 300)
    add_executable(openemperor-original-safe-demolition-world-tests tests/OriginalSafeDemolitionWorldTests.cpp)
    target_link_libraries(openemperor-original-safe-demolition-world-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-original-safe-demolition-world-tests)
    add_test(NAME original-safe-demolition-world COMMAND openemperor-original-safe-demolition-world-tests)
    set_tests_properties(original-safe-demolition-world PROPERTIES TIMEOUT 240)
    add_executable(openemperor-original-safe-demolition-save-tests tests/SafeDemolitionSaveTests.cpp)
    target_link_libraries(openemperor-original-safe-demolition-save-tests PRIVATE openemperor-recovery ZLIB::ZLIB)
    openemperor_enable_warnings(openemperor-original-safe-demolition-save-tests)
    add_test(NAME original-safe-demolition-save COMMAND openemperor-original-safe-demolition-save-tests)
    set_tests_properties(original-safe-demolition-save PROPERTIES TIMEOUT 300)
    add_executable(openemperor-cardinal-water-core-tests tests/CardinalWaterCoreTests.cpp)
    target_link_libraries(openemperor-cardinal-water-core-tests PRIVATE openemperor-simulation)
    openemperor_enable_warnings(openemperor-cardinal-water-core-tests)
    add_test(NAME original-cardinal-water-core COMMAND openemperor-cardinal-water-core-tests)
    add_executable(openemperor-desirability-investigation tools/DesirabilityInvestigation.cpp)
    target_link_libraries(openemperor-desirability-investigation PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-desirability-investigation)
    add_executable(openemperor-desirability-investigation-tests tests/DesirabilityInvestigationTests.cpp)
    target_link_libraries(openemperor-desirability-investigation-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-desirability-investigation-tests)
    add_test(NAME original-desirability-investigation-synthetic COMMAND openemperor-desirability-investigation-tests)
    add_test(NAME original-desirability-investigation-real COMMAND openemperor-desirability-investigation-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-desirability-investigation-synthetic original-desirability-investigation-real
        PROPERTIES TIMEOUT 300)
    set_tests_properties(original-desirability-investigation-real PROPERTIES SKIP_RETURN_CODE 77)
    add_executable(openemperor-high-tier-common-supply-tests tests/HighTierCommonSupplyTests.cpp)
    target_link_libraries(openemperor-high-tier-common-supply-tests PRIVATE openemperor-original-longplay-view-support)
    openemperor_enable_warnings(openemperor-high-tier-common-supply-tests)
    add_test(NAME original-high-tier-common-supply-real COMMAND openemperor-high-tier-common-supply-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-high-tier-common-supply-real PROPERTIES TIMEOUT 600 SKIP_RETURN_CODE 77)
    add_test(NAME original-high-tier-progression-baseline-real COMMAND openemperor-high-tier-progression-acceptance
        --mode check --rule 12 --variant balanced-three --data "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}" --ticks 100000)
    set_tests_properties(original-high-tier-progression-baseline-real PROPERTIES TIMEOUT 1800)
    if(Python3_Interpreter_FOUND)
        add_test(NAME original-high-tier-progression-runner-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/tests" -p HighTierProgressionRunnerTests.py)
        set_tests_properties(original-high-tier-progression-runner-contract PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME original-sustainable-economy-runner-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/tests" -p SustainableEconomyRunnerTests.py)
        set_tests_properties(original-sustainable-economy-runner-contract PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME original-fire-protection-runner-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/tests" -p FireProtectionRunnerTests.py)
        set_tests_properties(original-fire-protection-runner-contract PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
    endif()
    add_executable(openemperor-historical-save-bytes EXCLUDE_FROM_ALL tests/HistoricalSaveBytes.cpp)
    target_link_libraries(openemperor-historical-save-bytes PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-historical-save-bytes)
    add_test(NAME original-paid-progression-real COMMAND openemperor-paid-progression-acceptance
        --mode check --data "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}" --plan supplied --ticks 2000 --hold 3000)
    set_tests_properties(original-paid-progression-real PROPERTIES TIMEOUT 180)
    if(Python3_Interpreter_FOUND)
        add_test(NAME original-paid-progression-runner-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/tests" -p PaidProgressionRunnerTests.py)
        set_tests_properties(original-paid-progression-runner-contract PROPERTIES TIMEOUT 30
            ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
    endif()
    add_executable(openemperor-original-longplay-benchmark EXCLUDE_FROM_ALL tests/OriginalLongplayBenchmark.cpp)
    target_link_libraries(openemperor-original-longplay-benchmark PRIVATE openemperor-original-longplay-view-support)
    openemperor_enable_warnings(openemperor-original-longplay-benchmark)
    add_dependencies(openemperor-original-longplay-benchmark openemperor)
    foreach(longplay_scenario original-starter original-districts)
        add_test(NAME original-longplay-view-${longplay_scenario}
            COMMAND openemperor-original-longplay-view-tests --data "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}"
                --resources "${OPENEMPEROR_TEST_RESOURCE_ROOT}" --scenario ${longplay_scenario})
        set_tests_properties(original-longplay-view-${longplay_scenario} PROPERTIES TIMEOUT 240)
    endforeach()
    if(Python3_Interpreter_FOUND)
        add_test(NAME original-longplay-runner-contract COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/tests" -p OriginalLongplayRunnerTests.py)
        set_tests_properties(original-longplay-runner-contract PROPERTIES TIMEOUT 30)
    endif()
endif()
add_executable(openemperor-original-influence-fixture EXCLUDE_FROM_ALL tools/OriginalInfluenceFixture.cpp)
target_link_libraries(openemperor-original-influence-fixture PRIVATE openemperor-persistence)
openemperor_enable_warnings(openemperor-original-influence-fixture)
add_executable(openemperor-original-influence-play EXCLUDE_FROM_ALL
    tools/OriginalInfluencePlay.cpp ${bridge_fixture_sources})
target_compile_definitions(openemperor-original-influence-play PRIVATE OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE=1)
target_link_libraries(openemperor-original-influence-play PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-original-influence-play)
add_executable(openemperor-original-goods-fixture EXCLUDE_FROM_ALL tools/OriginalGoodsFixture.cpp)
target_link_libraries(openemperor-original-goods-fixture PRIVATE openemperor-persistence)
openemperor_enable_warnings(openemperor-original-goods-fixture)
add_executable(openemperor-original-goods-play EXCLUDE_FROM_ALL
    tools/OriginalGoodsPlay.cpp ${bridge_fixture_sources})
target_compile_definitions(openemperor-original-goods-play PRIVATE OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE=1)
target_link_libraries(openemperor-original-goods-play PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-original-goods-play)
add_executable(openemperor-original-production-play EXCLUDE_FROM_ALL
    tools/OriginalProductionPlay.cpp src/app/ResourceLocator.cpp ${bridge_fixture_sources})
target_compile_definitions(openemperor-original-production-play PRIVATE OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE=1)
target_link_libraries(openemperor-original-production-play PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-original-production-play)
set_property(TARGET openemperor-original-production-play APPEND PROPERTY LINK_DEPENDS ${OPENEMPEROR_COMPATIBILITY_FILES})
add_custom_command(TARGET openemperor-original-production-play POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:openemperor-original-production-play>/resources/Compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENEMPEROR_COMPATIBILITY_FILES}
        "$<TARGET_FILE_DIR:openemperor-original-production-play>/resources/Compatibility/${OPENEMPEROR_COMPATIBILITY_PACK_ID}"
    VERBATIM)
add_executable(openemperor-original-production-fixture EXCLUDE_FROM_ALL tools/OriginalProductionFixture.cpp)
target_link_libraries(openemperor-original-production-fixture PRIVATE openemperor-persistence)
openemperor_enable_warnings(openemperor-original-production-fixture)
add_executable(openemperor-original-finance-fixture EXCLUDE_FROM_ALL tools/OriginalFinanceFixture.cpp)
target_link_libraries(openemperor-original-finance-fixture PRIVATE openemperor-persistence)
openemperor_enable_warnings(openemperor-original-finance-fixture)
if(BUILD_TESTING)
    add_executable(openemperor-original-influence-view-tests tests/OriginalInfluenceViewTests.cpp ${bridge_fixture_sources})
    target_compile_definitions(openemperor-original-influence-view-tests PRIVATE OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE=1)
    target_link_libraries(openemperor-original-influence-view-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-original-influence-view-tests)
    add_test(NAME original-influence-view COMMAND openemperor-original-influence-view-tests)
    set_tests_properties(original-influence-view PROPERTIES TIMEOUT 60)
    add_executable(openemperor-original-goods-view-tests tests/OriginalGoodsViewTests.cpp ${bridge_fixture_sources})
    target_compile_definitions(openemperor-original-goods-view-tests PRIVATE OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE=1)
    target_link_libraries(openemperor-original-goods-view-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-original-goods-view-tests)
    add_test(NAME original-goods-view COMMAND openemperor-original-goods-view-tests)
    set_tests_properties(original-goods-view PROPERTIES TIMEOUT 60)
    add_executable(openemperor-original-production-view-tests tests/OriginalProductionViewTests.cpp ${bridge_fixture_sources})
    target_compile_definitions(openemperor-original-production-view-tests PRIVATE OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE=1)
    target_link_libraries(openemperor-original-production-view-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-original-production-view-tests)
    add_test(NAME original-production-view COMMAND openemperor-original-production-view-tests)
    # The paid Elite/nine-service sequence runs about twice the prior ticks;
    # unchanged Debug navigation assertions measured 75.5s in isolation.
    set_tests_properties(original-production-view PROPERTIES TIMEOUT 120)

    add_executable(openemperor-original-recovery-view-tests tests/OriginalRecoveryViewTests.cpp ${bridge_fixture_sources})
    target_link_libraries(openemperor-original-recovery-view-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-original-recovery-view-tests)
    add_test(NAME original-recovery-view COMMAND openemperor-original-recovery-view-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}")
    set_tests_properties(original-recovery-view PROPERTIES TIMEOUT 240)

    add_executable(openemperor-original-house-inspector-view-tests tests/OriginalHouseInspectorViewTests.cpp ${bridge_fixture_sources})
    target_compile_definitions(openemperor-original-house-inspector-view-tests PRIVATE OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE=1)
    target_link_libraries(openemperor-original-house-inspector-view-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-original-house-inspector-view-tests)
    add_test(NAME original-house-inspector-view COMMAND openemperor-original-house-inspector-view-tests)
    set_tests_properties(original-house-inspector-view PROPERTIES TIMEOUT 120)
    add_executable(openemperor-original-safe-demolition-view-tests tests/OriginalSafeDemolitionViewTests.cpp ${bridge_fixture_sources})
    target_compile_definitions(openemperor-original-safe-demolition-view-tests PRIVATE OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE=1)
    target_link_libraries(openemperor-original-safe-demolition-view-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-original-safe-demolition-view-tests)
    add_test(NAME original-safe-demolition-view COMMAND openemperor-original-safe-demolition-view-tests)
    set_tests_properties(original-safe-demolition-view PROPERTIES TIMEOUT 120)

    add_executable(openemperor-original-building-visual-view-tests tests/OriginalBuildingVisualViewTests.cpp ${bridge_fixture_sources})
    target_compile_definitions(openemperor-original-building-visual-view-tests PRIVATE OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE=1)
    target_link_libraries(openemperor-original-building-visual-view-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-original-building-visual-view-tests)
    add_dependencies(openemperor-original-building-visual-view-tests openemperor)
    add_test(NAME original-building-selection-wiring COMMAND openemperor-original-building-visual-view-tests --selection-only)
    set_tests_properties(original-building-selection-wiring PROPERTIES TIMEOUT 60)
    set(OPENEMPEROR_OE52_PAID_TEST_SAVE "" CACHE FILEPATH "Explicit earned Rule18/schema34 city for OE52 visual acceptance")
    if(OPENEMPEROR_OE52_PAID_TEST_SAVE)
        if(NOT EXISTS "${OPENEMPEROR_OE52_PAID_TEST_SAVE}" OR NOT OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT)
            message(FATAL_ERROR "OE52 paid View requires an existing explicit save and original data root")
        endif()
        add_test(NAME original-building-paid-production-view COMMAND openemperor-original-building-visual-view-tests
            "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}" "${CMAKE_SOURCE_DIR}" "${OPENEMPEROR_OE52_PAID_TEST_SAVE}"
            "${CMAKE_SOURCE_DIR}/.local/oe52/ctest-${CMAKE_BUILD_TYPE}")
        set_tests_properties(original-building-paid-production-view PROPERTIES TIMEOUT 240)
    endif()

    add_executable(openemperor-original-house-visual-view-tests tests/OriginalHouseVisualViewTests.cpp ${bridge_fixture_sources})
    target_compile_definitions(openemperor-original-house-visual-view-tests PRIVATE OPENEMPEROR_ORIGINAL_ECONOMY_SAVE_FIXTURE=1)
    target_link_libraries(openemperor-original-house-visual-view-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-original-house-visual-view-tests)
    add_dependencies(openemperor-original-house-visual-view-tests openemperor)
    add_test(NAME original-house-visual-production-view COMMAND openemperor-original-house-visual-view-tests
        --real "${OPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT}" "${OPENEMPEROR_TEST_RESOURCE_ROOT}")
    set_tests_properties(original-house-visual-production-view PROPERTIES TIMEOUT 240)
endif()
add_executable(openemperor-city-v16-bridges-fixture EXCLUDE_FROM_ALL
    tools/CityV16BridgesFixture.cpp ${bridge_fixture_sources})
target_link_libraries(openemperor-city-v16-bridges-fixture PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-city-v16-bridges-fixture)

add_executable(openemperor-original-height-audit EXCLUDE_FROM_ALL tools/OriginalHeightAudit.cpp)
target_link_libraries(openemperor-original-height-audit PRIVATE openemperor-persistence)
openemperor_enable_warnings(openemperor-original-height-audit)
if(BUILD_TESTING)
    add_executable(openemperor-ramp-transport-tests tests/RampTransportTests.cpp)
    target_link_libraries(openemperor-ramp-transport-tests PRIVATE openemperor-persistence)
    openemperor_enable_warnings(openemperor-ramp-transport-tests)
    add_test(NAME ramp-transport COMMAND openemperor-ramp-transport-tests)
endif()

add_executable(openemperor-city-v16-ramps-fixture EXCLUDE_FROM_ALL tools/CityV16RampsFixture.cpp ${bridge_fixture_sources})
target_link_libraries(openemperor-city-v16-ramps-fixture PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-city-v16-ramps-fixture)

add_executable(openemperor-cliff-ramp-visual-fixture EXCLUDE_FROM_ALL
    tools/CliffRampVisualFixture.cpp ${bridge_fixture_sources})
target_link_libraries(openemperor-cliff-ramp-visual-fixture PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-cliff-ramp-visual-fixture)

add_executable(openemperor-multi-ramp-visual-fixture EXCLUDE_FROM_ALL
    tools/MultiRampVisualFixture.cpp ${bridge_fixture_sources})
target_link_libraries(openemperor-multi-ramp-visual-fixture PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
openemperor_enable_warnings(openemperor-multi-ramp-visual-fixture)

# Explicit standalone experiment: no production renderer, World or save hooks.
add_executable(openemperor-original-ramp-prototype EXCLUDE_FROM_ALL
    tools/OriginalRampPrototype.cpp tools/original-ramp/CellTerrain.cpp)
target_link_libraries(openemperor-original-ramp-prototype PRIVATE openemperor-maps SDL3::SDL3 ZLIB::ZLIB)
target_compile_definitions(openemperor-original-ramp-prototype PRIVATE
    OPENEMPEROR_PROTOTYPE_RESOURCES="${CMAKE_CURRENT_SOURCE_DIR}/resources/compatibility/gog-derived-2.0.0.2-en-assetset-1")
openemperor_enable_warnings(openemperor-original-ramp-prototype)
if(BUILD_TESTING)
    add_executable(openemperor-original-ramp-cell-tests
        tests/OriginalRampCellTerrainTests.cpp tools/original-ramp/CellTerrain.cpp)
    target_include_directories(openemperor-original-ramp-cell-tests PRIVATE tools)
    openemperor_enable_warnings(openemperor-original-ramp-cell-tests)
    add_test(NAME original-ramp-cell-terrain COMMAND openemperor-original-ramp-cell-tests)
endif()

if(BUILD_TESTING)
    add_executable(openemperor-ramp-rendering-tests tests/RampRenderingTests.cpp src/renderer/RampRenderer.cpp)
    target_link_libraries(openemperor-ramp-rendering-tests PRIVATE openemperor-scene SDL3::SDL3)
    openemperor_enable_warnings(openemperor-ramp-rendering-tests)
    add_test(NAME ramp-rendering-pixels COMMAND openemperor-ramp-rendering-tests)
    add_executable(openemperor-ramp-material-rendering-tests tests/RampMaterialRenderingTests.cpp src/renderer/RampRenderer.cpp)
    target_link_libraries(openemperor-ramp-material-rendering-tests PRIVATE openemperor-scene SDL3::SDL3)
    openemperor_enable_warnings(openemperor-ramp-material-rendering-tests)
    add_test(NAME ramp-material-rendering-pixels COMMAND openemperor-ramp-material-rendering-tests)
endif()

if(BUILD_TESTING)
    add_test(NAME sandbox-ramps-view COMMAND openemperor-sandbox-view-tests --ramps-only)
endif()

if(BUILD_TESTING)
    add_executable(openemperor-authored-terrain-side-tests tests/AuthoredTerrainSideTests.cpp src/renderer/AuthoredTerrainSides.cpp src/renderer/RampRenderer.cpp)
    target_link_libraries(openemperor-authored-terrain-side-tests PRIVATE openemperor-maps SDL3::SDL3)
    openemperor_enable_warnings(openemperor-authored-terrain-side-tests)
    add_test(NAME authored-terrain-side-pixels COMMAND openemperor-authored-terrain-side-tests)
endif()

if(BUILD_TESTING)
    add_executable(openemperor-authored-land-trade-world-tests tests/AuthoredLandTradeWorldTests.cpp)
    target_link_libraries(openemperor-authored-land-trade-world-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-authored-land-trade-world-tests)
    add_test(NAME authored-land-trade-world COMMAND openemperor-authored-land-trade-world-tests)
    set_tests_properties(authored-land-trade-world PROPERTIES TIMEOUT 180)
    add_executable(openemperor-authored-land-trade-save-tests tests/AuthoredLandTradeSaveTests.cpp)
    target_link_libraries(openemperor-authored-land-trade-save-tests PRIVATE openemperor-recovery ZLIB::ZLIB)
    openemperor_enable_warnings(openemperor-authored-land-trade-save-tests)
    add_test(NAME authored-land-trade-save COMMAND openemperor-authored-land-trade-save-tests)
    set_tests_properties(authored-land-trade-save PROPERTIES TIMEOUT 180)
    add_executable(openemperor-authored-land-trade-view-tests tests/AuthoredLandTradeViewTests.cpp ${bridge_fixture_sources})
    target_link_libraries(openemperor-authored-land-trade-view-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-authored-land-trade-view-tests)
    add_test(NAME authored-land-trade-view COMMAND openemperor-authored-land-trade-view-tests)
    set_tests_properties(authored-land-trade-view PROPERTIES TIMEOUT 240 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(openemperor-authored-land-trade-menu-tests tests/AuthoredLandTradeMenuTests.cpp ${original_menu_test_sources})
    target_link_libraries(openemperor-authored-land-trade-menu-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version Threads::Threads)
    openemperor_enable_warnings(openemperor-authored-land-trade-menu-tests)
    add_test(NAME authored-land-trade-menu COMMAND openemperor-authored-land-trade-menu-tests)
    set_tests_properties(authored-land-trade-menu PROPERTIES TIMEOUT 120 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(openemperor-source-cell-ramp-world-tests tests/SourceCellRampWorldTests.cpp)
    target_link_libraries(openemperor-source-cell-ramp-world-tests PRIVATE openemperor-recovery)
    openemperor_enable_warnings(openemperor-source-cell-ramp-world-tests)
    add_test(NAME source-cell-ramp-world COMMAND openemperor-source-cell-ramp-world-tests)
    set_tests_properties(source-cell-ramp-world PROPERTIES TIMEOUT 180)
    add_executable(openemperor-source-cell-ramp-save-tests tests/SourceCellRampSaveTests.cpp)
    target_link_libraries(openemperor-source-cell-ramp-save-tests PRIVATE openemperor-recovery ZLIB::ZLIB)
    openemperor_enable_warnings(openemperor-source-cell-ramp-save-tests)
    add_test(NAME source-cell-ramp-save COMMAND openemperor-source-cell-ramp-save-tests)
    set_tests_properties(source-cell-ramp-save PROPERTIES TIMEOUT 180)
    add_executable(openemperor-source-cell-ramp-rendering-tests tests/SourceCellRampRenderingTests.cpp src/renderer/SourceCellRampRenderer.cpp)
    target_link_libraries(openemperor-source-cell-ramp-rendering-tests PRIVATE openemperor-recovery SDL3::SDL3)
    openemperor_enable_warnings(openemperor-source-cell-ramp-rendering-tests)
    add_test(NAME source-cell-ramp-rendering COMMAND openemperor-source-cell-ramp-rendering-tests)
endif()

if(BUILD_TESTING)
    add_executable(openemperor-source-cell-ramp-view-tests tests/SourceCellRampViewTests.cpp ${bridge_fixture_sources})
    target_link_libraries(openemperor-source-cell-ramp-view-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version)
    openemperor_enable_warnings(openemperor-source-cell-ramp-view-tests)
    add_test(NAME source-cell-ramp-view COMMAND openemperor-source-cell-ramp-view-tests)
    set_tests_properties(source-cell-ramp-view PROPERTIES TIMEOUT 240 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(openemperor-source-cell-ramp-menu-tests tests/SourceCellRampMenuTests.cpp ${original_menu_test_sources})
    target_link_libraries(openemperor-source-cell-ramp-menu-tests PRIVATE openemperor-recovery SDL3::SDL3 openemperor-version Threads::Threads)
    openemperor_enable_warnings(openemperor-source-cell-ramp-menu-tests)
    add_test(NAME source-cell-ramp-menu COMMAND openemperor-source-cell-ramp-menu-tests)
    set_tests_properties(source-cell-ramp-menu PROPERTIES TIMEOUT 120)
endif()
