// SAFETY: Using doctest framework for unit testing
#include <doctest/doctest.h>
DOCTEST_MAKE_STD_HEADERS_CLEAN_FROM_WARNINGS_ON_WALL_BEGIN
#include <cstdio>
DOCTEST_MAKE_STD_HEADERS_CLEAN_FROM_WARNINGS_ON_WALL_END

// SAFE: Simple test case with no memory operations or pointer dereferences
// OPTIMIZED: Direct printf call is already optimal for this use case
TEST_CASE("asd") {
    printf("hello from <lib_1_src2.cpp>\n");
}
