// Single translation unit that provides doctest's main().
// All other test files just #include "vendor/doctest.h" without this macro.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "vendor/doctest.h"
