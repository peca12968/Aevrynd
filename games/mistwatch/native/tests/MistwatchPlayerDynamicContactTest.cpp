#include <cstring>
#include "support/MistwatchPlayerDynamicContactCommon.h"
#include "support/MistwatchPlayerHeadMatrix.inc"
#include "support/MistwatchPlayerEmptyCompatibility.inc"

// 每個模式由正式測試包裝器以全新子程序呼叫，維持原 case 的獨立初始狀態。
int main(int argc,char** argv){
    if(argc==2&&std::strcmp(argv[1],"--matrix")==0)return RunPlayerHeadMatrix();
    if(argc==2&&std::strcmp(argv[1],"--empty")==0)return RunPlayerEmptyCompatibility();
    std::cerr<<"PLAYER_DYNAMIC_CONTACT_TEST_EXPECTS_MATRIX_OR_EMPTY_MODE\n";
    return 2;
}
