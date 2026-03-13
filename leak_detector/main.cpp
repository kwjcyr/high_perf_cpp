#include "leak_detector.h"
#include <vector>
#include <memory>
#include <format>

// 自动注册，在程序结束时报告
struct AutoReport {
    ~AutoReport() {
        LeakDetector::instance().report_leaks();
    }
};

static AutoReport auto_report;

void test_leak_1() {
    // 故意制造内存泄漏
    int* leak1 = new int(42);
    // 忘记 delete
}

void test_leak_2() {
    // 数组泄漏
    double* leak2 = new double[100];
    // 忘记 delete[]
}

void test_no_leak() {
    // 正确使用
    int* no_leak = new int(100);
    delete no_leak;
}

int main() {
    std::cout << std::format("{:=^60}\n", " C++20 Leak Detector Demo ");

    LeakDetector::instance().enable();

    std::cout << "\n=== Running Tests ===\n";

    test_no_leak();
    std::cout << "Test 1 (no leak): PASSED\n";

    test_leak_1();
    std::cout << "Test 2 (with leak): Created leak\n";

    test_leak_2();
    std::cout << "Test 3 (array leak): Created leak\n";

    std::cout << "\n=== Program Ending ===\n";
    std::cout << "Check leak report below...\n";

    return 0;
}

