include(FetchContent)
include(GoogleTest)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  DOWNLOAD_EXTRACT_TIMESTAMP true
)

FetchContent_MakeAvailable(googletest)

add_executable(spio_native_tests
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
target_link_libraries(spio_native_tests PRIVATE spio_cli_shell GTest::gtest_main)

gtest_discover_tests(spio_native_tests DISCOVERY_TIMEOUT 30)

add_test(
  NAME spio_cli_machine_info_json
  COMMAND bash -c "out=$(\n\"$<TARGET_FILE:spio>\" machine-info --json\n) && printf '%s\n' \"$out\" | grep -q '\"implementation_language\":\"c++\"' && printf '%s\n' \"$out\" | grep -q '\"metadata\":\\[1\\]' && printf '%s\n' \"$out\" | grep -q '\"resolution\":\\[1\\]' && ! printf '%s\n' \"$out\" | grep -q '\"bootstrap\"' && ! printf '%s\n' \"$out\" | grep -q 'cloud_execution_policy'"
)

add_test(
  NAME spio_cli_version
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" --version | grep -q '^spio '"
)

add_test(
  NAME spio_cli_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" --help | grep -q '^spio usage:'"
)

add_test(
  NAME spio_cli_removed_commands_are_unknown
  COMMAND bash -c "for command in install use set tool cloud fetch lock project-graph; do out_file=\$(mktemp); err_file=\$(mktemp); if \"$<TARGET_FILE:spio>\" \"\$command\" >\"\$out_file\" 2>\"\$err_file\"; then exit 1; fi; grep -q \"unknown command: \$command\" \"\$err_file\" || exit 1; rm -f \"\$out_file\" \"\$err_file\"; done"
)

add_test(
  NAME spio_cli_machine_info_rejects_unknown_flag
  COMMAND bash -c "out_file=\$(mktemp) && err_file=\$(mktemp) && trap 'rm -f \"\$out_file\" \"\$err_file\"' EXIT && ! \"$<TARGET_FILE:spio>\" machine-info --bogus >\"\$out_file\" 2>\"\$err_file\" && grep -q 'machine-info accepts only --json' \"\$err_file\""
)

add_test(
  NAME spio_cli_doctor_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" doctor --help | grep -q '^usage: spio doctor \\[--json\\] \\[--manifest-path <path>\\] \\[--styio-bin <path>\\]$'"
)

add_test(
  NAME spio_cli_metadata_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" metadata --help | grep -q '^usage: spio metadata --json \\[--manifest-path <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME spio_cli_tree_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" tree --help | grep -q '^usage: spio tree \\[--manifest-path <path>\\]$'"
)

add_test(
  NAME spio_cli_add_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" add --help | grep -q '^usage: spio add <package-name> (--path <path> | --git <source> --rev <rev> | --registry <url> --version <x.y.z>) \\[--alias <name>\\] \\[--dev\\] \\[--manifest-path <path>\\]$'"
)

add_test(
  NAME spio_cli_remove_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" remove --help | grep -q '^usage: spio remove <alias-or-package> \\[--dev\\] \\[--manifest-path <path>\\]$'"
)

add_test(
  NAME spio_cli_sync_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" sync --help | grep -q '^usage: spio sync \\[--manifest-path <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME spio_cli_build_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" build --help | grep -q '^usage: spio build \\[--manifest-path <path>\\] \\[--package <package-name>\\] \\[--bin <name>\\|--lib\\] \\[--profile <dev\\|release>\\] \\[--dry-run\\] \\[--styio-bin <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME spio_cli_build_rejects_positional_mode
  COMMAND bash -c "out_file=\$(mktemp) && err_file=\$(mktemp) && trap 'rm -f \"\$out_file\" \"\$err_file\"' EXIT && ! \"$<TARGET_FILE:spio>\" build minimal >\"\$out_file\" 2>\"\$err_file\" && grep -q 'unexpected argument for build: minimal' \"\$err_file\""
)

add_test(
  NAME spio_cli_run_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" run --help | grep -q '^usage: spio run \\[--manifest-path <path>\\] \\[--package <package-name>\\] \\[--bin <name>\\] \\[--profile <dev\\|release>\\] \\[--dry-run\\] \\[--styio-bin <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME spio_cli_test_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" test --help | grep -q '^usage: spio test \\[--manifest-path <path>\\] \\[--package <package-name>\\] \\[--test <name>\\] \\[--profile <dev\\|release>\\] \\[--dry-run\\] \\[--styio-bin <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME spio_cli_vendor_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" vendor --help | grep -q '^usage: spio vendor \\[--manifest-path <path>\\] \\[--output <path>\\] \\[--locked\\|--offline\\|--frozen\\]$'"
)

add_test(
  NAME spio_cli_pack_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" pack --help | grep -q '^usage: spio pack \\[--manifest-path <path>\\] \\[--package <package-name>\\] \\[--output <path>\\]$'"
)

add_test(
  NAME spio_cli_publish_help
  COMMAND bash -c "\"$<TARGET_FILE:spio>\" publish --help | grep -Fqx 'usage: spio publish [--manifest-path <path>] [--package <package-name>] [--output <path>] [--registry <http(s)-url>] [--registry-profile <name>] [--registry-policy-file <path>] [--registry-header <name:value>] [--dry-run]'"
)
