find_program(EXACTNESS exactness)

set(ENV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/env")

# We expect the test-data directories to have been previously retrieved
set(TEST_DATA_DIR "${CMAKE_SOURCE_DIR}/.deps/test-data")
if (NOT EXISTS "${TEST_DATA_DIR}/")
   message(FATAL_ERROR "${TEST_DATA_DIR} does not exist. Run ${CMAKE_SOURCE_DIR}/scripts/get-test-data.sh.")
endif ()

# Base eovim command to run for tests
set(EOVIM_BASE_CMD
   "${CMAKE_INSTALL_PREFIX}/bin/eovim"
   --config "${ENV_DIR}/default.cfg"
   -u "${ENV_DIR}/init.vim"
)
string(REPLACE ";" " " EOVIM_BASE_CMD_STR "${EOVIM_BASE_CMD}")

# Generate the pixel perfect tests file
configure_file(
   "${CMAKE_CURRENT_SOURCE_DIR}/tests.txt.in"
   "${CMAKE_CURRENT_BINARY_DIR}/tests.txt"
   @ONLY
)

# We have one test: run the pixel perfect regression test suite
add_test(
   NAME pixel_perfect
   COMMAND
      "${EXACTNESS}"
      --base-dir "${TEST_DATA_DIR}/recordings"
      --dest-dir "${TEST_DATA_DIR}/orig"
      --play "${CMAKE_CURRENT_BINARY_DIR}/tests.txt"
)
