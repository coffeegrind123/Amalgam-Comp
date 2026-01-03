// OPTIMIZED: Header includes with proper warning suppression for clean builds
#include <doctest/doctest.h>
DOCTEST_MAKE_STD_HEADERS_CLEAN_FROM_WARNINGS_ON_WALL_BEGIN
#include <cstdio>
DOCTEST_MAKE_STD_HEADERS_CLEAN_FROM_WARNINGS_ON_WALL_END

// SAFETY: Simple test case with no crash-prone code - printf is safe with string literal
// OPTIMIZED: Test name kept concise for fast test discovery
TEST_CASE("asd") {
    printf("hello from <lib_2_src.cpp>\n");
}
