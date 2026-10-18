#pragma once
#include <Aevrynd/SkinnedModel3D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Mistwatch {
enum class ExplorerArtMaterial : unsigned { Fabric, Leather, Skin, Hair, Metal, Count };
struct ExplorerArtRange { std::uint32_t firstIndex{},indexCount{};ExplorerArtMaterial material{}; };
struct ExplorerArtJoin { std::vector<std::uint32_t> boundary;unsigned joint{}; };
struct ExplorerArtReceipt {
    std::array<ExplorerArtRange,5> ranges{};
    std::vector<ExplorerArtJoin> joins;
    std::array<std::vector<std::uint32_t>,15> criticalVertices;
};
inline std::array<Aevrynd::Material3D,5> ExplorerArtMaterials() {
    std::array<Aevrynd::Material3D,5> result;
    result[0].surface={.86f,0,0,1};result[0].shadingModel=Aevrynd::CharacterShadingModel3D::Cloth;
    result[1].surface={.67f,0,0,1};
    result[2].surface={.61f,0,0,1};result[2].shadingModel=Aevrynd::CharacterShadingModel3D::Skin;
    result[3].surface={.78f,0,0,1};result[3].shadingModel=Aevrynd::CharacterShadingModel3D::Hair;
    result[4].surface={.34f,.72f,0,1};return result;
}
namespace ExplorerArtAuthor {
using Aevrynd::Vec3;using Aevrynd::Color;using Aevrynd::SkinInfluences3D;
using Ring=std::vector<std::uint32_t>;
inline constexpr float Pi=3.14159265358979323846f;
inline constexpr Color Coat{.24f,.31f,.225f,1},CoatLight{.31f,.37f,.27f,1},Seam{.12f,.16f,.115f,1};
inline constexpr Color Pants{.17f,.19f,.20f,1},Leather{.115f,.09f,.065f,1},Sole{.048f,.049f,.043f,1};
inline constexpr Color Skin{.58f,.40f,.29f,1},Hair{.10f,.075f,.052f,1},Pack{.31f,.235f,.145f,1};
inline Vec3 Unit(Vec3 v) { const float n=std::sqrt(Aevrynd::Dot(v,v));return n>1.e-12f?v*(1.f/n):Vec3{0,1,0}; }
inline SkinInfluences3D Weights(std::initializer_list<std::pair<unsigned,float>> values) {
    std::map<unsigned,float> combined;for(const auto [joint,w]:values)if(w>0)combined[joint]+=w;
    std::vector<std::pair<unsigned,float>> sorted(combined.begin(),combined.end());
    std::sort(sorted.begin(),sorted.end(),[](auto a,auto b){return a.second>b.second;});
    if(sorted.empty()||sorted.size()>4)throw std::runtime_error("Explorer influence count");
    float sum{};for(const auto& entry:sorted)sum+=entry.second;
    SkinInfluences3D result;std::uint32_t total{};
    for(std::size_t i=0;i<sorted.size();++i){result.joints[i]=static_cast<std::uint16_t>(sorted[i].first);
        result.weights[i]=static_cast<std::uint16_t>(std::floor(sorted[i].second/sum*65535.f));total+=result.weights[i];}
    result.weights[0]=static_cast<std::uint16_t>(result.weights[0]+65535u-total);
    for(std::size_t i=0;i<4;++i)if(!result.weights[i])result.joints[i]=0;
    return result;
}
inline SkinInfluences3D Rigid(unsigned j){return Weights({{j,1.f}});}
inline SkinInfluences3D Blend(unsigned a,unsigned b,float t){t=Aevrynd::Clamp(t,0.f,1.f);return Weights({{a,1-t},{b,t}});}
inline SkinInfluences3D TorsoWeights(Vec3 p) {
    const float spine=Aevrynd::Clamp((p.y-.96f)/.22f,0.f,1.f);
    const float shoulder=Aevrynd::Clamp((std::abs(p.x)-.13f)/.09f,0.f,1.f)*Aevrynd::Clamp((p.y-1.20f)/.13f,0.f,1.f)*.72f;
    return Weights({{0,(1-spine)*(1-shoulder)},{1,spine*(1-shoulder)},{p.x<0?3u:6u,shoulder}});
}
inline SkinInfluences3D ArmWeights(unsigned upper,float y) {
    if(y<.94f)return Blend(upper+1u,upper+2u,(.90f-y)/.08f);
    return Blend(upper,upper+1u,(1.18f-y)/.16f);
}
inline SkinInfluences3D LegWeights(unsigned upper,float y) {
    if(y>.79f)return Blend(0,upper,(.94f-y)/.15f);
    if(y<.21f)return Blend(upper+1u,upper+2u,(.19f-y)/.07f);
    return Blend(upper,upper+1u,(.56f-y)/.16f);
}

// 作者幾何在遊戲層；共用引擎仍負責法線封裝、骨架、蒙皮與材質提交。
class Builder {
public:
    explicit Builder(Aevrynd::SkinnedModel3D& target):model(target) {
        if(model.skeleton.joints.size()!=15||model.inverseBindMatrices.size()!=15)throw std::runtime_error("Explorer requires existing 15-joint rig");
        model.bindMesh.vertices.clear();model.bindMesh.indices.clear();model.influences.clear();model.tangents.clear();
    }
    Aevrynd::SkinnedModel3D& model;ExplorerArtReceipt receipt;
    std::vector<Vec3> hints;
    std::array<std::vector<std::uint32_t>,5> triangles;
    std::uint32_t Vertex(Vec3 p,Vec3 n,Color color,SkinInfluences3D influence,float u=0,float v=0) {
        const auto index=static_cast<std::uint32_t>(model.bindMesh.vertices.size());
        model.bindMesh.vertices.push_back({p.x,p.y,p.z,Aevrynd::PackNormalSnorm8(Unit(n)),Aevrynd::PackColorRgba8(color),u,v});
        model.influences.push_back(influence);hints.push_back(Unit(n));return index;
    }
    Vec3 Position(std::uint32_t i)const{const auto& v=model.bindMesh.vertices.at(i);return {v.x,v.y,v.z};}
    void Triangle(std::uint32_t a,std::uint32_t b,std::uint32_t c,ExplorerArtMaterial material,Vec3 expected) {
        const auto area=Aevrynd::Cross(Position(b)-Position(a),Position(c)-Position(a));
        if(Aevrynd::Dot(area,area)<1.e-16f)throw std::runtime_error("Explorer degenerate triangle");
        if(Aevrynd::Dot(area,expected)<0)std::swap(b,c);
        auto& list=triangles[static_cast<unsigned>(material)];list.insert(list.end(),{a,b,c});
    }
    void Face(std::uint32_t a,std::uint32_t b,std::uint32_t c,ExplorerArtMaterial m){Triangle(a,b,c,m,hints[a]+hints[b]+hints[c]);}
    Ring Ellipse(Vec3 centre,float rx,float rz,unsigned sides,Color color,SkinInfluences3D influence) {
        Ring result;result.reserve(sides);
        for(unsigned i=0;i<sides;++i){const float angle=2*Pi*static_cast<float>(i)/static_cast<float>(sides);
            result.push_back(Vertex(centre+Vec3{rx*std::sin(angle),0,rz*std::cos(angle)},
                {std::sin(angle)/rx,0,std::cos(angle)/rz},color,influence,static_cast<float>(i)/static_cast<float>(sides),centre.y));}
        return result;
    }
    // 連接沿用原頂點，肩、肘、膝、褲檔不再以獨立球體重疊。
    void Connect(const Ring& first,const Ring& second,ExplorerArtMaterial material,bool align=false) {
        if(first.size()!=second.size()||first.size()<3)throw std::runtime_error("Explorer ring size mismatch");
        Ring to=second;
        if(align){float best=1.e20f;Ring candidate(second.size());
            for(unsigned reverse=0;reverse<2;++reverse)for(std::size_t shift=0;shift<second.size();++shift){
                float cost{};for(std::size_t i=0;i<second.size();++i){const auto j=reverse?(shift+second.size()-i)%second.size():(shift+i)%second.size();
                    candidate[i]=second[j];const Vec3 delta=Position(first[i])-Position(candidate[i]);cost+=Aevrynd::Dot(delta,delta);}
                if(cost<best){best=cost;to=candidate;}}
        }
        std::vector<std::array<std::uint32_t,3>> strip;float orientation{};
        for(std::size_t i=0;i<first.size();++i){const auto next=(i+1u)%first.size();
            strip.push_back({first[i],to[i],first[next]});strip.push_back({first[next],to[i],to[next]});}
        for(const auto triangle:strip){const auto a=triangle[0],b=triangle[1],c=triangle[2];
            const Vec3 area=Aevrynd::Cross(Position(b)-Position(a),Position(c)-Position(a));
            if(Aevrynd::Dot(area,area)<1.e-16f)throw std::runtime_error("Explorer degenerate ring strip");
            orientation+=Aevrynd::Dot(area,hints[a]+hints[b]+hints[c]);}
        if(orientation==0)throw std::runtime_error("Explorer ambiguous ring strip orientation");
        // 同一接面必須一致繞序；袖窿底邊水平三角形不能靠零點法線任意翻面。
        auto& list=triangles[static_cast<unsigned>(material)];for(auto triangle:strip){
            if(orientation<0)std::swap(triangle[1],triangle[2]);list.insert(list.end(),triangle.begin(),triangle.end());}
    }
    void Cap(const Ring& ring,Vec3 normal,Color color,SkinInfluences3D influence,ExplorerArtMaterial material) {
        Vec3 p{};for(auto i:ring)p=p+Position(i);p=p*(1.f/static_cast<float>(ring.size()));
        const auto centre=Vertex(p,normal,color,influence);
        for(std::size_t i=0;i<ring.size();++i)Triangle(centre,ring[i],ring[(i+1u)%ring.size()],material,normal);
    }
    void Join(const Ring& ring,unsigned joint){receipt.joins.push_back({ring,joint});receipt.criticalVertices.at(joint).insert(receipt.criticalVertices[joint].end(),ring.begin(),ring.end());}
    void Loft(const std::vector<std::array<float,5>>& rows,unsigned sides,Color color,unsigned joint,ExplorerArtMaterial material) {
        Ring previous,first;for(const auto row:rows){const Ring ring=Ellipse({row[0],row[1],row[2]},row[3],row[4],sides,color,Rigid(joint));
            if(previous.empty())first=ring;else Connect(previous,ring,material);previous=ring;}
        Cap(first,{0,-1,0},color,Rigid(joint),material);Cap(previous,{0,1,0},color,Rigid(joint),material);
    }
    // 具真厚度的面部、衣縫、肩帶與手掌多邊形；耳切支援拇指及指間凹形。
    void Extrude(const std::vector<Aevrynd::Vec2>& outline,float back,float front,Color color,SkinInfluences3D weights,ExplorerArtMaterial material) {
        const std::size_t n=outline.size();if(n<3)throw std::runtime_error("Explorer polygon size");
        Ring low,high;for(const auto p:outline){low.push_back(Vertex({p.x,p.y,back},{0,0,-1},color,weights,p.x,p.y));high.push_back(Vertex({p.x,p.y,front},{0,0,1},color,weights,p.x,p.y));}
        float signedArea{};for(std::size_t i=0;i<n;++i){const auto a=outline[i],b=outline[(i+1u)%n];signedArea+=a.x*b.y-b.x*a.y;}
        const float sign=signedArea>0?1.f:-1.f;
        std::vector<std::size_t> remaining;for(std::size_t i=0;i<n;++i)remaining.push_back(i);
        const auto cross=[](Aevrynd::Vec2 a,Aevrynd::Vec2 b,Aevrynd::Vec2 c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);};
        while(remaining.size()>2){bool clipped=false;
            for(std::size_t k=0;k<remaining.size();++k){const auto a=remaining[(k+remaining.size()-1u)%remaining.size()],b=remaining[k],c=remaining[(k+1u)%remaining.size()];
                if(cross(outline[a],outline[b],outline[c])*sign<=1.e-10f)continue;
                bool contains=false;for(const auto p:remaining){if(p==a||p==b||p==c)continue;
                    if(cross(outline[a],outline[b],outline[p])*sign>=0&&cross(outline[b],outline[c],outline[p])*sign>=0&&cross(outline[c],outline[a],outline[p])*sign>=0){contains=true;break;}}
                if(contains)continue;
                Triangle(high[a],high[b],high[c],material,{0,0,1});Triangle(low[a],low[c],low[b],material,{0,0,-1});
                remaining.erase(remaining.begin()+static_cast<std::ptrdiff_t>(k));clipped=true;break;
            }
            if(!clipped)throw std::runtime_error("Explorer polygon ear cut failed");
        }
        for(std::size_t i=0;i<n;++i){const auto next=(i+1u)%n;const auto a=outline[i],b=outline[next];
            const Vec3 outward{(b.y-a.y)*sign,(a.x-b.x)*sign,0};
            // 側面另建頂點，保留縫線與薄板邊緣的硬法線。
            const auto a0=Vertex(Position(low[i]),outward,color,weights),b0=Vertex(Position(low[next]),outward,color,weights);
            const auto a1=Vertex(Position(high[i]),outward,color,weights),b1=Vertex(Position(high[next]),outward,color,weights);
            Triangle(a0,b0,a1,material,outward);Triangle(a1,b0,b1,material,outward);
        }
    }
    void Patch(float x,float y,float w,float h,float z,float thickness,Color color,SkinInfluences3D weights,ExplorerArtMaterial material) {
        const float cut=(std::min)(w,h)*.18f;
        Extrude({{x-w+cut,y-h},{x+w-cut,y-h},{x+w,y-h+cut},{x+w,y+h-cut},
            {x+w-cut,y+h},{x-w+cut,y+h},{x-w,y+h-cut},{x-w,y-h+cut}},z,z+thickness,color,weights,material);
    }
    ExplorerArtReceipt Finish() {
        for(unsigned group=0;group<triangles.size();++group){const auto first=static_cast<std::uint32_t>(model.bindMesh.indices.size());
            model.bindMesh.indices.insert(model.bindMesh.indices.end(),triangles[group].begin(),triangles[group].end());
            receipt.ranges[group]={first,static_cast<std::uint32_t>(triangles[group].size()),static_cast<ExplorerArtMaterial>(group)};}
        std::vector<Vec3> sums(model.bindMesh.vertices.size());
        for(std::size_t i=0;i<model.bindMesh.indices.size();i+=3){const auto a=model.bindMesh.indices[i],b=model.bindMesh.indices[i+1u],c=model.bindMesh.indices[i+2u];
            const Vec3 normal=Unit(Aevrynd::Cross(Position(b)-Position(a),Position(c)-Position(a)));
            for(const auto v:{a,b,c})sums[v]=sums[v]+normal;}
        for(std::size_t i=0;i<sums.size();++i){if(Aevrynd::Dot(sums[i],sums[i])<1.e-10f)throw std::runtime_error("Explorer unused or opposing normal vertex");
            model.bindMesh.vertices[i].normal=Aevrynd::PackNormalSnorm8(Unit(sums[i]));}
        model.bindMesh.InvalidateRaycastIndex();return receipt;
    }
};

inline void CoatBody(Builder& b) {
    constexpr std::array<std::array<float,3>,9> profiles{{{.89f,.175f,.125f},{.98f,.18f,.127f},{1.10f,.165f,.13f},
        {1.20f,.19f,.14f},{1.28f,.217f,.145f},{1.36f,.232f,.138f},{1.44f,.22f,.123f},{1.49f,.082f,.075f},{1.535f,.074f,.07f}}};
    std::array<Ring,9> rings;
    for(std::size_t row=0;row<profiles.size();++row){const auto p=profiles[row];
        rings[row].resize(32);
        for(unsigned i=0;i<32;++i){
            // 袖窿內側不存在面，不能建立六個無三角形引用的孤立頂點。
            if(row==5&&((i>6&&i<10)||(i>22&&i<26)))continue;
            const float angle=2*Pi*static_cast<float>(i)/32.f;
            const Vec3 position{p[1]*std::sin(angle),p[0],p[2]*std::cos(angle)};
            rings[row][i]=b.Vertex(position,{std::sin(angle)/p[1],0,std::cos(angle)/p[2]},Coat,TorsoWeights(position),static_cast<float>(i)/32.f,p[0]);
        }
        if(row){for(unsigned i=0;i<32;++i){if((row==5||row==6)&&((i>=6&&i<10)||(i>=22&&i<26)))continue;
            const unsigned next=(i+1u)%32u;b.Face(rings[row-1u][i],rings[row][i],rings[row-1u][next],ExplorerArtMaterial::Fabric);
            b.Face(rings[row-1u][next],rings[row][i],rings[row][next],ExplorerArtMaterial::Fabric);}}
    }
    b.Cap(rings.front(),{0,-1,0},Coat,Rigid(0),ExplorerArtMaterial::Fabric);
    b.Join(rings[0],0);b.Join(rings[3],1);
    // 領口與頭頸衣物重疊，衣物開口保持真幾何。
    for(unsigned side=0;side<2;++side){const unsigned start=side?6u:22u,upper=3u+side*3u;const float s=side?1.f:-1.f;
        Ring hole;for(unsigned i=0;i<=4;++i)hole.push_back(rings[4][start+i]);hole.push_back(rings[5][start+4u]);
        for(unsigned i=0;i<=4;++i)hole.push_back(rings[6][start+4u-i]);hole.push_back(rings[5][start]);
        b.Join(hole,upper);Ring previous=hole;
        constexpr std::array<std::array<float,3>,11> arm{{{1.26f,.070f,.083f},{1.22f,.079f,.078f},{1.16f,.071f,.074f},
            {1.10f,.067f,.071f},{1.055f,.064f,.067f},{1.01f,.060f,.065f},{.96f,.055f,.060f},{.91f,.049f,.054f},
            {.875f,.046f,.049f},{.85f,.044f,.045f},{.835f,.041f,.042f}}};
        for(std::size_t row=0;row<arm.size();++row){const auto p=arm[row];
            const float centreline=p[0]>1.10f?.265f+(1.28f-p[0])*.0833f:.28f;
            // 首圈低於袖窿下緣且內側在衣身之外；第二圈平順接回原上臂輪廓。
            const float collarOffset=row==0?.025f:row==1?.010f:0.f;
            const float x=s*(centreline+collarOffset);
            const auto ring=b.Ellipse({x,p[0],0},p[1],p[2],12,row>=9?Seam:Coat,ArmWeights(upper,p[0]));
            b.Connect(previous,ring,ExplorerArtMaterial::Fabric,row==0);previous=ring;
            if(row==3)b.Join(ring,upper+1u);if(row==9)b.Join(ring,upper+2u);
        }
        b.Cap(previous,{0,-1,0},Seam,Rigid(upper+2u),ExplorerArtMaterial::Fabric);
    }
    // 領片、門襟、胸袋、袋蓋與腰部縫線有厚度；各衣片使用所處骨節的相同蒙皮。
    b.Extrude({{-.079f,1.535f},{-.017f,1.45f},{-.048f,1.414f},{-.105f,1.477f}},.071f,.080f,CoatLight,Rigid(1),ExplorerArtMaterial::Fabric);
    b.Extrude({{.079f,1.535f},{.017f,1.45f},{.048f,1.414f},{.105f,1.477f}},.071f,.080f,CoatLight,Rigid(1),ExplorerArtMaterial::Fabric);
    for(unsigned row=0;row<8;++row){const float y=.99f+static_cast<float>(row)*.058f;
        b.Patch(0,y,.010f,.031f,.143f,.006f,Seam,TorsoWeights({0,y,0}),ExplorerArtMaterial::Fabric);
        b.Patch(0,y,.003f,.022f,.151f,.002f,{.39f,.39f,.33f,1},TorsoWeights({0,y,0}),ExplorerArtMaterial::Metal);}
    for(float x:{-.107f,.107f}){b.Patch(x,1.275f,.045f,.058f,.127f,.009f,CoatLight,Rigid(1),ExplorerArtMaterial::Fabric);
        b.Patch(x,1.331f,.050f,.013f,.139f,.006f,Seam,Rigid(1),ExplorerArtMaterial::Fabric);}
}

inline void PantsAndBoots(Builder& b) {
    auto waist=b.Ellipse({0,.945f,0},.176f,.123f,24,Pants,Rigid(0));
    b.Cap(waist,{0,1,0},Pants,Rigid(0),ExplorerArtMaterial::Fabric);
    auto hips=b.Ellipse({0,.855f,0},.184f,.130f,24,Pants,Rigid(0));b.Connect(waist,hips,ExplorerArtMaterial::Fabric);
    for(const auto v:hips){const Vec3 p=b.Position(v);b.model.influences[v]=std::abs(p.x)<1.e-6f?
        Weights({{0,.43333334f},{9,.28333333f},{12,.28333333f}}):LegWeights(p.x<0?9u:12u,p.y);}
    // 褲檔共享前後中心及三個內側頂點；兩腿是真分叉面，不是髖部上的兩顆橢球。
    Ring crotch;for(float z:{-.065f,0.f,.065f})crotch.push_back(b.Vertex({0,.79f,z},{0,-1,0},Pants,Weights({{0,.5f},{9,.25f},{12,.25f}})));
    for(unsigned side=0;side<2;++side){const float s=side?1.f:-1.f;const unsigned upper=9u+side*3u;Ring opening;
        if(side){for(unsigned i=0;i<=12;++i)opening.push_back(hips[i]);for(auto i:crotch)opening.push_back(i);}
        else {for(unsigned i=12;i<24;++i)opening.push_back(hips[i]);opening.push_back(hips[0]);for(auto i=crotch.rbegin();i!=crotch.rend();++i)opening.push_back(*i);}
        b.Join(opening,upper);Ring previous=opening;
        constexpr std::array<std::array<float,3>,14> leg{{{.775f,.100f,.116f},{.72f,.101f,.109f},{.64f,.094f,.102f},{.56f,.087f,.096f},
            {.51f,.083f,.090f},{.48f,.081f,.087f},{.45f,.080f,.086f},{.405f,.079f,.085f},{.35f,.077f,.083f},
            {.28f,.071f,.078f},{.23f,.066f,.071f},{.20f,.064f,.068f},{.18f,.063f,.067f},{.165f,.062f,.066f}}};
        for(std::size_t row=0;row<leg.size();++row){const auto p=leg[row];auto ring=b.Ellipse({s*.12f,p[0],0},p[1],p[2],16,Pants,LegWeights(upper,p[0]));
            b.Connect(previous,ring,ExplorerArtMaterial::Fabric,row==0);previous=ring;if(row==5)b.Join(ring,upper+1u);if(row==12)b.Join(ring,upper+2u);}
        b.Cap(previous,{0,-1,0},Pants,Rigid(upper+2u),ExplorerArtMaterial::Fabric);
        // 側縫與膝部摺線分成短片，避免以整條剛性飾條跨越可彎膝蓋。
        for(unsigned row=0;row<9;++row){const float y=.26f+static_cast<float>(row)*.058f;
            b.Patch(s*.161f,y,.003f,.026f,.087f,.002f,Seam,LegWeights(upper,y),ExplorerArtMaterial::Fabric);}
        b.Loft({{s*.12f,.03f,.059f,.079f,.142f},{s*.12f,.065f,.065f,.081f,.143f},
            {s*.12f,.105f,.043f,.074f,.119f},{s*.12f,.135f,.015f,.067f,.080f},{s*.12f,.20f,0,.070f,.073f}},16,Leather,upper+2u,ExplorerArtMaterial::Leather);
        b.Loft({{s*.12f,.005f,.059f,.081f,.145f},{s*.12f,.030f,.059f,.083f,.147f}},16,Sole,upper+2u,ExplorerArtMaterial::Leather);
        for(unsigned lace=0;lace<4;++lace)b.Patch(s*.12f,.113f+static_cast<float>(lace)*.017f,.046f,.0025f,.093f,.004f,Seam,Rigid(upper+2u),ExplorerArtMaterial::Fabric);
    }
}

inline void HeadAndHands(Builder& b) {
    const auto headFirst=static_cast<std::uint32_t>(b.model.bindMesh.vertices.size());
    // 24公分頭高、約7.5頭身；下顎、顴骨、眉額由作者輪廓控制。
    b.Loft({{0,1.49f,0,.047f,.051f},{0,1.535f,0,.048f,.052f}},20,Skin,2,ExplorerArtMaterial::Skin);
    b.Loft({{0,1.535f,.018f,.041f,.049f},{0,1.565f,.012f,.065f,.068f},{0,1.61f,.002f,.082f,.079f},
        {0,1.655f,-.001f,.090f,.088f},{0,1.705f,-.009f,.085f,.090f},{0,1.745f,-.016f,.070f,.076f},
        {0,1.775f,-.019f,.029f,.038f}},28,Skin,2,ExplorerArtMaterial::Skin);
    // 鼻樑、鼻尖、眼眶、眼白、瞳孔、眉毛與上下唇均為實際網格。
    b.Extrude({{-.012f,1.684f},{.012f,1.684f},{.017f,1.632f},{.024f,1.620f},{0,1.614f},{-.024f,1.620f},{-.017f,1.632f}},.080f,.111f,Skin,Rigid(2),ExplorerArtMaterial::Skin);
    for(float s:{-1.f,1.f}){const float x=s*.041f;
        b.Patch(x,1.669f,.022f,.011f,.076f,.012f,{.39f,.245f,.18f,1},Rigid(2),ExplorerArtMaterial::Skin);
        b.Patch(x,1.669f,.016f,.0045f,.090f,.002f,{.79f,.77f,.68f,1},Rigid(2),ExplorerArtMaterial::Skin);
        b.Patch(x-s*.002f,1.669f,.0043f,.0043f,.093f,.002f,{.16f,.20f,.145f,1},Rigid(2),ExplorerArtMaterial::Skin);
        b.Patch(x,1.690f,.024f,.004f,.079f,.007f,Hair,Rigid(2),ExplorerArtMaterial::Hair);
        b.Patch(s*.018f,1.62f,.0045f,.003f,.112f,.0015f,{.26f,.15f,.10f,1},Rigid(2),ExplorerArtMaterial::Skin);
        b.Loft({{s*.086f,1.621f,-.004f,.013f,.012f},{s*.092f,1.652f,-.008f,.017f,.013f},{s*.085f,1.678f,-.012f,.010f,.010f}},10,Skin,2,ExplorerArtMaterial::Skin);
    }
    b.Patch(0,1.593f,.025f,.003f,.079f,.005f,{.37f,.20f,.155f,1},Rigid(2),ExplorerArtMaterial::Skin);
    b.Patch(0,1.588f,.023f,.003f,.077f,.005f,{.50f,.29f,.22f,1},Rigid(2),ExplorerArtMaterial::Skin);
    // 新短髮與戶外帽：頭頂輪廓、帽帶與前帽簷都不借用既有人物。
    b.Loft({{0,1.727f,-.014f,.087f,.093f},{0,1.755f,-.016f,.077f,.084f},{0,1.780f,-.016f,.039f,.047f}},24,Hair,2,ExplorerArtMaterial::Hair);
    b.Loft({{0,1.75f,-.011f,.100f,.104f},{0,1.76f,-.011f,.100f,.104f},{0,1.795f,-.017f,.081f,.090f},{0,1.807f,-.018f,.056f,.067f}},24,Coat,2,ExplorerArtMaterial::Fabric);
    b.Extrude({{-.091f,1.749f},{.091f,1.749f},{.079f,1.760f},{-.079f,1.760f}},.081f,.150f,CoatLight,Rigid(2),ExplorerArtMaterial::Fabric);
    const auto headEnd=static_cast<std::uint32_t>(b.model.bindMesh.vertices.size());
    for(auto i=headFirst;i<headEnd;++i)b.receipt.criticalVertices[2].push_back(i);
    for(unsigned side=0;side<2;++side){const float s=side?1.f:-1.f,x=s*.28f;const unsigned hand=5u+side*3u;
        const std::vector<Aevrynd::Vec2> local{{-.032f,.846f},{.032f,.846f},{.039f,.800f},{.047f,.780f},{.043f,.754f},{.035f,.745f},
            {.028f,.762f},{.026f,.734f},{.023f,.713f},{.014f,.710f},{.011f,.739f},{.006f,.738f},{.004f,.707f},
            {-.005f,.706f},{-.009f,.738f},{-.014f,.739f},{-.015f,.714f},{-.024f,.718f},{-.026f,.747f},
            {-.031f,.726f},{-.039f,.733f},{-.037f,.778f}};
        std::vector<Aevrynd::Vec2> outline;for(const auto p:local)outline.push_back({x+s*p.x,p.y});
        const auto first=static_cast<std::uint32_t>(b.model.bindMesh.vertices.size());
        b.Extrude(outline,-.028f,.028f,Leather,Rigid(hand),ExplorerArtMaterial::Leather);
        const auto end=static_cast<std::uint32_t>(b.model.bindMesh.vertices.size());for(auto i=first;i<end;++i)b.receipt.criticalVertices[hand].push_back(i);
        b.Patch(x,.823f,.032f,.006f,.029f,.003f,CoatLight,Rigid(hand),ExplorerArtMaterial::Fabric);
    }
}

inline void Backpack(Builder& b) {
    b.Loft({{0,1.03f,-.18f,.13f,.085f},{0,1.07f,-.21f,.15f,.105f},{0,1.34f,-.21f,.15f,.10f},
        {0,1.39f,-.19f,.115f,.080f}},16,Pack,1,ExplorerArtMaterial::Leather);
    b.Patch(0,1.235f,.105f,.091f,-.333f,.012f,Leather,Rigid(1),ExplorerArtMaterial::Leather);
    b.Patch(0,1.329f,.114f,.017f,-.347f,.011f,Pack,Rigid(1),ExplorerArtMaterial::Leather);
    for(float s:{-1.f,1.f}){
        // 分段肩帶經肩上通往背包；重用身體权重，手臂擺動時不拉裂跨關節帶。
        for(unsigned row=0;row<6;++row){const float y=1.15f+static_cast<float>(row)*.047f,x=s*(.124f+(y-1.15f)*.045f);
            b.Patch(x,y,.016f,.027f,.118f,.011f,Leather,TorsoWeights({x,y,0}),ExplorerArtMaterial::Leather);}
        b.Patch(s*.124f,1.436f,.018f,.026f,-.123f,.22f,Leather,Weights({{1,.9f},{s<0?3u:6u,.1f}}),ExplorerArtMaterial::Leather);
        b.Patch(s*.093f,1.215f,.018f,.012f,-.356f,.006f,{.45f,.43f,.36f,1},Rigid(1),ExplorerArtMaterial::Metal);
    }
}
}
inline ExplorerArtReceipt BuildExplorerArtMesh(Aevrynd::SkinnedModel3D& model) {
    ExplorerArtAuthor::Builder builder(model);ExplorerArtAuthor::CoatBody(builder);
    ExplorerArtAuthor::PantsAndBoots(builder);ExplorerArtAuthor::HeadAndHands(builder);ExplorerArtAuthor::Backpack(builder);
    return builder.Finish();
}
}
