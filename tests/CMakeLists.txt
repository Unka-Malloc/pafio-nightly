include(FetchContent)
include(GoogleTest)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  DOWNLOAD_EXTRACT_TIMESTAMP true
)

FetchContent_MakeAvailable(googletest)

add_executable(pafio_native_tests
  native/BuildPlanTests.cpp
  native/BuildWorkflowCliTests.cpp
  native/DoctorTests.cpp
  native/LockTests.cpp
  native/ManifestTests.cpp
  native/MetadataTests.cpp
  native/PackTests.cpp
  native/ProcessTests.cpp
  native/PublishTests.cpp
  native/ResolutionContractTests.cpp
  native/RunAndTestCliTests.cpp
  native/SecurityTests.cpp
  native/SyncTests.cpp
  native/TufTests.cpp
  native/VendorTests.cpp
)
target_link_libraries(pafio_native_tests PRIVATE pafio_cli_shell GTest::gtest_main)

gtest_discover_tests(pafio_native_tests DISCOVERY_TIMEOUT 30)

add_test(
  NAME pafio_cli_machine_info_json
  COMMAND bash -c "out=$(\n\"$<TARGET_FILE:pafio>\" machine-info --json\n) && printf '%s\n' \"$out\" | grep -q '\"implementation_language\":\"c++\"' && printf '%s\n' \"$out\" | grep -q '\"metadata\":\\[1\\]' && printf '%s\n' \"$out\" | grep -q '\"resolution\":\\[1\\]' && ! printf '%s\n' \"$out\" | grep -q '\"bootstrap\"' && ! printf '%s\n' \"$out\" | grep -q 'cloud_execution_policy'"
)

add_test(
  NAME pafio_cli_version
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" --version | grep -q '^pafio '"
)

add_test(
  NAME pafio_cli_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" --help | grep -q '^pafio usage:'"
)

add_test(
  NAME pafio_cli_machine_info_rejects_unknown_flag
  COMMAND bash -c "out_file=\$(mktemp) && err_file=\$(mktemp) && trap 'rm -f \"\$out_file\" \"\$err_file\"' EXIT && ! \"$<TARGET_FILE:pafio>\" machine-info --bogus >\"\$out_file\" 2>\"\$err_file\" && grep -q 'machine-info accepts only --json' \"\$err_file\""
)

add_test(
  NAME pafio_cli_doctor_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" doctor --help | grep -q '^usage: pafio doctor \\[--json\\] \\[--manifest-path <path>\\] \\[--styio-bin <path>\\]$'"
)

add_test(
  NAME pafio_cli_metadata_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" metadata --help | grep -q '^usage: pafio metadata --json \\[--manifest-path <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME pafio_cli_tree_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" tree --help | grep -q '^usage: pafio tree \\[--manifest-path <path>\\]$'"
)

add_test(
  NAME pafio_cli_add_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" add --help | grep -q '^usage: pafio add <package-name> (--path <path> | --git <source> --rev <rev> | --registry <url> --version <x.y.z>) \\[--alias <name>\\] \\[--dev\\] \\[--manifest-path <path>\\]$'"
)

add_test(
  NAME pafio_cli_remove_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" remove --help | grep -q '^usage: pafio remove <alias-or-package> \\[--dev\\] \\[--manifest-path <path>\\]$'"
)

add_test(
  NAME pafio_cli_sync_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" sync --help | grep -q '^usage: pafio sync \\[--manifest-path <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME pafio_cli_build_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" build --help | grep -q '^usage: pafio build \\[--manifest-path <path>\\] \\[--package <package-name>\\] \\[--bin <name>\\|--lib\\] \\[--profile <dev\\|release>\\] \\[--dry-run\\] \\[--styio-bin <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME pafio_cli_build_rejects_positional_mode
  COMMAND bash -c "out_file=\$(mktemp) && err_file=\$(mktemp) && trap 'rm -f \"\$out_file\" \"\$err_file\"' EXIT && ! \"$<TARGET_FILE:pafio>\" build minimal >\"\$out_file\" 2>\"\$err_file\" && grep -q 'unexpected argument for build: minimal' \"\$err_file\""
)

add_test(
  NAME pafio_cli_run_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" run --help | grep -q '^usage: pafio run \\[--manifest-path <path>\\] \\[--package <package-name>\\] \\[--bin <name>\\] \\[--profile <dev\\|release>\\] \\[--dry-run\\] \\[--styio-bin <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME pafio_cli_test_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" test --help | grep -q '^usage: pafio test \\[--manifest-path <path>\\] \\[--package <package-name>\\] \\[--test <name>\\] \\[--profile <dev\\|release>\\] \\[--dry-run\\] \\[--styio-bin <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME pafio_cli_vendor_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" vendor --help | grep -q '^usage: pafio vendor \\[--manifest-path <path>\\] \\[--output <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME pafio_cli_pack_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" pack --help | grep -q '^usage: pafio pack \\[--manifest-path <path>\\] \\[--package <package-name>\\] \\[--output <path>\\]$'"
)

add_test(
  NAME pafio_cli_publish_help
  COMMAND bash -c "\"$<TARGET_FILE:pafio>\" publish --help | grep -Fqx 'usage: pafio publish [--manifest-path <path>] [--package <package-name>] [--output <path>] [--registry <http(s)-url>] [--dry-run]'"
)
