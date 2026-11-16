if(NOT DEFINED MISTWATCH_RUNTIME_LIBRARY)
  set(MISTWATCH_RUNTIME_LIBRARY AevryndEngine)
endif()

add_executable(MistwatchWorldServer WorldServer.cpp)
target_link_libraries(MistwatchWorldServer PRIVATE ${MISTWATCH_RUNTIME_LIBRARY} ws2_32)
target_compile_definitions(MistwatchWorldServer PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(MistwatchWorldServer PRIVATE /W4 /WX /permissive- /utf-8)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME MistwatchWorldServerLoopbackTest
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyWorldServer.py"
    "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}/LocalWorldVerification.json")
set_tests_properties(MistwatchWorldServerLoopbackTest PROPERTIES TIMEOUT 20 LABELS "mistwatch;network")
add_library(MistwatchLocalWorldClient STATIC LocalWorldClient.cpp)
target_include_directories(MistwatchLocalWorldClient PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(MistwatchLocalWorldClient PUBLIC ${MISTWATCH_RUNTIME_LIBRARY} ws2_32)
target_compile_definitions(MistwatchLocalWorldClient PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(MistwatchLocalWorldClient PRIVATE /W4 /WX /permissive- /utf-8)
option(MISTWATCH_ENABLE_RIVERBANK_BATTLE "Enable the shared riverbank encounter in the private build" OFF)
if(MISTWATCH_ENABLE_RIVERBANK_BATTLE)
  target_sources(MistwatchWorldServer PRIVATE RiverbankEncounter.cpp)
  target_compile_definitions(MistwatchWorldServer PRIVATE AEVRYND_MISTWATCH_BATTLE=1)
  target_compile_definitions(MistwatchLocalWorldClient PUBLIC AEVRYND_MISTWATCH_BATTLE=1)
  add_test(NAME MistwatchRiverbankBattleLoopbackTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankBattle.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}/SharedBattleVerification.json")
  set_tests_properties(MistwatchRiverbankBattleLoopbackTest PROPERTIES TIMEOUT 45 LABELS "mistwatch;combat;network")
  add_test(NAME MistwatchRiverbankCrossingLoopbackTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankCrossing.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(MistwatchRiverbankCrossingLoopbackTest PROPERTIES TIMEOUT 85 LABELS "mistwatch;network;crossing")
endif()
option(MISTWATCH_ENABLE_RIVERBANK_PROFILES "Enable local persistent characters in the private battle build" OFF)
if(MISTWATCH_ENABLE_RIVERBANK_PROFILES)
  if(NOT MISTWATCH_ENABLE_RIVERBANK_BATTLE)
    message(FATAL_ERROR "Riverbank profiles require the shared encounter build")
  endif()
  target_sources(MistwatchWorldServer PRIVATE RiverbankProfileStore.cpp)
  target_link_libraries(MistwatchWorldServer PRIVATE winsqlite3)
  target_compile_definitions(MistwatchWorldServer PRIVATE AEVRYND_MISTWATCH_PROFILES=1)
  target_compile_definitions(MistwatchLocalWorldClient PUBLIC AEVRYND_MISTWATCH_PROFILES=1)
  add_executable(MistwatchRiverbankProfileStoreSmoke RiverbankProfileStore.cpp RiverbankProfileStoreSmoke.cpp)
  target_link_libraries(MistwatchRiverbankProfileStoreSmoke PRIVATE ${MISTWATCH_RUNTIME_LIBRARY} winsqlite3)
  target_compile_definitions(MistwatchRiverbankProfileStoreSmoke PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(MistwatchRiverbankProfileStoreSmoke PRIVATE /W4 /WX /permissive- /utf-8)
  add_test(NAME MistwatchRiverbankProfileStoreTest COMMAND MistwatchRiverbankProfileStoreSmoke "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME MistwatchRiverbankLaunchReceiptTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankLaunchReceipt.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(MistwatchRiverbankLaunchReceiptTest PROPERTIES TIMEOUT 20 LABELS "mistwatch;profile;network")
  add_test(NAME MistwatchRiverbankProfileReconnectTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankProfiles.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME MistwatchRiverbankGuildLoopbackTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankGuild.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}" "$<TARGET_FILE:MistwatchLocalWorldClientSmoke>")
  add_test(NAME MistwatchRiverbankTerritoryLoopbackTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankTerritory.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}" "$<TARGET_FILE:MistwatchLocalWorldClientSmoke>")
  add_test(NAME MistwatchRiverbankExpeditionLoopbackTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankExpedition.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME MistwatchRiverbankLiveReconnectTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankLiveReconnect.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}" "$<TARGET_FILE:MistwatchLocalWorldClientSmoke>")
  add_executable(MistwatchRiverbankTerritoryHudSmoke RiverbankTerritoryHudSmoke.cpp)
  target_compile_definitions(MistwatchRiverbankTerritoryHudSmoke PRIVATE NOMINMAX)
  target_compile_options(MistwatchRiverbankTerritoryHudSmoke PRIVATE /W4 /WX /permissive- /utf-8)
  add_test(NAME MistwatchRiverbankTerritoryHudTest COMMAND MistwatchRiverbankTerritoryHudSmoke)
  set_tests_properties(MistwatchRiverbankTerritoryHudTest PROPERTIES TIMEOUT 10 LABELS "mistwatch;territory")
  add_executable(MistwatchRiverbankPartyMarkerSmoke RiverbankPartyMarkerSmoke.cpp)
  target_link_libraries(MistwatchRiverbankPartyMarkerSmoke PRIVATE ${MISTWATCH_RUNTIME_LIBRARY})
  target_compile_definitions(MistwatchRiverbankPartyMarkerSmoke PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(MistwatchRiverbankPartyMarkerSmoke PRIVATE /W4 /WX /permissive- /utf-8)
  add_test(NAME MistwatchRiverbankPartyMarkerTest COMMAND MistwatchRiverbankPartyMarkerSmoke)
  set_tests_properties(MistwatchRiverbankPartyMarkerTest PROPERTIES TIMEOUT 10 LABELS "mistwatch;party")
  add_test(NAME MistwatchRiverbankPartyLoopbackTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankParty.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}" "$<TARGET_FILE:MistwatchLocalWorldClientSmoke>")
  set_tests_properties(MistwatchRiverbankPartyLoopbackTest PROPERTIES TIMEOUT 25 LABELS "mistwatch;party;network")
  add_test(NAME MistwatchRiverbankPartyCombatLoopbackTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankPartyCombat.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(MistwatchRiverbankPartyCombatLoopbackTest PROPERTIES TIMEOUT 65 LABELS "mistwatch;party;combat;network")
  add_test(NAME MistwatchRiverbankBossGearLoopbackTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankBossGear.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(MistwatchRiverbankBossGearLoopbackTest PROPERTIES TIMEOUT 100 LABELS "mistwatch;gear;network")
  add_test(NAME MistwatchRiverbankTrailSuppliesTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankTrailSupplies.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(MistwatchRiverbankTrailSuppliesTest PROPERTIES TIMEOUT 120 LABELS "mistwatch;profile;field;network")
  add_test(NAME MistwatchRiverbankAsyncRegistrationTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankAsyncRegistration.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(MistwatchRiverbankAsyncRegistrationTest PROPERTIES TIMEOUT 25 LABELS "mistwatch;profile;network")
  add_test(NAME MistwatchRiverbankFullRoomTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/MeasureRiverbankCapacity.py"
      --server "$<TARGET_FILE:MistwatchWorldServer>" --output "${CMAKE_CURRENT_BINARY_DIR}"
      --clients 16 --seconds 10 --verify-full)
  set_tests_properties(MistwatchRiverbankFullRoomTest PROPERTIES TIMEOUT 35 LABELS "mistwatch;capacity;network")
  add_test(NAME MistwatchRiverbankExpandedRoomTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/MeasureRiverbankCapacity.py"
      --server "$<TARGET_FILE:MistwatchWorldServer>" --output "${CMAKE_CURRENT_BINARY_DIR}"
      --clients 32 --capacity 32 --seconds 10 --verify-full)
  set_tests_properties(MistwatchRiverbankExpandedRoomTest PROPERTIES TIMEOUT 45 LABELS "mistwatch;capacity;network")
  add_test(NAME MistwatchRiverbankFieldHuntLoopbackTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankFieldHunt.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(MistwatchRiverbankFieldHuntLoopbackTest PROPERTIES TIMEOUT 105 LABELS "mistwatch;combat;field;network")
  add_test(NAME MistwatchRiverbankFieldContactTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankFieldContact.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(MistwatchRiverbankFieldContactTest PROPERTIES TIMEOUT 40 LABELS "mistwatch;combat;field;network")
  add_test(NAME MistwatchRiverbankCampRecoveryTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankCampRecovery.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(MistwatchRiverbankCampRecoveryTest PROPERTIES TIMEOUT 50 LABELS "mistwatch;combat;field;network")
  add_test(NAME MistwatchRiverbankCoopJourneyLoopbackTest
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankCoopJourney.py"
      "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(MistwatchRiverbankCoopJourneyLoopbackTest PROPERTIES TIMEOUT 150 LABELS "mistwatch;party;expedition;network")


  set_tests_properties(MistwatchRiverbankProfileStoreTest PROPERTIES TIMEOUT 20 LABELS "mistwatch;profile")
  set_tests_properties(MistwatchRiverbankProfileReconnectTest PROPERTIES TIMEOUT 70 LABELS "mistwatch;profile")
  set_tests_properties(MistwatchRiverbankGuildLoopbackTest PROPERTIES TIMEOUT 30 LABELS "mistwatch;guild;network")
  set_tests_properties(MistwatchRiverbankTerritoryLoopbackTest PROPERTIES TIMEOUT 100 LABELS "mistwatch;guild;territory;network")
  set_tests_properties(MistwatchRiverbankExpeditionLoopbackTest PROPERTIES TIMEOUT 135 LABELS "mistwatch;expedition;network")
  set_tests_properties(MistwatchRiverbankLiveReconnectTest PROPERTIES TIMEOUT 30 LABELS "mistwatch;profile;network")
endif()
add_executable(MistwatchLocalWorldClientSmoke LocalWorldClientSmoke.cpp)
target_link_libraries(MistwatchLocalWorldClientSmoke PRIVATE MistwatchLocalWorldClient)
target_compile_options(MistwatchLocalWorldClientSmoke PRIVATE /W4 /WX /permissive- /utf-8)
add_test(NAME MistwatchNativeLocalWorldClientTest
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyWorldServer.py"
    "$<TARGET_FILE:MistwatchWorldServer>" "${CMAKE_CURRENT_BINARY_DIR}/NativeClientVerification.json"
    "$<TARGET_FILE:MistwatchLocalWorldClientSmoke>")
set_tests_properties(MistwatchNativeLocalWorldClientTest PROPERTIES TIMEOUT 20 LABELS "mistwatch;network")
add_test(NAME MistwatchRiverbankNetworkLatencyTest
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/VerifyRiverbankLatency.py"
    "$<TARGET_FILE:MistwatchWorldServer>" "$<TARGET_FILE:MistwatchLocalWorldClientSmoke>" "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(MistwatchRiverbankNetworkLatencyTest PROPERTIES TIMEOUT 20 LABELS "mistwatch;network")
add_test(NAME MistwatchTailnetAddressValidationTest COMMAND MistwatchLocalWorldClientSmoke --tailnet-addresses)
set_tests_properties(MistwatchTailnetAddressValidationTest PROPERTIES TIMEOUT 5 LABELS "mistwatch;network")

# 專用伺服器與原生 TCP 客戶端的固定驗收，不需要互動式圖形桌面。
if(AEVRYND_BUILD_MISTWATCH_SERVER_ONLY AND MISTWATCH_ENABLE_RIVERBANK_PROFILES)
  set_property(TEST
    MistwatchWorldServerLoopbackTest
    MistwatchNativeLocalWorldClientTest
    MistwatchRiverbankNetworkLatencyTest
    MistwatchRiverbankLaunchReceiptTest
    MistwatchRiverbankCrossingLoopbackTest
    MistwatchRiverbankFullRoomTest
    MistwatchRiverbankExpandedRoomTest
    MistwatchRiverbankCoopJourneyLoopbackTest
    MistwatchRiverbankTerritoryLoopbackTest
    MistwatchRiverbankFieldContactTest
    MistwatchRiverbankLiveReconnectTest
    APPEND PROPERTY LABELS mistwatch-multiplayer-gates)
endif()

option(MISTWATCH_TEST_RIVERBANK_ENCOUNTER "Build the isolated riverbank encounter rules test" OFF)
if(MISTWATCH_TEST_RIVERBANK_ENCOUNTER)
  add_executable(MistwatchRiverbankEncounterSmoke RiverbankEncounter.cpp RiverbankEncounterSmoke.cpp)
  target_link_libraries(MistwatchRiverbankEncounterSmoke PRIVATE ${MISTWATCH_RUNTIME_LIBRARY})
  target_compile_definitions(MistwatchRiverbankEncounterSmoke PRIVATE NOMINMAX)
  target_compile_options(MistwatchRiverbankEncounterSmoke PRIVATE /W4 /WX /permissive- /utf-8)
  add_test(NAME MistwatchRiverbankEncounterTest COMMAND MistwatchRiverbankEncounterSmoke)
  set_tests_properties(MistwatchRiverbankEncounterTest PROPERTIES TIMEOUT 5 LABELS "mistwatch;combat")
endif()
if(MISTWATCH_LOCAL_WORLD_AVATAR AND TARGET AevryndEngine)
  add_executable(MistwatchRiverbankAvatarPoseSmoke RiverbankAvatarPoseSmoke.cpp)
  set_property(SOURCE RiverbankAvatarPoseSmoke.cpp APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/RiverbankAvatarAsset.h" "${CMAKE_CURRENT_SOURCE_DIR}/RiverbankAvatarMotion.h")
  target_include_directories(MistwatchRiverbankAvatarPoseSmoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../riverbank/include")
  target_link_libraries(MistwatchRiverbankAvatarPoseSmoke PRIVATE AevryndEngine d3d12 dxgi dwmapi)
  target_compile_definitions(MistwatchRiverbankAvatarPoseSmoke PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(MistwatchRiverbankAvatarPoseSmoke PRIVATE /W4 /WX /permissive- /utf-8)
  add_test(NAME MistwatchRiverbankAvatarPoseTest
    COMMAND MistwatchRiverbankAvatarPoseSmoke "${MISTWATCH_LOCAL_WORLD_AVATAR}")
  set_tests_properties(MistwatchRiverbankAvatarPoseTest PROPERTIES TIMEOUT 20 LABELS "mistwatch;character")
  foreach(wardrobe IN LISTS MISTWATCH_RIVERBANK_WARDROBES)
    get_filename_component(role_asset "${wardrobe}" NAME_WE)
    add_test(NAME "MistwatchWardrobePose-${role_asset}" COMMAND MistwatchRiverbankAvatarPoseSmoke "${wardrobe}")
    set_tests_properties("MistwatchWardrobePose-${role_asset}" PROPERTIES TIMEOUT 20 LABELS "mistwatch;character")
  endforeach()
endif()
