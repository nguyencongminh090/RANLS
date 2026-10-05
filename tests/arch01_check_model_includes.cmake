# ARCH-01 guard: src/model must not #include anything from engine/ (layer rule
# ui -> command/engine -> model). Matches #include lines only, so string
# literals ("engine/pbrain-rapfi") and comments are ignored.
# Usage: cmake -DMODEL_DIR=<src/model> -P arch01_check_model_includes.cmake
if(NOT MODEL_DIR)
    message(FATAL_ERROR "MODEL_DIR not set")
endif()
file(GLOB_RECURSE _files "${MODEL_DIR}/*.h" "${MODEL_DIR}/*.hpp" "${MODEL_DIR}/*.cpp")
if(NOT _files)
    message(FATAL_ERROR "no files found under ${MODEL_DIR}")
endif()
set(_bad "")
foreach(_f IN LISTS _files)
    file(STRINGS "${_f}" _hits REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"][^>\"]*engine/")
    foreach(_h IN LISTS _hits)
        string(APPEND _bad "${_f}: ${_h}\n")
    endforeach()
endforeach()
if(_bad)
    message(FATAL_ERROR "ARCH-01 violation: model/ includes engine/:\n${_bad}")
endif()
message(STATUS "ARCH-01: src/model has no engine/ includes")
