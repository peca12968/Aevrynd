#pragma once
#include "Math.h"
#include "ExactTriangleContact3D.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <limits>
#include <span>

namespace Aevrynd {
// 僅平移的直立盒與靜態凸包；旋轉、穿透排除及滑動回應由上層處理。
enum class ConvexAabbSweepStatus3D { Separated, Touching, Hit, StartedOverlapping };
class ConvexAabbSweep3D {
public:
    static constexpr std::size_t MaximumPoints=32;
    static constexpr std::size_t MaximumAxes=3+MaximumPoints*(MaximumPoints-1)*(MaximumPoints-2)/6+3*MaximumPoints*(MaximumPoints-1)/2;
    struct Result {
        ConvexAabbSweepStatus3D status{ConvexAabbSweepStatus3D::Separated};
        double first{1},last{1};
        // 負／正側外法線；同時命中的軸均保留，不把稜角接觸壓成單一法線。
        std::bitset<MaximumAxes> negativeContacts{},positiveContacts{};
    };

    // 點集合的凸包，不是三角網格；凹形必須由呼叫端拆成多個凸包。
    // 僅接受 4..32 個不同、有限且非共面的點。失敗保留先前已準備的資料。
    // 保留所有三點法線、所有點對與盒軸叉積：包含真正凸包的面／邊軸，
    // 多餘軸不改 SAT 結果。小型碰撞體用有界容量換取不漏掉邊角軸。
    bool Prepare(std::span<const Vec3> points) noexcept try {
        if(points.size()<4||points.size()>MaximumPoints)return false;
        std::array<D3,MaximumPoints> local{};
        for(std::size_t i=0;i<points.size();++i){
            if(!Finite(points[i]))return false;
            for(std::size_t j=0;j<i;++j)
                if(points[i].x==points[j].x&&points[i].y==points[j].y&&points[i].z==points[j].z)return false;
            local[i]=Sub(ToDouble(points[i]),ToDouble(points[0]));
        }
        // 共面判斷重用引擎精確有理數；double 行列式會把大座標的消去誤差
        // 當成體積。只有準備階段使用配置；Sweep 仍完全無配置。
        namespace Exact=ExactTriangleContactDetail3D;
        std::array<Exact::Point,MaximumPoints> exact{};
        const auto anchor=Exact::ToExact(points[0]);
        for(std::size_t i=0;i<points.size();++i)exact[i]=Exact::Subtract(Exact::ToExact(points[i]),anchor);
        bool volume=false;
        for(std::size_t i=1;i<points.size();++i)
            for(std::size_t j=i+1;j<points.size();++j)
                for(std::size_t k=j+1;k<points.size();++k)
                    volume=volume||Exact::Dot3(exact[i],Exact::Cross3(exact[j],exact[k])).Sign()!=0;
        if(!volume)return false;
        ConvexAabbSweep3D next;
        next.origin_=ToDouble(points[0]);
        constexpr std::array<D3,3> boxAxes{{{1,0,0},{0,1,0},{0,0,1}}};
        for(const auto axis:boxAxes)next.AddAxis(axis,local,points.size());
        for(std::size_t i=0;i<points.size();++i){
            for(std::size_t j=i+1;j<points.size();++j){
                const auto edge=Sub(local[j],local[i]);
                for(const auto axis:boxAxes)next.AddAxis(CrossD(edge,axis),local,points.size());
                for(std::size_t k=j+1;k<points.size();++k)
                    next.AddAxis(CrossD(edge,Sub(local[k],local[i])),local,points.size());
            }
        }
        *this=next;
        return true;
    }catch(...){return false;}

    // false 代表無效輸入，output 不變；不是「沒有碰撞」。
    // Touching 代表只有邊界接觸，沒有沿此位移進入內部；Hit 可在 t=0/1。
    // double 浮點 SAT，不宣稱精確算術；不將小速度／小非零軸視為零。
    bool Sweep(Vec3 center,Vec3 extents,Vec3 displacement,Result& output) const noexcept {
        if(!axisCount_||!Finite(center)||!Finite(extents)||!Finite(displacement)||
            extents.x<=0||extents.y<=0||extents.z<=0)return false;
        const auto c=Sub(ToDouble(center),origin_),d=ToDouble(displacement),e=ToDouble(extents);
        // 前三軸固定為 XYZ；只用包絡排除不可能碰到的構件，不以盒頂代替坡面。
        const double position[3]{c.x,c.y,c.z},motion[3]{d.x,d.y,d.z},extent[3]{e.x,e.y,e.z};
        for(std::size_t i=0;i<3;++i){
            const double end=position[i]+motion[i];
            if(std::max(position[i],end)<axes_[i].low-extent[i]||
               std::min(position[i],end)>axes_[i].high+extent[i]){output=Result{};return true;}
        }
        double enter=-std::numeric_limits<double>::infinity(),leave=std::numeric_limits<double>::infinity();
        bool strictInside=true,stationaryBoundary=false,impossible=false;
        Result result;
        for(std::size_t i=0;i<axisCount_;++i){
            const auto& axis=axes_[i];const auto n=axis.n;
            const double radius=std::abs(n.x)*e.x+std::abs(n.y)*e.y+std::abs(n.z)*e.z;
            const double low=axis.low-radius,high=axis.high+radius,p=DotD(n,c),v=DotD(n,d);
            strictInside=strictInside&&(p>low&&p<high);
            if(v==0){
                if(p<low||p>high)impossible=true;
                if(p==low||p==high)stationaryBoundary=true;
            }else{
                double a=(low-p)/v,b=(high-p)/v;if(a>b)std::swap(a,b);
                enter=std::max(enter,a);leave=std::min(leave,b);
            }
        }
        if(strictInside){result.status=ConvexAabbSweepStatus3D::StartedOverlapping;result.first=0;result.last=std::clamp(leave,0.,1.);}
        else if(!impossible&&enter<=leave&&leave>=0&&enter<=1){
            result.first=std::max(0.,enter);result.last=std::min(1.,leave);
            result.status=(!stationaryBoundary&&enter<leave&&leave>0)?ConvexAabbSweepStatus3D::Hit:ConvexAabbSweepStatus3D::Touching;
            for(std::size_t i=0;i<axisCount_;++i){
                const auto& a=axes_[i];const auto n=a.n;
                const double r=std::abs(n.x)*e.x+std::abs(n.y)*e.y+std::abs(n.z)*e.z;
                const double p=DotD(n,c),v=DotD(n,d),lo=a.low-r,hi=a.high+r;
                // 以時間比值核對限制軸，避免接觸點再投影累積另一輪捨入。
                if(v!=0){
                    if(SameTime((lo-p)/v,result.first))result.negativeContacts.set(i);
                    if(SameTime((hi-p)/v,result.first))result.positiveContacts.set(i);
                }else{
                    if(p==lo)result.negativeContacts.set(i);
                    if(p==hi)result.positiveContacts.set(i);
                }
            }
        }
        output=result;return true;
    }

    std::size_t AxisCount()const noexcept{return axisCount_;}
    // result 位元指定正負側，索引只對產生該 result 的同一個 prepared hull 有效。
    bool Normal(std::size_t axis,bool positive,Vec3& output)const noexcept{
        if(axis>=axisCount_)return false;
        const auto n=axes_[axis].n;const double sign=positive?1.:-1.;
        output={float(sign*n.x),float(sign*n.y),float(sign*n.z)};return true;
    }
private:
    struct D3 {double x{},y{},z{};};
    struct Axis {D3 n;double low{},high{};};
    static D3 ToDouble(Vec3 p)noexcept{return{p.x,p.y,p.z};}
    static D3 Sub(D3 a,D3 b)noexcept{return{a.x-b.x,a.y-b.y,a.z-b.z};}
    static D3 CrossD(D3 a,D3 b)noexcept{return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
    static double DotD(D3 a,D3 b)noexcept{return a.x*b.x+a.y*b.y+a.z*b.z;}
    static bool Finite(Vec3 p)noexcept{return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
    static bool SameTime(double a,double b)noexcept{return a==b||std::nextafter(a,b)==b;}
    void AddAxis(D3 n,const std::array<D3,MaximumPoints>& points,std::size_t count)noexcept{
        const double scale=std::max({std::abs(n.x),std::abs(n.y),std::abs(n.z)});
        if(scale==0)return;
        n={n.x/scale,n.y/scale,n.z/scale};const double length=std::sqrt(DotD(n,n));
        n={n.x/length,n.y/length,n.z/length};
        // 相同軸不做近似去重，以免刪掉細長形體所需的分離方向。
        auto& axis=axes_[axisCount_++];axis.n=n;axis.low=axis.high=DotD(n,points[0]);
        for(std::size_t i=1;i<count;++i){const double p=DotD(n,points[i]);axis.low=std::min(axis.low,p);axis.high=std::max(axis.high,p);}
    }
    D3 origin_{};
    std::array<Axis,MaximumAxes> axes_{};
    std::size_t axisCount_{};
};
// 數學參考：David Eberly, Method of Separating Axes, §4/6 (CC BY 4.0)。
// https://www.geometrictools.com/Documentation/MethodOfSeparatingAxes.pdf
// 以上為獨立 C++ 實作，未使用 Unreal Engine 程式碼。
}
