#pragma once
#include "MistwatchWestCliffRelief.h"
#include <vector>

namespace Mistwatch::WestCliffClearance {
// World.hlsl ApplyWind：世界變換之後施加位移，不再乘物件縮放。
// |dx| <= (1+.35)*.13=.1755、0 <= dy <= .08*.13=.0104、|dz| <= .55*.13=.0715。
// 各軸向外擴大；Y 採對稱界限，因此比實際只向上的位移更保守。
// 此界限只涵蓋目前固定風擺，不能代替蒙皮、其他 shader 位移或動態操作驗收。
inline constexpr Aevrynd::Vec3 FoliageSweep{.176f,.011f,.072f};
struct Proof {
    bool disjoint{},separated{};
    double minimumY{},ceiling{},padding{};
    std::size_t cells{};
};
// 對完整格子取高度上界；不將有限點採樣誤稱為三角面交集證明。
class Envelope {
    struct Cell {double x0,x1,z0,z1,ceiling,scale;};
    std::vector<Cell> cells_;
    static bool Finite(Aevrynd::Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
    static bool Overlaps(double x0,double x1,double z0,double z1,double a,double b,double c,double d){
        return x0<=b&&x1>=a&&z0<=d&&z1>=c;
    }
public:
    Envelope(const Aevrynd::HeightField3D& canonical,const Aevrynd::HeightField3D& derived){
        const auto& s=canonical.Settings();const auto& t=derived.Settings();
        if(!Finite(s.origin)||!Finite(t.origin)||!std::isfinite(s.cellSize)||s.cellSize<=0||
           s.width<2||s.depth<2||s.width>1000000||s.depth>1000000||
           std::uint64_t(s.width)*s.depth>4194304||
           canonical.SampleCount()!=std::size_t(s.width)*s.depth||
           !WestCliffRelief::SameLayout(canonical,derived))
            throw std::invalid_argument("WEST_CLIFF_CLEARANCE_LAYOUT");
        std::vector<double> xs(s.width),zs(s.depth);
        for(unsigned x=0;x<s.width;++x){xs[x]=canonical.SamplePosition(x,0).x;
            if(!std::isfinite(xs[x])||(x&&xs[x]<=xs[x-1]))throw std::invalid_argument("WEST_CLIFF_CLEARANCE_X_GRID");}
        for(unsigned z=0;z<s.depth;++z){zs[z]=canonical.SamplePosition(0,z).z;
            if(!std::isfinite(zs[z])||(z&&zs[z]<=zs[z-1]))throw std::invalid_argument("WEST_CLIFF_CLEARANCE_Z_GRID");}
        if(xs.front()>WestCliffRelief::GuardMinX||xs.back()<WestCliffRelief::GuardMaxX||
           zs.front()>WestCliffRelief::GuardMinZ||zs.back()<WestCliffRelief::GuardMaxZ)
            throw std::invalid_argument("WEST_CLIFF_CLEARANCE_GUARD_COVERAGE");
        for(unsigned z=0;z+1<s.depth;++z)for(unsigned x=0;x+1<s.width;++x){
            if(!Overlaps(xs[x],xs[x+1],zs[z],zs[z+1],WestCliffRelief::GuardMinX,WestCliffRelief::GuardMaxX,
                         WestCliffRelief::GuardMinZ,WestCliffRelief::GuardMaxZ))continue;
            if(cells_.size()>=4096)throw std::invalid_argument("WEST_CLIFF_CLEARANCE_CELL_BUDGET");
            Cell cell{xs[x],xs[x+1],zs[z],zs[z+1],-(std::numeric_limits<double>::infinity)(),1.0};
            for(unsigned dz=0;dz<2;++dz)for(unsigned dx=0;dx<2;++dx){
                for(const double h:{double(canonical.Height(x+dx,z+dz)),double(derived.Height(x+dx,z+dz))}){
                    if(!std::isfinite(h))throw std::invalid_argument("WEST_CLIFF_CLEARANCE_NONFINITE_HEIGHT");
                    cell.ceiling=(std::max)(cell.ceiling,h);cell.scale=(std::max)(cell.scale,std::abs(h));
                }
            }
            cells_.push_back(cell);
        }
        if(cells_.empty())throw std::invalid_argument("WEST_CLIFF_CLEARANCE_EMPTY_COVERAGE");
    }
    Proof Query(Aevrynd::Vec3 a,Aevrynd::Vec3 b,Aevrynd::Vec3 c,Aevrynd::Vec3 sweptRadius={})const{
        if(!Finite(a)||!Finite(b)||!Finite(c))throw std::invalid_argument("WEST_CLIFF_CLEARANCE_NONFINITE_TRIANGLE");
        if(!Finite(sweptRadius)||sweptRadius.x<0||sweptRadius.y<0||sweptRadius.z<0)
            throw std::invalid_argument("WEST_CLIFF_CLEARANCE_INVALID_SWEEP");
        // 先展開完整運動包絡，再裁到地形修改區，避免漏掉從 guard 外擺入的枝葉。
        const double x0=(std::max)(double(WestCliffRelief::GuardMinX),double((std::min)({a.x,b.x,c.x}))-sweptRadius.x);
        const double x1=(std::min)(double(WestCliffRelief::GuardMaxX),double((std::max)({a.x,b.x,c.x}))+sweptRadius.x);
        const double z0=(std::max)(double(WestCliffRelief::GuardMinZ),double((std::min)({a.z,b.z,c.z}))-sweptRadius.z);
        const double z1=(std::min)(double(WestCliffRelief::GuardMaxZ),double((std::max)({a.z,b.z,c.z}))+sweptRadius.z);
        Proof proof{};proof.minimumY=(std::min)({double(a.y),double(b.y),double(c.y)})-sweptRadius.y;
        if(x0>x1||z0>z1){proof.disjoint=true;return proof;}
        proof.ceiling=-(std::numeric_limits<double>::infinity)();double scale=1.0;
        for(const auto& cell:cells_){
            if(!Overlaps(x0,x1,z0,z1,cell.x0,cell.x1,cell.z0,cell.z1))continue;
            ++proof.cells;proof.ceiling=(std::max)(proof.ceiling,cell.ceiling);scale=(std::max)(scale,cell.scale);
        }
        if(!proof.cells||!std::isfinite(proof.ceiling))throw std::invalid_argument("WEST_CLIFF_CLEARANCE_UNCOVERED_TRIANGLE");
        // 每格 a/c/b、b/c/d 面為凸組合，四角最大值界定完整幾何上界。
        // 再保留 64 * float epsilon * 高度尺度的數值餘量，涵蓋短式線性插值的
        // 減法／乘法／加法捨入；不是依植物類別或任意離地高度放行。
        // 界限針對輸入的實際 float 世界頂點；不宣稱未知 shader 位移或任意姿態誤差。
        scale=(std::max)(scale,(std::max)({std::abs(double(a.y)),std::abs(double(b.y)),std::abs(double(c.y))}));
        scale=(std::max)(scale,std::abs(proof.minimumY));
        proof.padding=64.0*double(std::numeric_limits<float>::epsilon())*scale;
        proof.separated=proof.minimumY>proof.ceiling+proof.padding;
        return proof;
    }
};
}
