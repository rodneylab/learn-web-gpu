#include <catch2/catch_test_macros.hpp>

uint32_t factorial(uint32_t number)
{
    return (number <= 1) ? number : factorial(number - 1) * number;
}

TEST_CASE("It computer factoriasl", "[factorial]")
{
    REQUIRE(factorial(1) == 1);
}
