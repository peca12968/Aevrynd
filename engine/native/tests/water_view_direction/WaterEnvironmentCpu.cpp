#include <string_view>
#include <iostream>
#include "WaterEnvironment.inc"
int main(){const auto empty=ParseWaterAerialDiagnosticEnvironment(L"");if(!empty.valid||empty.mutation)return 1;
 for(wchar_t c=L'1';c<=L'9';++c){const auto s=ParseWaterAerialDiagnosticEnvironment({&c,1});if(!s.valid||s.mutation!=unsigned(c-L'0'))return 2;}
 for(wchar_t c=L'A';c<=L'D';++c){const auto s=ParseWaterAerialDiagnosticEnvironment({&c,1});if(!s.valid||s.mutation!=unsigned(c-L'A')+10u)return 5;}
 for(auto text:{L"0",L"10",L"-1",L"01",L" 1",L"1 ",L"true",L"123456789",L"a",L"E",L"AA"})if(ParseWaterAerialDiagnosticEnvironment(text).valid)return 3;
 for(auto text:{L"",L"1",L"5",L"6",L"hostile"}){const auto s=ParseWaterAerialDiagnosticEnvironment(text,false);if(!s.valid||s.mutation)return 4;}
 std::cout<<"PASS default production=0, legal1..9/A..D, invalid reject, disabled ignores; GPU=false\n";}
