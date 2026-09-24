# Applies a patch to a fetched dependency, and does nothing if it is already
# applied. FetchContent re-runs PATCH_COMMAND on reconfigure, so a plain
# `git apply` would fail the second time and break the build.
#
# Usage: cmake -DPATCH_FILE=<abs path> -DWORK_DIR=<abs path> -P ApplyPatch.cmake

if(NOT EXISTS "${PATCH_FILE}")
    message(FATAL_ERROR "ApplyPatch: no such patch file: ${PATCH_FILE}")
endif()

find_package(Git QUIET REQUIRED)

# --reverse --check succeeds only when the patch is already in place.
execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${PATCH_FILE}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE already_applied
    OUTPUT_QUIET ERROR_QUIET)

if(already_applied EQUAL 0)
    message(STATUS "ApplyPatch: already applied, skipping")
    return()
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace "${PATCH_FILE}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE result
    ERROR_VARIABLE err)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "ApplyPatch: failed to apply ${PATCH_FILE}\n${err}")
endif()

message(STATUS "ApplyPatch: applied ${PATCH_FILE}")
