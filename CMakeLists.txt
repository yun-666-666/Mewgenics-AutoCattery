cmake_minimum_required(VERSION 3.25)

project(AutoCattery VERSION 0.5.4 LANGUAGES C CXX RC)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "AutoCattery phase 01 supports Windows x64 only.")
endif()

option(AUTOCATTERY_BUILD_TESTS "Build AutoCattery unit tests" ON)

include(cmake/Warnings.cmake)

add_library(mew_ui_api STATIC
    third_party/mew_ui_api/src/native/mew_ui_api.c
)
target_include_directories(mew_ui_api
    PUBLIC
        third_party/mew_ui_api/src/native
)
target_compile_definitions(mew_ui_api
    PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
)
if(MSVC)
    target_compile_options(mew_ui_api PRIVATE /W3 /utf-8)
endif()
target_link_libraries(mew_ui_api PUBLIC user32 kernel32)

add_library(auto_cattery_core STATIC
    src/breeding/breeding_ranker.cpp
    src/breeding/breeding_scorer.cpp
    src/breeding/pair_ranker.cpp
    src/breeding/pair_trait_scorer.cpp
    src/classification/classifier.cpp
    src/classification/protection_adapter.cpp
    src/diagnostics/cat_data_collector.cpp
    src/execution/plan_sealer.cpp
    src/execution/plan_sealer_snapshot.cpp
    src/execution/precondition_validator.cpp
    src/execution/backup_service.cpp
    src/execution/file_safety.cpp
    src/execution/journal_store.cpp
    src/execution/recovery_package.cpp
    src/execution/game_write_adapter.cpp
    src/execution/transaction_executor.cpp
    src/protection/policy.cpp
    src/protection/sidecar.cpp
    src/protection/sidecar_writer.cpp
    src/protection/identity.cpp
    src/protection/editor_model.cpp
    src/recommendation/candidate_source.cpp
    src/recommendation/mapping_probe.cpp
    src/recommendation/ranking_provider.cpp
    src/recommendation/snapshot_reader.cpp
    src/recommendation/snapshot_validator.cpp
    src/room_planning/capability_adapter.cpp
    src/room_planning/balanced_move_only_assignment.cpp
    src/room_planning/balanced_move_only_breeding.cpp
    src/room_planning/balanced_move_only_context.cpp
    src/room_planning/balanced_move_only_fixed.cpp
    src/room_planning/balanced_move_only_planner.cpp
    src/room_planning/balanced_move_only_preferences.cpp
    src/room_planning/balanced_move_only_targets.cpp
    src/room_planning/planner.cpp
    src/room_planning/validator.cpp
    src/config.cpp
    src/config/decoder.cpp
    src/config/defaults.cpp
    src/config/migration.cpp
    src/config/reader.cpp
    src/config/validation.cpp
    src/config_runtime.cpp
    src/logger.cpp
    src/module_registry.cpp
    src/scoring/combat_ranker.cpp
    src/scoring/combat_ranking_cache.cpp
    src/scoring/combat_scorer.cpp
    src/save_safety/file_hash.cpp
    src/save_safety/atomic_file_replace.cpp
    src/save_safety/backup_catalog.cpp
    src/save_safety/backup_manifest_store.cpp
    src/save_safety/game_process_probe.cpp
    src/save_safety/game_build_gate.cpp
    src/save_safety/restore_service.cpp
    src/save_safety/safe_path.cpp
    src/save_safety/save_stability.cpp
    src/save_safety/single_cat_move_test_service.cpp
    src/save_safety/test_copy_house_state_store.cpp
    src/save_safety/test_copy_guard.cpp
    src/snapshot/cat_blob_parser.cpp
    src/snapshot/cat_personality.cpp
    src/snapshot/domain.cpp
    src/snapshot/furniture_catalog.cpp
    src/snapshot/furniture_room_attributes.cpp
    src/snapshot/house_state_parser.cpp
    src/snapshot/house_state_writer.cpp
    src/snapshot/lz4_block.cpp
    src/snapshot/mutation_catalog.cpp
    src/snapshot/save_database.cpp
    src/snapshot/save_database_files.cpp
    src/snapshot/save_locator.cpp
    src/snapshot/save_snapshot_adapter.cpp
    src/snapshot/snapshot_assembler.cpp
    src/snapshot/pedigree_parser.cpp
    src/snapshot/progress_unlocks.cpp
    src/snapshot/unlocked_breeding_data.cpp
    src/snapshot/visual_part_slots.cpp
    src/snapshot/win_sqlite_api.cpp
    src/workflow/organize_workflow_facade.cpp
    src/workflow/digests.cpp
    src/workflow/move_only_protection.cpp
    src/workflow/execution_router.cpp
    src/workflow/preview_builder.cpp
    src/workflow/preview_store.cpp
    src/workflow/recommendation_snapshot_writer.cpp
    src/workflow/state_machine.cpp
    src/settings_file_editor.cpp
    src/ui/house_button_controller.cpp
    src/ui/house_move_probe_controller.cpp
    src/ui/house_move_probe_mapper.cpp
    src/ui/house_move_probe_report.cpp
    src/ui/house_move_probe_session.cpp
    src/ui/in_game_panel_controller.cpp
    src/ui/in_game_panel_current_save.cpp
    src/ui/in_game_panel_protection.cpp
    src/ui/in_game_panel_render.cpp
    src/ui/in_game_panel_settings.cpp
    src/ui/in_game_preview_model.cpp
    src/ui/in_game_settings_input.cpp
    src/ui/in_game_settings_model.cpp
    src/ui/in_game_settings_pages.cpp
    src/ui/mew_ui_house_button_view.cpp
    src/ui/mew_ui_house_cat_probe.c
    src/ui/mew_ui_live_house_state.c
    src/ui/mew_ui_house_move_capture.c
    src/ui/mew_ui_house_move_adapter.c
    src/ui/mew_ui_house_move_invoke.c
    src/ui/mew_ui_house_move_probe.c
    src/ui/mew_ui_house_move_room_scan.c
    src/ui/mew_ui_house_detail_adapter.c
    src/ui/mew_ui_mapping_probe.c
    src/ui/mew_ui_management_panel_input.cpp
    src/ui/mew_ui_management_panel_edit.cpp
    src/ui/mew_ui_management_panel_view.cpp
    src/ui/mew_ui_movie_clip.cpp
    src/ui/recommendation_marker_controller.cpp
    src/ui/runtime_house_move_gateway.cpp
    src/ui/runtime_house_state_capture.cpp
    src/ui/runtime_room_resolution.cpp
    src/ui/runtime_snapshot_overlay.cpp
    src/ui/runtime_matched_save_snapshot_adapter.cpp
    src/ui/mew_ui_recommendation_marker_view.cpp
    src/ui/scene_context.cpp
    src/ui/mew_ui_bridge.cpp
    src/ui/mew_ui_config_binding.cpp
    src/ui/mew_ui_scene_components.c
    src/ui/mew_ui_scene_probe.c
)
target_link_libraries(auto_cattery_core PUBLIC mew_ui_api bcrypt)
target_compile_features(auto_cattery_core PUBLIC cxx_std_20)
target_include_directories(auto_cattery_core
    PUBLIC
        include
        third_party/mewjector
    PRIVATE
        src/ui
        third_party/json/include
)
target_compile_definitions(auto_cattery_core
    PUBLIC
        AUTOCATTERY_VERSION="${PROJECT_VERSION}"
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)
autocattery_enable_warnings(auto_cattery_core)

add_library(AutoCattery SHARED src/mod_main.cpp)
target_link_libraries(AutoCattery PRIVATE auto_cattery_core)
target_include_directories(AutoCattery PRIVATE third_party/mewjector)
target_compile_features(AutoCattery PRIVATE cxx_std_20)
autocattery_enable_warnings(AutoCattery)
set_target_properties(AutoCattery PROPERTIES
    OUTPUT_NAME "AutoCattery"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
)
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(AutoCattery PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_BINARY_DIR}/out/${config}"
    )
endforeach()

configure_file(
    src/settings_app/settings_app.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/settings_app.rc
    @ONLY
)

add_executable(AutoCatterySettings WIN32
    src/settings_app/settings_app.cpp
    src/settings_app/protection_editor.cpp
    src/settings_app/protection_editor_controls.cpp
    src/settings_app/settings_form.cpp
    src/settings_app/settings_form_data.cpp
    src/settings_app/settings_form_schema.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/settings_app.rc
)
target_link_libraries(
    AutoCatterySettings
    PRIVATE auto_cattery_core user32 gdi32
)
target_compile_features(AutoCatterySettings PRIVATE cxx_std_20)
target_compile_definitions(AutoCatterySettings PRIVATE UNICODE _UNICODE)
if(MSVC)
    target_link_options(
        AutoCatterySettings
        PRIVATE
            "/MANIFEST:EMBED"
            "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/src/settings_app/settings_app.manifest"
    )
endif()
autocattery_enable_warnings(AutoCatterySettings)
set_target_properties(AutoCatterySettings PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
)
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(AutoCatterySettings PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_BINARY_DIR}/out/${config}"
    )
endforeach()

add_executable(AutoCatteryRestore tools/restore_main.cpp)
target_link_libraries(AutoCatteryRestore PRIVATE auto_cattery_core)
target_compile_features(AutoCatteryRestore PRIVATE cxx_std_20)
autocattery_enable_warnings(AutoCatteryRestore)
set_target_properties(AutoCatteryRestore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
)

add_executable(AutoCatterySaveLab tools/save_lab_main.cpp)
target_link_libraries(AutoCatterySaveLab PRIVATE auto_cattery_core)
target_compile_features(AutoCatterySaveLab PRIVATE cxx_std_20)
autocattery_enable_warnings(AutoCatterySaveLab)
set_target_properties(AutoCatterySaveLab PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
)
if(MSVC)
    set_target_properties(AutoCatterySaveLab PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(AutoCatteryRestore PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_BINARY_DIR}/out/${config}"
    )
    set_target_properties(AutoCatterySaveLab PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_BINARY_DIR}/out/${config}"
    )
endforeach()

if(AUTOCATTERY_BUILD_TESTS)
    enable_testing()
    add_executable(auto_cattery_tests
        tests/balanced_move_only_planner_tests.cpp
        tests/deterministic_room_assignment_tests.cpp
        tests/breeding_ranker_tests.cpp
        tests/breeding_scorer_tests.cpp
        tests/classifier_tests.cpp
        tests/protection_policy_tests.cpp
        tests/protection_editor_tests.cpp
        tests/protection_sidecar_tests.cpp
        tests/recommendation_mapping_probe_tests.cpp
        tests/recommendation_ranking_provider_tests.cpp
        tests/recommendation_snapshot_reader_tests.cpp
        tests/recommendation_snapshot_validator_tests.cpp
        tests/room_capability_adapter_tests.cpp
        tests/room_planner_tests.cpp
        tests/room_planning_validator_tests.cpp
        tests/combat_ranker_tests.cpp
        tests/combat_ranking_cache_tests.cpp
        tests/combat_scorer_tests.cpp
        tests/config_boundary_tests.cpp
        tests/config_migration_tests.cpp
        tests/config_runtime_tests.cpp
        tests/config_tests.cpp
        tests/execution_precondition_tests.cpp
        tests/execution_storage_tests.cpp
        tests/execution_transaction_success_tests.cpp
        tests/execution_transaction_failure_tests.cpp
        tests/execution_transaction_concurrency_tests.cpp
        tests/atomic_file_replace_tests.cpp
        tests/backup_manifest_tests.cpp
        tests/restore_service_tests.cpp
        tests/single_cat_move_test_service_tests.cpp
        tests/file_hash_tests.cpp
        tests/furniture_attributes_tests.cpp
        tests/game_build_gate_tests.cpp
        tests/cat_blob_parser_tests.cpp
        tests/cat_data_collector_tests.cpp
        tests/module_registry_tests.cpp
        tests/mutation_catalog_tests.cpp
        tests/pedigree_parser_tests.cpp
        tests/pair_ranker_tests.cpp
        tests/organize_workflow_facade_tests.cpp
        tests/workflow_state_machine_tests.cpp
        tests/workflow_digest_tests.cpp
        tests/workflow_preview_builder_tests.cpp
        tests/workflow_preview_store_tests.cpp
        tests/workflow_preview_binding_tests.cpp
        tests/workflow_execution_router_tests.cpp
        tests/workflow_recommendation_writer_tests.cpp
        tests/lz4_block_tests.cpp
        tests/mew_ui_house_cat_probe_tests.cpp
        tests/mew_ui_house_move_probe_tests.cpp
        tests/house_move_probe_session_tests.cpp
        tests/snapshot_domain_tests.cpp
        tests/snapshot_assembler_tests.cpp
        tests/win_sqlite_api_tests.cpp
        tests/house_button_controller_tests.cpp
        tests/in_game_settings_model_tests.cpp
        tests/in_game_preview_model_tests.cpp
        tests/virtual_viewport_tests.cpp
        tests/runtime_house_state_tests.cpp
        tests/house_state_parser_tests.cpp
        tests/house_state_writer_tests.cpp
        tests/recommendation_marker_controller_tests.cpp
        tests/save_database_tests.cpp
        tests/save_locator_tests.cpp
        tests/save_stability_tests.cpp
        tests/scene_context_tests.cpp
        tests/settings_file_editor_tests.cpp
        tests/test_copy_guard_tests.cpp
        tests/test_copy_house_state_store_tests.cpp
        tests/test_main.cpp
    )
    target_link_libraries(auto_cattery_tests PRIVATE auto_cattery_core)
    target_include_directories(
        auto_cattery_tests
        PRIVATE third_party/json/include src/ui
    )
    target_compile_features(auto_cattery_tests PRIVATE cxx_std_20)
    autocattery_enable_warnings(auto_cattery_tests)
    add_test(NAME phase14_unit_tests COMMAND auto_cattery_tests)

    add_executable(snapshot_probe tests/snapshot_probe.cpp)
    target_link_libraries(snapshot_probe PRIVATE auto_cattery_core)
    target_compile_features(snapshot_probe PRIVATE cxx_std_20)
    autocattery_enable_warnings(snapshot_probe)

    add_executable(breeding_data_probe tests/breeding_data_probe.cpp)
    target_link_libraries(breeding_data_probe PRIVATE auto_cattery_core)
    target_compile_features(breeding_data_probe PRIVATE cxx_std_20)
    autocattery_enable_warnings(breeding_data_probe)

    add_executable(
        room_attribute_probe tests/room_attribute_probe.cpp)
    target_link_libraries(room_attribute_probe PRIVATE auto_cattery_core)
    target_compile_features(room_attribute_probe PRIVATE cxx_std_20)
    autocattery_enable_warnings(room_attribute_probe)

    add_executable(save_format_probe tests/save_format_probe.cpp)
    target_link_libraries(save_format_probe PRIVATE auto_cattery_core)
    target_compile_features(save_format_probe PRIVATE cxx_std_20)
    autocattery_enable_warnings(save_format_probe)

    add_executable(dll_smoke_tests tests/dll_smoke_tests.cpp)
    target_compile_features(dll_smoke_tests PRIVATE cxx_std_20)
    autocattery_enable_warnings(dll_smoke_tests)
    add_test(
        NAME phase14_dll_load_smoke
        COMMAND dll_smoke_tests $<TARGET_FILE:AutoCattery>
    )
    add_test(
        NAME phase14_settings_editor_validate
        COMMAND AutoCatterySettings --validate ${CMAKE_SOURCE_DIR}
    )
    add_test(
        NAME phase14_restore_cli_smoke
        COMMAND AutoCatteryRestore --list --backup-root ${CMAKE_BINARY_DIR}/empty-backup-catalog
    )
    add_test(
        NAME phase14_save_lab_cli_smoke
        COMMAND AutoCatterySaveLab --help
    )
endif()
