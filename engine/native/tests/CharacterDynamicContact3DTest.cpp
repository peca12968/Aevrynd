#include <iostream>

int RunDynamicContactOriginalCases();
int RunDynamicContactNumericCases();

// 兩個已執行來源各自保留匿名命名空間與全部斷言，不以簡化 callback 取代。
int main() {
    const int original=RunDynamicContactOriginalCases();
    if(original!=0)return original;
    const int numeric=RunDynamicContactNumericCases();
    if(numeric!=0)return numeric;
    std::cout<<"CHARACTER_DYNAMIC_CONTACT_FORMAL_CPU_PASS original_suite=1 numeric_suite=1 main=0 GPU=0\n";
    return 0;
}
