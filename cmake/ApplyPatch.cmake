# Applies one patch to a fetched dependency, once: run as a FetchContent PATCH_COMMAND with
# -DPATCH=<file>, in the dependency's source folder. A patch already in place is left alone, so
# a second configure of the same folder does not fail.
execute_process(COMMAND git apply --reverse --check "${PATCH}" RESULT_VARIABLE already OUTPUT_QUIET ERROR_QUIET)
if (already EQUAL 0)
    message(STATUS "Patch already applied: ${PATCH}")
    return()
endif()
execute_process(COMMAND git apply "${PATCH}" RESULT_VARIABLE failed)
if (NOT failed EQUAL 0)
    message(FATAL_ERROR "Could not apply ${PATCH}")
endif()
message(STATUS "Applied ${PATCH}")
