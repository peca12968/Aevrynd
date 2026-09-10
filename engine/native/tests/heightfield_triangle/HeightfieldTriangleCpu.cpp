#include <Aevrynd/HeightField3D.h>
#include <cmath>
#include <iostream>
using namespace Aevrynd;
int main(){HeightField3D t({{0,0,0},2,2,1});t.SetHeight(0,0,0);t.SetHeight(1,0,1);t.SetHeight(0,1,2);t.SetHeight(1,1,9);
 unsigned count=0;for(unsigned z=0;z<=100;++z)for(unsigned x=0;x<=100;++x){float u=x/100.f,v=z/100.f;const float expected=u+v<=1?u+2*v:9-7*(1-u)-8*(1-v);if(std::abs(t.SampleHeight(u,v)-expected)>1e-5)return 1;++count;}
 if(t.SampleHeight(-1,-1)!=0||t.SampleHeight(2,2)!=9||t.SampleHeight(1,0)!=1||t.SampleHeight(0,1)!=2)return 2;
 std::cout<<"PASS triangle queries="<<count<<" edges/clamping valid; saddle midpoint old=3 new=1.5\n";
}
