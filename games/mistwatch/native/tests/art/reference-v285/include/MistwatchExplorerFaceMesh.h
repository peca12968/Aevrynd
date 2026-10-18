#pragma once
#include "MistwatchExplorerArtMesh.h"
#include <array>
#include <cmath>
#include <vector>

namespace Mistwatch::ExplorerArtAuthor {
struct ExplorerFaceAudit {
    std::vector<std::uint32_t> mainSurface;
    std::array<Ring,2> socketBoundary,apertureBoundary;
    std::array<std::vector<std::uint32_t>,2> eyeLens,eyelids,earFront;
    std::vector<std::uint32_t> eyebrow;
    std::size_t firstVertex{},endVertex{};
};
namespace FaceAuthor {
inline constexpr unsigned Columns=48;
inline constexpr std::array<float,42> Rows{{1.535f,1.542f,1.550f,1.558f,1.565f,1.572f,1.579f,
    1.583f,1.586f,1.589f,1.591f,1.593f,1.595f,1.598f,1.602f,1.607f,1.612f,1.618f,1.622f,
    1.626f,1.630f,1.635f,1.640f,1.646f,1.651f,1.656f,1.660f,1.664f,1.668f,1.672f,
    1.676f,1.680f,1.684f,1.689f,1.695f,1.702f,1.710f,1.720f,1.735f,1.750f,1.765f,1.775f}};
inline constexpr unsigned SocketLowRow=24,SocketHighRow=31;
inline constexpr unsigned EyeSides=2u*((7u-1u)+(SocketHighRow-SocketLowRow));
inline constexpr float EyeX=.034f,EyeY=1.668f;
inline float Gaussian(float value,float width){const float t=value/width;return std::exp(-t*t);}
inline float BrowY(float x,float centre){const float u=(x-centre)/.020f;return 1.685f+.0032f*(1.f-u*u);}
inline float LipY(float x){return 1.591f-.0020f*std::abs(x)/.025f+.0012f*Gaussian(std::abs(x)-.008f,.005f);}
struct Profile {float radiusX{},radiusZ{},centreZ{};};
inline Profile HeadProfile(float y) {
    constexpr std::array<std::array<float,4>,10> points{{{1.535f,.039f,.049f,.017f},{1.550f,.052f,.059f,.013f},
        {1.572f,.069f,.069f,.008f},{1.612f,.084f,.079f,.001f},{1.646f,.090f,.087f,-.002f},
        {1.681f,.089f,.092f,-.007f},{1.710f,.085f,.092f,-.010f},{1.740f,.074f,.082f,-.016f},
        {1.765f,.049f,.057f,-.018f},{1.775f,.027f,.037f,-.019f}}};
    for(std::size_t i=1;i<points.size();++i)if(y<=points[i][0]){const auto a=points[i-1u],b=points[i];
        const float t=Aevrynd::Clamp((y-a[0])/(b[0]-a[0]),0.f,1.f);
        return {a[1]+(b[1]-a[1])*t,a[2]+(b[2]-a[2])*t,a[3]+(b[3]-a[3])*t};}
    const auto p=points.back();return {p[1],p[2],p[3]};
}
inline float Displacement(float x,float y) {
    float result{};
    for(float s:{-1.f,1.f}){
        const float eye=s*EyeX;
        result-=.0055f*Gaussian(x-eye,.020f)*Gaussian(y-EyeY,.011f);
        result+=.0044f*Gaussian(x-eye,.025f)*Gaussian(y-BrowY(x,eye),.008f);
        result+=.0033f*Gaussian(x-s*.052f,.021f)*Gaussian(y-1.643f,.014f);
        result+=.0070f*Gaussian(x-s*.0165f,.010f)*Gaussian(y-1.621f,.007f);
        result-=.0032f*Gaussian(x-s*.0140f,.0055f)*Gaussian(y-1.6155f,.0042f);
    }
    // 鼻根、鼻樑、鼻尖與鼻翼是同一頭部表面的連續位移，無直方塊覆蓋。
    result+=.0105f*Gaussian(x,.0105f)*Gaussian(y-1.654f,.033f);
    result+=.0190f*Gaussian(x,.0125f)*Gaussian(y-1.625f,.0115f);
    result-=.0020f*Gaussian(x,.006f)*Gaussian(y-1.613f,.004f);
    const float mouthWidth=Gaussian(x,.025f);
    result+=.0034f*mouthWidth*Gaussian(y-LipY(x)-.0028f,.0032f);
    result+=.0048f*mouthWidth*Gaussian(y-LipY(x)+.0031f,.0037f);
    result-=.0018f*mouthWidth*Gaussian(y-LipY(x),.0014f);
    result-=.0020f*Gaussian(x,.012f)*Gaussian(y-1.603f,.006f);
    result+=.0032f*Gaussian(x,.028f)*Gaussian(y-1.562f,.014f);
    return result;
}
inline float Front(float x,float y){const auto p=HeadProfile(y);const float u=Aevrynd::Clamp(x/p.radiusX,-.9999f,.9999f);
    return p.centreZ+p.radiusZ*std::sqrt(1.f-u*u)+Displacement(x,y);}
inline Color BlendColor(Color a,Color b,float t){t=Aevrynd::Clamp(t,0.f,1.f);return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t,1};}
inline Color SkinColor(float x,float y){Color result=Skin;
    const float lip=Gaussian(x,.024f)*Gaussian(y-LipY(x),.0050f);
    result=BlendColor(result,{.47f,.275f,.235f,1},lip*.64f);
    for(float s:{-1.f,1.f}){const float pit=Gaussian(x-s*.014f,.0045f)*Gaussian(y-1.6155f,.0030f);
        result=BlendColor(result,{.30f,.195f,.145f,1},pit*.55f);}
    return result;
}
inline float Angle(unsigned column){if(column<=16)return static_cast<float>(column)*Pi/32.f;
    if(column<=32)return Pi*.5f+static_cast<float>(column-16u)*Pi/16.f;
    return Pi*1.5f+static_cast<float>(column-32u)*Pi/32.f;}
inline Vec3 Point(unsigned row,unsigned column){const float y=Rows.at(row),a=Angle(column);const auto p=HeadProfile(y);
    const float x=p.radiusX*std::sin(a),c=std::cos(a);return {x,y,p.centreZ+p.radiusZ*c+(c>0?Displacement(x,y)*c*c*c:0.f)};}
inline unsigned FirstColumn(unsigned side){return side?1u:41u;}
inline unsigned LastColumn(unsigned side){return side?7u:47u;}
inline bool RemovedVertex(unsigned row,unsigned col){for(unsigned side=0;side<2;++side)
    if(row>SocketLowRow&&row<SocketHighRow&&col>FirstColumn(side)&&col<LastColumn(side))return true;return false;}
inline bool RemovedCell(unsigned row,unsigned col){for(unsigned side=0;side<2;++side)
    if(row>=SocketLowRow&&row<SocketHighRow&&col>=FirstColumn(side)&&col<LastColumn(side))return true;return false;}
inline Ring SocketRing(const std::vector<Ring>& grid,unsigned side){const unsigned first=FirstColumn(side),last=LastColumn(side);Ring result;
    for(unsigned c=first;c<=last;++c)result.push_back(grid[SocketLowRow][c]);
    for(unsigned r=SocketLowRow+1u;r<=SocketHighRow;++r)result.push_back(grid[r][last]);
    for(unsigned c=last;c>first;--c)result.push_back(grid[SocketHighRow][c-1u]);
    for(unsigned r=SocketHighRow-1u;r>SocketLowRow;--r)result.push_back(grid[r][first]);return result;}
inline Ring SkinShell(Builder& b,ExplorerFaceAudit* audit){std::vector<Ring> grid(Rows.size(),Ring(Columns));
    for(unsigned row=0;row<Rows.size();++row)for(unsigned col=0;col<Columns;++col){if(RemovedVertex(row,col))continue;
        const auto p=Point(row,col);const float a=Angle(col);const Color color=std::cos(a)>0?SkinColor(p.x,p.y):Skin;
        grid[row][col]=b.Vertex(p,{std::sin(a),0,std::cos(a)},color,Rigid(2),static_cast<float>(col)/static_cast<float>(Columns),(p.y-1.535f)/.240f);
        if(audit)audit->mainSurface.push_back(grid[row][col]);}
    for(unsigned row=0;row+1u<Rows.size();++row)for(unsigned col=0;col<Columns;++col){if(RemovedCell(row,col))continue;
        const auto next=(col+1u)%Columns;b.Face(grid[row][col],grid[row+1u][col],grid[row][next],ExplorerArtMaterial::Skin);
        b.Face(grid[row][next],grid[row+1u][col],grid[row+1u][next],ExplorerArtMaterial::Skin);}
    b.Cap(grid.front(),{0,-1,0},Skin,Rigid(2),ExplorerArtMaterial::Skin);b.Cap(grid.back(),{0,1,0},Skin,Rigid(2),ExplorerArtMaterial::Skin);
    for(unsigned side=0;side<2;++side){auto ring=SocketRing(grid,side);if(audit)audit->socketBoundary[side]=ring;}
    // 兩個真正socket開口的boundary由Eye直接共享填入，不用在完整皮面前放眼框。
    Ring all;for(unsigned side=0;side<2;++side){const auto ring=SocketRing(grid,side);all.insert(all.end(),ring.begin(),ring.end());}return all;
}
inline Vec3 EyePoint(float x,float width,float high,float low,unsigned index,float lift,bool ocular=false){const float a=2.f*Pi*static_cast<float>(index)/static_cast<float>(EyeSides);
    const float sine=std::sin(a),cosine=std::cos(a);const float taper=.40f+.60f*std::abs(sine);
    const float px=x+width*cosine,py=EyeY+(sine>=0?high:low)*sine*taper;
    const float depth=ocular?Front(x,EyeY)-(x>0?.18f:-.18f)*(px-x):Front(px,py);
    return {px,py,depth+lift};}
inline Ring EyeRing(Builder& b,float x,float width,float high,float low,float lift,Color color){Ring ring;
    for(unsigned i=0;i<EyeSides;++i){const auto p=EyePoint(x,width,high,low,i,lift,true);ring.push_back(b.Vertex(p,{0,0,1},color,Rigid(2),p.x,p.y));}return ring;}
inline void Eye(Builder& b,unsigned side,const Ring& socket,ExplorerFaceAudit* audit){const float x=side?EyeX:-EyeX;
    const auto centre=b.Vertex({x,EyeY,Front(x,EyeY)+.006f},{0,0,1},{.026f,.024f,.018f,1},Rigid(2));
    Ring previous;const std::array<std::array<float,4>,5> layers{{{.0022f,.0020f,.0020f,.0059f},{.0038f,.0028f,.0026f,.0057f},
        {.0048f,.0032f,.0030f,.0055f},{.0096f,.0045f,.0039f,.0042f},{.0145f,.0048f,.0042f,.0020f}}};
    constexpr std::array<Color,5> colors{{{.047f,.052f,.037f,1},{.23f,.20f,.12f,1},{.115f,.128f,.082f,1},{.73f,.74f,.68f,1},{.70f,.69f,.635f,1}}};
    if(audit)audit->eyeLens[side].push_back(centre);
    for(std::size_t layer=0;layer<layers.size();++layer){const auto data=layers[layer];const auto ring=EyeRing(b,x,data[0],data[1],data[2],data[3],colors[layer]);
        if(previous.empty()){for(unsigned i=0;i<EyeSides;++i)b.Triangle(centre,ring[i],ring[(i+1u)%EyeSides],ExplorerArtMaterial::Skin,{0,0,1});}
        else b.Connect(previous,ring,ExplorerArtMaterial::Skin);previous=ring;
        if(audit)audit->eyeLens[side].insert(audit->eyeLens[side].end(),ring.begin(),ring.end());}
    if(audit)audit->apertureBoundary[side]=previous;
    // 眼白、iris、pupil、上下眼瞼與眼窩都共享圈邊，無矩形眼框或平面貼白。
    constexpr std::array<std::array<float,4>,3> lids{{{.0162f,.0059f,.0052f,.0030f},{.0186f,.0065f,.0062f,.0011f},{.0206f,.0071f,.0068f,0.f}}};
    for(const auto data:lids){Ring ring;for(unsigned i=0;i<EyeSides;++i){const auto p=EyePoint(x,data[0],data[1],data[2],i,data[3]);
            ring.push_back(b.Vertex(p,{0,0,1},SkinColor(p.x,p.y),Rigid(2),p.x,p.y));}
        b.Connect(previous,ring,ExplorerArtMaterial::Skin);previous=ring;
        if(audit)audit->eyelids[side].insert(audit->eyelids[side].end(),ring.begin(),ring.end());}
    b.Connect(previous,socket,ExplorerArtMaterial::Skin,true);
}
inline void Brow(Builder& b,float centre,ExplorerFaceAudit* audit){constexpr unsigned length=20,width=2;std::array<Ring,width+1u> rows;
    for(unsigned across=0;across<=width;++across)for(unsigned i=0;i<=length;++i){const float u=-1.f+2.f*static_cast<float>(i)/static_cast<float>(length);
        const float x=centre+u*.019f,taper=.30f+.70f*std::sqrt((std::max)(0.f,1.f-u*u));
        const float y=BrowY(x,centre)+(static_cast<float>(across)-1.f)*.0017f*taper;
        const auto v=b.Vertex({x,y,Front(x,y)+.00075f},{0,0,1},Hair,Rigid(2),x,y);rows[across].push_back(v);if(audit)audit->eyebrow.push_back(v);}
    for(unsigned across=0;across<width;++across)for(unsigned i=0;i<length;++i){b.Face(rows[across][i],rows[across][i+1u],rows[across+1u][i],ExplorerArtMaterial::Hair);
        b.Face(rows[across+1u][i],rows[across][i+1u],rows[across+1u][i+1u],ExplorerArtMaterial::Hair);}
}
inline float EarDepth(float radius,float sine){return -.0030f*Gaussian(radius,.25f)+.0038f*Gaussian(radius-.84f,.15f)+.0015f*Gaussian(radius-.47f,.11f)*(1.f-.35f*sine);}
inline void Ear(Builder& b,unsigned side,ExplorerFaceAudit* audit){const float sign=side?1.f:-1.f;const Vec3 centre{sign*.0875f,1.651f,-.011f};
    const Vec3 horizontal{sign*.69f,0,-.72f},outward{sign*.72f,0,.69f};constexpr unsigned sides=24;
    const auto frontCentre=b.Vertex(centre+outward*EarDepth(0.f,0.f),outward,Skin,Rigid(2));Ring previous,outer;
    if(audit)audit->earFront[side].push_back(frontCentre);
    for(float radius:{.22f,.44f,.63f,.83f,1.f}){Ring ring;
        for(unsigned i=0;i<sides;++i){const float angle=2*Pi*static_cast<float>(i)/static_cast<float>(sides),c=std::cos(angle),s=std::sin(angle);
            const Vec3 p=centre+horizontal*(.0140f*radius*c)+Vec3{0,.026f*radius*s,0}+outward*EarDepth(radius,s);
            ring.push_back(b.Vertex(p,outward,BlendColor(Skin,{.54f,.345f,.26f,1},.20f*(1-radius)),Rigid(2),radius,c));}
        if(previous.empty())for(unsigned i=0;i<sides;++i)b.Triangle(frontCentre,ring[i],ring[(i+1u)%sides],ExplorerArtMaterial::Skin,outward);
        else b.Connect(previous,ring,ExplorerArtMaterial::Skin);previous=ring;
        if(audit)audit->earFront[side].insert(audit->earFront[side].end(),ring.begin(),ring.end());}
    outer=previous;Ring back;const auto backCentre=b.Vertex(centre+outward*-.0035f,outward*-1.f,Skin,Rigid(2));
    for(unsigned i=0;i<sides;++i){const float angle=2*Pi*static_cast<float>(i)/static_cast<float>(sides),c=std::cos(angle),s=std::sin(angle);
        const Vec3 p=centre+horizontal*(.014f*c)+Vec3{0,.026f*s,0}+outward*-.0035f;
        back.push_back(b.Vertex(p,outward*-1.f,Skin,Rigid(2)));}
    for(unsigned i=0;i<sides;++i){const unsigned next=(i+1u)%sides;const float angle=2*Pi*(static_cast<float>(i)+.5f)/static_cast<float>(sides);
        const Vec3 radial=horizontal*std::cos(angle)+Vec3{0,std::sin(angle),0};
        b.Triangle(backCentre,back[next],back[i],ExplorerArtMaterial::Skin,outward*-1.f);
        b.Triangle(outer[i],back[i],outer[next],ExplorerArtMaterial::Skin,radial);
        b.Triangle(outer[next],back[i],back[next],ExplorerArtMaterial::Skin,radial);}
}
}
inline void BuildExplorerFace(Builder& b,ExplorerFaceAudit* audit) {
    if(audit){*audit={};audit->firstVertex=b.model.bindMesh.vertices.size();}
    b.Loft({{0,1.49f,0,.047f,.051f},{0,1.535f,0,.048f,.052f}},20,Skin,2,ExplorerArtMaterial::Skin);
    const auto sockets=FaceAuthor::SkinShell(b,audit);
    for(unsigned side=0;side<2;++side){const Ring socket(sockets.begin()+side*FaceAuthor::EyeSides,sockets.begin()+(side+1u)*FaceAuthor::EyeSides);
        FaceAuthor::Eye(b,side,socket,audit);FaceAuthor::Brow(b,side?FaceAuthor::EyeX:-FaceAuthor::EyeX,audit);FaceAuthor::Ear(b,side,audit);}
    // 原帽髮與帽簷保留原V282作者座標，本包只重建成年頭臉。
    b.Loft({{0,1.727f,-.014f,.087f,.093f},{0,1.755f,-.016f,.077f,.084f},{0,1.780f,-.016f,.039f,.047f}},24,Hair,2,ExplorerArtMaterial::Hair);
    b.Loft({{0,1.75f,-.011f,.100f,.104f},{0,1.76f,-.011f,.100f,.104f},{0,1.795f,-.017f,.081f,.090f},{0,1.807f,-.018f,.056f,.067f}},24,Coat,2,ExplorerArtMaterial::Fabric);
    b.Extrude({{-.091f,1.749f},{.091f,1.749f},{.079f,1.760f},{-.079f,1.760f}},.081f,.150f,CoatLight,Rigid(2),ExplorerArtMaterial::Fabric);
    if(audit)audit->endVertex=b.model.bindMesh.vertices.size();
}
}
