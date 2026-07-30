add_library(spio_build_config INTERFACE)
target_include_directories(spio_build_config INTERFACE "${CMAKE_SOURCE_DIR}/src")
if(EXISTS "${CMAKE_SOURCE_DIR}/src-private")
  target_include_directories(spio_build_config INTERFACE "${CMAKE_SOURCE_DIR}/src-private")
endif()
set(SPIO_VERSION_SUFFIX "-dev" CACHE STRING "Suffix appended to the spio project version, for example -dev or -alpha.")
target_compile_definitions(spio_build_config INTERFACE
  "SPIO_PROJECT_ROOT=\"${CMAKE_SOURCE_DIR}\""
  "SPIO_PROJECT_VERSION=\"${PROJECT_VERSION}${SPIO_VERSION_SUFFIX}\""
)
target_link_libraries(spio_build_config INTERFACE tomlplusplus::tomlplusplus nlohmann_json::nlohmann_json)

function(spio_add_internal_static_library target)
  add_library(${target} STATIC ${ARGN})
  target_link_libraries(${target} PUBLIC spio_build_config)
endfunction()

set(SPIO_FOUNDATION_SOURCES
    SpioCompat/Compat.cpp
    SpioCore/AtomicFile.cpp
    SpioCore/FileLock.cpp
    SpioCore/Paths.cpp
    SpioCore/Process.cpp
    SpioCore/Sha256.cpp
    SpioCore/ToolPaths.cpp
)

set(SPIO_MANIFEST_SOURCES
    SpioManifest/Lockfile.cpp
    SpioManifest/Manifest.cpp
)

set(SPIO_RESOLUTION_SOURCES
    SpioRegistryClient/Client.cpp
    SpioResolve/MetadataContract.cpp
    SpioResolve/ResolutionContract.cpp
    SpioResolve/Resolver.cpp
    SpioSecurity/Ed25519.cpp
    SpioSecurity/PrescanArchive.cpp
    SpioSecurity/RegistrySecurity.cpp
    SpioSecurity/RegistryTrust.cpp
    SpioSecurity/TufVerifier.cpp
)

set(SPIO_PLAN_SOURCES
    SpioPlan/CompilePlan.cpp
)

set(SPIO_PACKAGE_SERVICE_SOURCES
    SpioPack/Pack.cpp
    SpioPublish/Publish.cpp
    SpioTree/Render.cpp
    SpioVendor/Vendor.cpp
    SpioWorkflow/Dependencies.cpp
)

set(SPIO_CLI_SUPPORT_SOURCES
    SpioCLI/MachineInfoContract.cpp
    SpioCLI/Support.cpp
)

set(SPIO_CLI_COMMAND_SOURCES
    SpioApp/DoctorApp.cpp
    SpioApp/PackageApp.cpp
    SpioApp/WorkflowApp.cpp
)

set(SPIO_CLI_SHELL_SOURCES
    SpioCLI/CLI.cpp
)

file(GLOB_RECURSE SPIO_PRIVATE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src-private/*.cpp")

spio_add_internal_static_library(spio_foundation ${SPIO_FOUNDATION_SOURCES})

spio_add_internal_static_library(spio_manifest ${SPIO_MANIFEST_SOURCES})
target_link_libraries(spio_manifest PUBLIC spio_foundation)

spio_add_internal_static_library(spio_resolution ${SPIO_RESOLUTION_SOURCES})
target_link_libraries(spio_resolution PUBLIC spio_foundation spio_manifest)

spio_add_internal_static_library(spio_plan ${SPIO_PLAN_SOURCES})
target_link_libraries(spio_plan PUBLIC spio_foundation spio_manifest spio_resolution)

spio_add_internal_static_library(spio_package_service ${SPIO_PACKAGE_SERVICE_SOURCES})
target_link_libraries(spio_package_service PUBLIC spio_foundation spio_manifest spio_resolution)

add_library(spio_backend_services INTERFACE)
target_link_libraries(spio_backend_services INTERFACE
  spio_foundation
  spio_manifest
  spio_resolution
  spio_plan
  spio_package_service
)

spio_add_internal_static_library(spio_cli_support ${SPIO_CLI_SUPPORT_SOURCES})
target_link_libraries(spio_cli_support PUBLIC spio_foundation spio_manifest spio_resolution spio_plan)

spio_add_internal_static_library(spio_cli_commands ${SPIO_CLI_COMMAND_SOURCES})
target_link_libraries(spio_cli_commands PUBLIC spio_backend_services spio_cli_support)

spio_add_internal_static_library(spio_cli_shell ${SPIO_CLI_SHELL_SOURCES})
target_link_libraries(spio_cli_shell PUBLIC spio_cli_commands spio_cli_support)

if(SPIO_PRIVATE_SOURCES)
  spio_add_internal_static_library(spio_private_extensions ${SPIO_PRIVATE_SOURCES})
  target_link_libraries(spio_private_extensions PUBLIC spio_backend_services spio_cli_support)
  target_link_libraries(spio_cli_shell PUBLIC spio_private_extensions)
endif()

add_executable(spio main.cpp)
target_link_libraries(spio PRIVATE spio_cli_shell)
