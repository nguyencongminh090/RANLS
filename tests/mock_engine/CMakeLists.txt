# PORT-03: portable stand-in "engine" executables for the test suite.
#
# Two tiny, dependency-free targets that replace the /bin/cat and /bin/true
# POSIX stand-ins the test suites used to hardcode:
#   mock_engine       — cat-like: echoes stdin, stays alive until EOF/"END".
#   mock_engine_quit  — true-like: exits 0 immediately.
#
# Their on-disk paths are handed to the test targets as compile definitions
# (MOCK_ENGINE_PATH / MOCK_ENGINE_QUIT_PATH) — see tests/CMakeLists.txt.

add_executable(mock_engine mock_engine.cpp)
target_compile_features(mock_engine PRIVATE cxx_std_20)

add_executable(mock_engine_quit mock_engine.cpp)
target_compile_features(mock_engine_quit PRIVATE cxx_std_20)
target_compile_definitions(mock_engine_quit PRIVATE MOCK_ENGINE_QUIT)
