#pragma once
#include <Aevrynd/Math.h>
#include <Aevrynd/ExactTriangleContact3D.h>
#include <algorithm>
#include <array>
#include <limits>

namespace Aevrynd {
// 圓柱只描述接觸體積，不自動授權接地或改變任一角色。
struct DynamicCylinder3D {
    Vec3 center{};
    float radius{}, halfHeight{};
};
enum class BoxCylinderContactKind3D { Clear, SideEntry, VerticalEntry, SimultaneousEntry, StartedOverlapping };
struct BoxCylinderContact3D {
    BoxCylinderContactKind3D kind{BoxCylinderContactKind3D::Clear};
    double first{1};
    Vec3 horizontalNormal{}, verticalNormal{};
    double horizontalPenetration{};
};
namespace DynamicContactDetail3D {
inline bool Finite(Vec3 p) noexcept {return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
struct Interval {
    double first{-(std::numeric_limits<double>::infinity)()},last{(std::numeric_limits<double>::infinity)()};
    bool valid{true};
    bool resolved{true};
};
inline Interval ConstantSlab(double p,double delta,double extent) noexcept {
    if(delta==0)return std::abs(p)<extent?Interval{}:Interval{0,0,false};
    const double a=(-extent-p)/delta,b=(extent-p)/delta;
    const double first=(std::min)(a,b),last=(std::max)(a,b);
    return {first,last,first<last,first<last};
}
inline Interval Intersect(Interval a,Interval b) noexcept {
    const double low=(std::max)(a.first,b.first),high=(std::min)(a.last,b.last);
    return {low,high,a.valid&&b.valid&&low<high,a.resolved&&b.resolved};
}
using Exact=ExactTriangleContactDetail3D::Rational;
struct ExactInput {Vec3 center,extents,delta;DynamicCylinder3D body;};
inline Exact F(float value){return Exact::Float(value);}
inline Exact Abs(Exact value){return value.Sign()<0?-value:value;}
struct ExactHorizontal {int sign{},direction{};};
inline ExactHorizontal HorizontalAt(const ExactInput& i,const Exact& t) {
    const auto x=F(i.center.x)-F(i.body.center.x)+F(i.delta.x)*t;
    const auto z=F(i.center.z)-F(i.body.center.z)+F(i.delta.z)*t;
    auto qx=Abs(x)-F(i.extents.x),qz=Abs(z)-F(i.extents.z);
    if(qx.Sign()<0)qx={};if(qz.Sign()<0)qz={};
    const auto squared=qx*qx+qz*qz-F(i.body.radius)*F(i.body.radius);
    const auto derivative=qx*(x.Sign()<0?-F(i.delta.x):F(i.delta.x))+
        qz*(z.Sign()<0?-F(i.delta.z):F(i.delta.z));
    return {squared.Sign(),derivative.Sign()};
}
inline int DiskPredicate(const ExactInput& i,int sx,int sz,bool moving) {
    const auto x=F(i.center.x)-F(i.body.center.x)+F(i.extents.x)*F(float(sx));
    const auto z=F(i.center.z)-F(i.body.center.z)+F(i.extents.z)*F(float(sz));
    const auto r=F(i.body.radius);
    if(!moving)return (x*x+z*z-r*r).Sign();
    const auto dx=F(i.delta.x),dz=F(i.delta.z),cross=x*dz-z*dx;
    return (r*r*(dx*dx+dz*dz)-cross*cross).Sign();
}
inline Interval Disk(double x,double z,double dx,double dz,double radius,
    const ExactInput* input=nullptr,int sx=0,int sz=0) {
    const double speed2=dx*dx+dz*dz;
    constexpr double epsilon=std::numeric_limits<double>::epsilon();
    if(speed2==0){
        const double distance=std::hypot(x,z),error=64*epsilon*((std::max)(distance,radius));
        if(std::abs(distance-radius)<=error){
            if(!input)return {0,0,false,false};
            return DiskPredicate(*input,sx,sz,false)<0?Interval{}:Interval{0,0,false};
        }
        return distance<radius?Interval{}:Interval{0,0,false};
    }
    // 先求到直線的正交距離，避免用 b*b-a*c 扣掉兩個接近的大數。
    const double cross=x*dz-z*dx,closest=-(x*dx+z*dz)/speed2;
    const double radial=radius*radius-cross*cross/speed2;
    const double scaleX=input?std::abs(double(input->center.x))+std::abs(double(input->body.center.x))+input->extents.x:std::abs(x);
    const double scaleZ=input?std::abs(double(input->center.z))+std::abs(double(input->body.center.z))+input->extents.z:std::abs(z);
    const double crossError=8*epsilon*(scaleX*std::abs(dz)+scaleZ*std::abs(dx));
    const double error=64*epsilon*(radius*radius+cross*cross/speed2)+
        (2*std::abs(cross)*crossError+crossError*crossError)/speed2;
    // 誤差篩選不宣稱所有域內根皆精確；不確定正根拒絕，真 tangent 以有理數判斷。
    if(std::abs(radial)<=error){
        if(!input)return {0,0,false,false};
        if(DiskPredicate(*input,sx,sz,true)<=0)return {0,0,false};
        return {0,0,false,false};
    }
    if(!(radial>0))return {0,0,false};
    const double half=std::sqrt(radial/speed2);
    const double first=closest-half,last=closest+half;
    return {first,last,first<last,first<last};
}
inline Interval RoundedRectangle(double x,double z,double dx,double dz,double ex,double ez,double radius,
    const ExactInput* input=nullptr) {
    // 矩形 Minkowski 圓盤：兩條長方帶與四個角圓的聯集，無方盒角落的多餘碰撞區。
    const std::array<Interval,6> parts{{
        Intersect(ConstantSlab(x,dx,ex+radius),ConstantSlab(z,dz,ez)),
        Intersect(ConstantSlab(x,dx,ex),ConstantSlab(z,dz,ez+radius)),
        Disk(x-ex,z-ez,dx,dz,radius,input,-1,-1),Disk(x-ex,z+ez,dx,dz,radius,input,-1,1),
        Disk(x+ex,z-ez,dx,dz,radius,input,1,-1),Disk(x+ex,z+ez,dx,dz,radius,input,1,1)}};
    Interval result{0,0,false};
    for(const auto part:parts){
        if(!part.resolved)return {0,0,false,false};
        if(!part.valid)continue;
        if(!result.valid)result=part;
        else{result.first=(std::min)(result.first,part.first);result.last=(std::max)(result.last,part.last);}
    }
    return result;
}
inline double SignedRoundedDistance(double x,double z,double ex,double ez,double radius) noexcept {
    const double a=std::abs(x)-ex,b=std::abs(z)-ez;
    return std::hypot((std::max)(a,0.),(std::max)(b,0.))+(std::min)((std::max)(a,b),0.)-radius;
}
inline Vec3 HorizontalNormal(double x,double z,double dx,double dz,double ex,double ez) noexcept {
    const double a=x-std::clamp(x,-ex,ex),b=z-std::clamp(z,-ez,ez),length=std::hypot(a,b);
    if(length>0)return {float(a/length),0,float(b/length)};
    // 原先重疊且圓心落在矩形內時，依最近逃離面取法線；等距才借用本輪意圖破同分。
    const double gapX=ex-std::abs(x),gapZ=ez-std::abs(z);
    const bool axisX=gapX<gapZ||(gapX==gapZ&&std::abs(dx)>=std::abs(dz));
    if(axisX)return {x<0?-1.f:x>0?1.f:dx<0?-1.f:1.f,0,0};
    return {0,0,z<0?-1.f:z>0?1.f:dz<0?-1.f:1.f};
}
}

// 玩家 box 的相對平移與圓柱做連續查詢；要查雙方移動時傳入 playerDelta-npcDelta。
// 回傳 false 時 out 原樣保留。VerticalEntry 必須交由讓位／交易政策處理，不能忽略為 Clear。
inline bool QueryBoxCylinderContact3D(Vec3 center,Vec3 extents,Vec3 relativeTranslation,
    const DynamicCylinder3D& body,BoxCylinderContact3D& out) noexcept try {
    using namespace DynamicContactDetail3D;
    const auto finite=Finite;
    if(!finite(center)||!finite(extents)||!finite(relativeTranslation)||!finite(body.center)||
        extents.x<=0||extents.y<=0||extents.z<=0||!std::isfinite(body.radius)||body.radius<=0||
        !std::isfinite(body.halfHeight)||body.halfHeight<=0||!finite(center+relativeTranslation)||
        !finite(center+extents)||!finite(center-extents)||
        !finite(center+relativeTranslation+extents)||!finite(center+relativeTranslation-extents))return false;
    const Vec3 bodyExtents{body.radius,body.halfHeight,body.radius};
    if(!finite(body.center+bodyExtents)||!finite(body.center-bodyExtents))return false;
    const double x=double(center.x)-body.center.x,y=double(center.y)-body.center.y,z=double(center.z)-body.center.z;
    const double dx=relativeTranslation.x,dy=relativeTranslation.y,dz=relativeTranslation.z;
    const double ex=extents.x,ez=extents.z,radius=body.radius,height=double(extents.y)+body.halfHeight;
    const double limit=(std::numeric_limits<float>::max)();
    if(ex+radius>limit||ez+radius>limit||height>limit)return false;
    // 明訂支援尺度，拒絕不可解析的極端跨度；這不是全部 double 根的 exact 正確性證明。
    // 目前 Session ±10000、玩家 .45/.9、野豬 1.3/.8 遠在此域內；不按測試結果調門檻。
    const double feature=(std::min)({ex,double(extents.y),ez,radius,double(body.halfHeight)});
    const double magnitude=(std::max)({std::abs(double(center.x)),std::abs(double(center.y)),std::abs(double(center.z)),
        std::abs(double(body.center.x)),std::abs(double(body.center.y)),std::abs(double(body.center.z)),
        std::abs(dx),std::abs(dy),std::abs(dz),ex,double(extents.y),ez,radius,double(body.halfHeight)});
    if(magnitude>feature*16777216.)return false;
    const ExactInput exact{center,extents,relativeTranslation,body};
    BoxCylinderContact3D result;
    double separation=SignedRoundedDistance(x,z,ex,ez,radius);
    constexpr double epsilon=std::numeric_limits<double>::epsilon();
    const double horizontalError=64*epsilon*(std::abs(double(center.x))+std::abs(double(center.z))+
        std::abs(double(body.center.x))+std::abs(double(body.center.z))+ex+ez+radius);
    int horizontalSign=(separation>0)-(separation<0);
    if(std::abs(separation)<=horizontalError){
        const int sign=HorizontalAt(exact,Exact{}).sign;
        if(sign&&sign!=horizontalSign)return false;
        horizontalSign=sign;if(!sign)separation=0;
    }
    const double yError=8*epsilon*(std::abs(double(center.y))+std::abs(double(body.center.y))+height);
    int ySign=(std::abs(y)>height)-(std::abs(y)<height);
    if(std::abs(std::abs(y)-height)<=yError)
        ySign=(Abs(F(center.y)-F(body.center.y))-F(extents.y)-F(body.halfHeight)).Sign();
    if(horizontalSign<0&&ySign<0){
        if(!(separation<0))return false;
        result.kind=BoxCylinderContactKind3D::StartedOverlapping;result.first=0;
        result.horizontalNormal=HorizontalNormal(x,z,dx,dz,ex,ez);result.horizontalPenetration=-separation;
        out=result;return true;
    }
    const auto horizontal=RoundedRectangle(x,z,dx,dz,ex,ez,radius,&exact);
    const auto vertical=dy==0?(ySign<0?Interval{}:Interval{0,0,false}):ConstantSlab(y,dy,height);
    if(!horizontal.resolved||!vertical.resolved)return false;
    const auto contact=Intersect(Intersect(horizontal,vertical),{0,1,true});
    bool exactVertical=false,simultaneous=false;
    double verticalTime{};
    // 一般同高行走保留 double 快路徑；只有 Y 入界／離界可能參與事件時建立精確候選。
    if(dy!=0&&(ySign>=0||(vertical.last>=0&&vertical.last<=1))){
        const auto ey=F(extents.y)+F(body.halfHeight),initialY=F(center.y)-F(body.center.y),motionY=F(relativeTranslation.y);
        const auto entry=((dy<0?ey:-ey)-initialY)/motionY;
        const auto exit=((dy<0?-ey:ey)-initialY)/motionY;
        const Exact zero,one(1);
        if(Compare(exit,zero)<=0||Compare(entry,one)>=0){out=result;return true;}
        if(Compare(entry,zero)>=0){
            const auto h=HorizontalAt(exact,entry);
            // 頂面入界時水平已離開／正在切向接觸，不存在之後的嚴格體積交集。
            if(h.sign>=0&&h.direction>=0){out=result;return true;}
            if(h.sign<=0){
                exactVertical=true;simultaneous=h.sign==0;
                verticalTime=entry.Approximate();
                if(!std::isfinite(verticalTime)||verticalTime<0||verticalTime>=1)return false;
            }
        }
        if(Compare(exit,zero)>0&&Compare(exit,one)<=0){
            const auto h=HorizontalAt(exact,exit);
            // Y 已離界才剛進入水平體積，只有邊界點；不能假報同時碰撞。
            if(h.sign>=0&&h.direction<=0){out=result;return true;}
            if(h.sign<0&&!contact.valid&&!exactVertical)return false;
        }
    }
    if(!contact.valid&&!exactVertical){out=result;return true;}
    result.first=exactVertical?verticalTime:contact.first;
    const bool side=!exactVertical||simultaneous,top=exactVertical;
    if(side)result.horizontalNormal=HorizontalNormal(x+dx*result.first,z+dz*result.first,dx,dz,ex,ez);
    if(top)result.verticalNormal={0,dy<0?1.f:-1.f,0};
    result.kind=side&&top?BoxCylinderContactKind3D::SimultaneousEntry:side?BoxCylinderContactKind3D::SideEntry:BoxCylinderContactKind3D::VerticalEntry;
    if(!Finite(result.horizontalNormal)||!Finite(result.verticalNormal)||!std::isfinite(result.first)||result.first<0||result.first>=1)return false;
    out=result;return true;
}catch(...){return false;}
}
