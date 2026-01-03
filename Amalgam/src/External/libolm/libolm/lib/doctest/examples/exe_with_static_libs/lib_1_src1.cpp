#include <doctest/doctest.h>
DOCTEST_MAKE_STD_HEADERS_CLEAN_FROM_WARNINGS_ON_WALL_BEGIN
#include <cstdio>
DOCTEST_MAKE_STD_HEADERS_CLEAN_FROM_WARNINGS_ON_WALL_END

// OPTIMIZED: Simple test case with minimal overhead
TEST_CASE("asd") {
    // SAFETY: Using printf with const string literal - no format string vulnerabilities
    printf("hello from <lib_1_src1.cpp>\n");
}
