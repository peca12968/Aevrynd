#include "RiftwardenRules.h"
#include <iostream>

int main() {
    if (RiftwardenRules::ComboReward(17, 1, 1.f) != 17) {
        std::cerr << "The first kill must retain its base reward\n";
        return 1;
    }
    if (RiftwardenRules::ComboReward(17, 5, 1.f) != 20) {
        std::cerr << "A five-hit combo must grant a 20 percent bonus\n";
        return 2;
    }
    if (RiftwardenRules::ComboReward(17, 20, 1.25f) != 32) {
        std::cerr << "Combo and event gold modifiers must stack predictably\n";
        return 3;
    }
    return 0;
}
