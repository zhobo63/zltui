#include "unit_test.h"

#define TEST_EXAMPLE 1

#if TEST_EXAMPLE
void test_example(UnitReport& parent)
{
    UnitReport unit("example");

    int a=1;
    int b=1;
    UNIT_TEST("a == b", a==b);

    parent.report.push_back(unit);
}
#endif

void main() {
#ifdef _WIN32
    // Set C runtime locale so std::cout handles multibyte (UTF-8) characters correctly.
    // setlocale(LC_ALL, "zh_TW.UTF-8");
    // Set console input/output code pages to UTF-8 so emoji and all Unicode display correctly.
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
#endif


    UnitReport main("UnitTest");

    try {
#if TEST_EXAMPLE        
        test_example(main);
#endif        
    }
    catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
    }
    unit_test_valid(main);
    std::cout << "Report:" << std::endl;
    print_uint_test(main);

    if (main.result) {
        log_unit_test("all", true);
    }
    else {
        std::cout << "Failed:" << std::endl;
        print_uint_test_fail(main);
    }
}
