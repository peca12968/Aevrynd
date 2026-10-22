#pragma once
#include "MistwatchWorld.h"
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/BeveledBox3D.h>
#include <string_view>
#include <stdexcept>
namespace Mistwatch {
enum class RangerHutArtMode { Baseline,MaterialOnly,TrimOnly,Combined };
struct RangerHutArtSettings {bool authoredUv{true},visibleTrim{true};};
inline RangerHutArtSettings RangerHutArtSettingsFor(RangerHutArtMode mode) {
    switch(mode) {
    case RangerHutArtMode::Baseline:return {false,false};
    case RangerHutArtMode::MaterialOnly:return {true,false};
    case RangerHutArtMode::TrimOnly:return {false,true};
    case RangerHutArtMode::Combined:return {true,true};
    }throw std::invalid_argument("Invalid ranger hut art mode");
}
inline RangerHutArtMode ParseRangerHutArtMode(std::wstring_view text) {
    if(text==L"baseline")return RangerHutArtMode::Baseline;
    if(text==L"material")return RangerHutArtMode::MaterialOnly;
    if(text==L"trim")return RangerHutArtMode::TrimOnly;
    if(text==L"combined")return RangerHutArtMode::Combined;
    throw std::invalid_argument("Invalid ranger hut art comparison flag");
}
inline const char* RangerHutArtModeName(RangerHutArtMode mode) {
    switch(mode) {
    case RangerHutArtMode::Baseline:return "baseline";
    case RangerHutArtMode::MaterialOnly:return "material";
    case RangerHutArtMode::TrimOnly:return "trim";
    case RangerHutArtMode::Combined:return "combined";
    }throw std::invalid_argument("Invalid ranger hut art mode");
}
inline Aevrynd::Material3D RangerHutMaterial(Aevrynd::Material3D wood,RangerHutArtSettings settings,bool noNormal=false) {
    if(settings.authoredUv){wood.useUv=true;wood.textureScale=1.f;wood.normalScale=.22f;}
    if(noNormal)wood.normalScale=0;
    return wood;
}
enum class RangerHutPartKind { Host,EntryFrame,Jamb,Sill,UpperFrame,Roof,Gable,WallBoard,CornerStile,Fascia };
struct RangerHutPart {RangerHutPartKind kind{};std::size_t firstIndex{},indexCount{};Aevrynd::Vec3 centre{};};
// 每面 U 沿可用最長軸；端面選次長軸，V 限於單一 siding 中央 .05 寬帶。
// 法線決定 V 的朝向，避免各面共用零 UV 或重複整張八條凸板。
inline Aevrynd::Vec2 RangerHutFaceUv(Aevrynd::Vec3 point,Aevrynd::Vec3 normal,Aevrynd::Vec3 size,std::size_t part) {
    const std::array lengths{size.x,size.y,size.z},positions{point.x,point.y,point.z},normals{normal.x,normal.y,normal.z};
    unsigned plane=0;for(unsigned axis=1;axis<3;++axis)if(std::abs(normals[axis])>std::abs(normals[plane]))plane=axis;
    unsigned grain=plane==0?1u:0u;for(unsigned axis=0;axis<3;++axis)if(axis!=plane&&lengths[axis]>lengths[grain])grain=axis;
    const unsigned across=3u-plane-grain;
    Aevrynd::Vec3 u{},v{};if(grain==0)u.x=1;else if(grain==1)u.y=1;else u.z=1;
    if(across==0)v.x=1;else if(across==1)v.y=1;else v.z=1;
    const float sign=Aevrynd::Dot(Aevrynd::Cross(u,v),normal)<0?-1.f:1.f;
    const float band=(float(part%8u)+.5f)/8.f,phase=.11f*float(part%5u);
    return {phase+(positions[grain]+.5f)*lengths[grain]*.5f,band+positions[across]*.05f*sign};
}
// Worn Planks 原圖為不等寬直板：以下四帶在已觀看原圖的天然灰褐板內，避開板縫／黃綠漆。
// U 跨板，V 順真木紋；沿紋 1.4m 才重複一次，不把整幅多板圖塞到每片木料。
inline constexpr std::array<Aevrynd::Vec2,4> HutWornPlankBands{{
    {132.f/1024.f,216.f/1024.f},{360.f/1024.f,436.f/1024.f},
    {694.f/1024.f,758.f/1024.f},{932.f/1024.f,1006.f/1024.f}}};
inline Aevrynd::Vec2 RangerHutWornPlankUv(Aevrynd::Vec3 point,Aevrynd::Vec3 normal,
    Aevrynd::Vec3 size,std::size_t part) {
    const std::array lengths{size.x,size.y,size.z},positions{point.x,point.y,point.z},normals{normal.x,normal.y,normal.z};
    unsigned plane=0;for(unsigned axis=1;axis<3;++axis)if(std::abs(normals[axis])>std::abs(normals[plane]))plane=axis;
    unsigned grain=plane==0?1u:0u;for(unsigned axis=0;axis<3;++axis)if(axis!=plane&&lengths[axis]>lengths[grain])grain=axis;
    const unsigned across=3u-plane-grain;
    Aevrynd::Vec3 u{},v{};if(across==0)u.x=1;else if(across==1)u.y=1;else u.z=1;
    if(grain==0)v.x=1;else if(grain==1)v.y=1;else v.z=1;
    const float sign=Aevrynd::Dot(Aevrynd::Cross(u,v),normal)<0?-1.f:1.f;
    const auto band=HutWornPlankBands[part%HutWornPlankBands.size()];
    return {band.x+Aevrynd::Clamp(positions[across]*sign+.5f,0.f,1.f)*(band.y-band.x),
        Aevrynd::Clamp(positions[grain]+.5f,0.f,1.f)*lengths[grain]/1.4f+.17f*float(part%5u)};
}
inline Aevrynd::Mesh3D CreateLegacyRangerHut() {
    using namespace Aevrynd;
    Mesh3D result;
    const auto box=Mesh3D::CreateUnitBox();
    const auto solids=RangerHutSolids();
    for(std::size_t i=0;i<solids.size();++i) {
        if(i==2)continue;
        const auto transform=CreateTransformMatrix(solids[i].center-Session::rangerLog,{},solids[i].extents*2.f);
        const auto normal=CreateNormalMatrix(transform);
        const auto base=static_cast<std::uint32_t>(result.vertices.size());
        for(auto v:box.vertices) {
            const auto p=TransformPoint(transform,{v.x,v.y,v.z});
            const auto n=UnpackNormalSnorm8(v.normal);
            v.x=p.x;v.y=p.y;v.z=p.z;v.normal=PackNormalSnorm8(TransformNormal(normal,{n.x,n.y,n.z}));
            result.vertices.push_back(v);
        }
        for(auto index:box.indices)result.indices.push_back(base+index);
    }
    return result;
}
// 改善外觀仍從原有碰撞體取牆尺寸，桌子另依同一碰撞體繪製。
// 西側入口與四個窗孔不新增板材；新坡屋面只位於舊屋頂上方。
inline Aevrynd::Mesh3D CreateRangerHut(RangerHutArtSettings settings={},std::vector<RangerHutPart>* parts=nullptr) {
    using namespace Aevrynd;
    Mesh3D result;
    const auto box=Mesh3D::CreateUnitBox();
    if(parts)parts->clear();std::size_t partNumber{};
    // 只改 Combined 的可見木作；另外三個既有模式完全沿用原作者。
    const bool wallArt=settings.authoredUv&&settings.visibleTrim;
    const auto append=[&](Vec3 center,Vec3 size,Color tint,Vec3 rotation=Vec3{},RangerHutPartKind kind=RangerHutPartKind::Host) {
        // 五個原薄內襯保持全部 raw；其餘可見木料明確擁有新色／UV，不宣稱舊68份全raw。
        const bool weathered=wallArt&&!(kind==RangerHutPartKind::Host&&(size.x==.036f||size.z==.036f));
        if(weathered) {
            switch(kind) {
            case RangerHutPartKind::WallBoard:tint={.96f,.95f,.92f,1};break;
            case RangerHutPartKind::Jamb:tint={.90f,.89f,.85f,1};break;
            case RangerHutPartKind::Sill:case RangerHutPartKind::UpperFrame:tint={.86f,.85f,.80f,1};break;
            case RangerHutPartKind::EntryFrame:tint={.80f,.79f,.74f,1};break;
            case RangerHutPartKind::Roof:tint={.76f,.75f,.71f,1};break;
            case RangerHutPartKind::Fascia:tint={.88f,.87f,.82f,1};break;
            default:tint={.70f,.69f,.65f,1};break;
            }
        }
        if(wallArt&&(kind==RangerHutPartKind::WallBoard||kind==RangerHutPartKind::Jamb)) {
            const auto firstVertex=result.vertices.size(),firstIndex=result.indices.size();
            // 木板與窗框均無旋轉；共用倒角保留原部件全尺寸，不擴張建築輪廓。
            if(!AppendBeveledBox3D(result,center,size,.006f,tint))
                throw std::runtime_error("Ranger hut wall bevel failed");
            for(std::size_t index=firstVertex;index<result.vertices.size();++index) {
                auto& v=result.vertices[index];const auto n=UnpackNormalSnorm8(v.normal);
                const Vec3 local{(v.x-center.x)/size.x,(v.y-center.y)/size.y,(v.z-center.z)/size.z};
                const auto uv=RangerHutWornPlankUv(local,{n.x,n.y,n.z},size,partNumber);v.u=uv.x;v.v=uv.y;
            }
            if(parts)parts->push_back({kind,firstIndex,result.indices.size()-firstIndex,center});
            ++partNumber;return;
        }
        const auto matrix=CreateTransformMatrix(center,rotation,size);
        const auto normals=CreateNormalMatrix(matrix);
        const auto base=static_cast<std::uint32_t>(result.vertices.size());
        const auto firstIndex=result.indices.size();
        for(auto v:box.vertices) {
            const Vec3 local{v.x,v.y,v.z};
            const auto p=TransformPoint(matrix,{v.x,v.y,v.z});
            const auto n=UnpackNormalSnorm8(v.normal);
            v.x=p.x;v.y=p.y;v.z=p.z;
            v.normal=PackNormalSnorm8(TransformNormal(normals,{n.x,n.y,n.z}));
            v.color=PackColorRgba8(tint);
            if(settings.authoredUv){const auto uv=weathered?RangerHutWornPlankUv(local,{n.x,n.y,n.z},size,partNumber):RangerHutFaceUv(local,{n.x,n.y,n.z},size,partNumber);v.u=uv.x;v.v=uv.y;}
            result.vertices.push_back(v);
        }
        for(auto index:box.indices)result.indices.push_back(base+index);
        if(parts)parts->push_back({kind,firstIndex,result.indices.size()-firstIndex,center});
        ++partNumber;
    };
    const auto solids=RangerHutSolids();
    append(solids[0].center-Session::rangerLog,solids[0].extents*2.f,{.62f,.59f,.52f,1});
    for(std::size_t i=1;i<solids.size();++i) {
        if(i==2)continue;
        const auto center=solids[i].center-Session::rangerLog;
        const auto size=solids[i].extents*2.f;
        if(size.x<.4f&&size.z<.4f) {
            auto postSize=size;if(settings.visibleTrim)postSize.z-=.06f;
            append(center,postSize,{.56f,.5f,.42f,1});continue;
        }
        const unsigned rows=static_cast<unsigned>(std::round(size.y/.2f));
        const float height=size.y/float(rows);
        // 薄內襯封閉板縫，但外層板材仍有真正凹槽；不把牆變成可穿越的孔。
        auto back=center,backSize=size;
        if(size.x<size.z){back.x-=.078f;backSize.x=.036f;}
        else {back.z-=std::copysign(.078f,back.z);backSize.z=.036f;}
        append(back,backSize,{.38f,.35f,.3f,1});
        if(wallArt) {
            // 30–40cm 的實際橫板，12mm 陰影接縫；深處沿用同一內襯，不穿窗或入口。
            const auto boardPartBegin=partNumber;
            const unsigned wallRows=static_cast<unsigned>(std::round(size.y/.34f));
            const float wallHeight=size.y/float(wallRows);
            const bool alongZ=size.x<size.z;
            const float span=alongZ?size.z:size.x;
            const unsigned pieces=span>3.f?3u:1u;
            for(unsigned row=0;row<wallRows;++row) {
                const float stagger=row%2u==0?.18f:-.18f;
                for(unsigned piece=0;piece<pieces;++piece) {
                    const float low=-span*.5f+span*float(piece)/float(pieces)+(piece>0?stagger:0.f);
                    const float high=-span*.5f+span*float(piece+1u)/float(pieces)+(piece+1u<pieces?stagger:0.f);
                    auto board=center,boardSize=size;
                    board.y=center.y-size.y*.5f+wallHeight*(float(row)+.5f);boardSize.y=wallHeight-.012f;
                    if(alongZ){board.z+=(low+high)*.5f;boardSize.z=high-low-.012f;boardSize.x-=.06f;}
                    else {board.x+=(low+high)*.5f;boardSize.x=high-low-.012f;boardSize.z-=.06f;}
                    const float shade=.69f+.035f*float((row+unsigned(i)+piece)%5);
                    append(board,boardSize,{shade,shade*.92f,shade*.8f,1},{},RangerHutPartKind::WallBoard);
                }
            }
            // 虛擬原部件序號承接，三舊模式仍原樣；Combined 僅五薄內襯沿用原 atlas/raw。
            partNumber=boardPartBegin+std::size_t(rows)*pieces;
            continue;
        }
        for(unsigned row=0;row<rows;++row) {
            auto p=center,s=size;
            p.y=center.y-size.y*.5f+height*(float(row)+.5f);s.y=height-.006f;
            // 長板拆成三塊實際板件，接頭交錯且由內襯封閉；避免整面牆成為單一長方塊。
            const bool alongZ=size.x<size.z;
            const float span=alongZ?size.z:size.x;
            const unsigned pieces=span>3.f?3u:1u;
            const float stagger=(row%2u==0?.18f:-.18f);
            for(unsigned piece=0;piece<pieces;++piece) {
                const float low=-span*.5f+span*float(piece)/float(pieces)+(piece>0?stagger:0.f);
                const float high=-span*.5f+span*float(piece+1u)/float(pieces)+(piece+1u<pieces?stagger:0.f);
                auto board=p,boardSize=s;
                if(alongZ){board.z+=(low+high)*.5f;boardSize.z=high-low-.004f;}
                else {board.x+=(low+high)*.5f;boardSize.x=high-low-.004f;}
                // 表面單側縮入 3cm，與原內襯仍交疊；原框面在 collider 內成為最前景。
                if(settings.visibleTrim){if(alongZ)boardSize.x-=.06f;else boardSize.z-=.06f;}
                const float shade=.69f+.035f*float((row+unsigned(i)+piece)%5);
                append(board,boardSize,{shade,shade*.92f,shade*.8f,1});
            }
        }
    }
    // 西側開放入口的立框只落在既有南北牆邊；橫框高於原入口淨空。
    for(const float end:{-1.f,1.f})
        append({-2.4f,1.3f,end*2.91f},{.16f,2.6f,.15f},{.48f,.40f,.30f,1},{},RangerHutPartKind::EntryFrame);
    append({-2.4f,2.60f,0},{.16f,.09f,5.6f},{.52f,.44f,.33f,1},{},RangerHutPartKind::EntryFrame);
    // 四個窗框均位於原立柱／上下牆帶內，窗孔 0.9–2.2 公尺完全保留。
    for(const float end:{-1.f,1.f}) {
        for(const float x:{-2.235f,-.075f,.075f,2.235f})
            append({x,1.55f,end*2.958f},{.12f,1.42f,.072f},{.51f,.43f,.32f,1},{},RangerHutPartKind::Jamb);
        for(const float x:{-1.15f,1.15f}) {
            append({x,.858f,end*2.951f},{2.10f,.074f,.096f},{.56f,.47f,.34f,1},{},RangerHutPartKind::Sill);
            append({x,2.24f,end*2.958f},{2.10f,.065f,.072f},{.51f,.43f,.32f,1},{},RangerHutPartKind::UpperFrame);
        }
    }
    // 雙坡屋面保持原屋頂平面腳印，新屋面最低點高於 2.85 公尺。
    const float angle=std::atan(.85f/2.48f),length=std::hypot(2.48f,.85f);
    for(const float side:{-1.f,1.f})
        append({side*(1.24f-std::sin(angle)*.04f),3.335f-std::cos(angle)*.04f,0},
            {length,.012f,5.988f},{.29f,.27f,.23f,1},{0,0,-side*angle},RangerHutPartKind::Roof);
    for(const float side:{-1.f,1.f})for(unsigned strip=0;strip<12;++strip) {
        const float z=-2.75f+.5f*float(strip);
        const float shade=.39f+.018f*float(strip%3);
        append({side*1.24f,3.335f,z},{length,.1f,.494f},{shade,shade*.96f,shade*.89f,1},{0,0,-side*angle},RangerHutPartKind::Roof);
    }
    append({0,3.78f,0},{.16f,.12f,5.994f},{.29f,.28f,.25f,1},{},RangerHutPartKind::Roof);
    const auto triangle=[&](Vec3 a,Vec3 b,Vec3 c) {
        const auto base=static_cast<std::uint32_t>(result.vertices.size());
        const auto normal=PackNormalSnorm8(Normalize(Cross(b-a,c-a)));
        const Vec3 low{(std::min)({a.x,b.x,c.x}),(std::min)({a.y,b.y,c.y}),(std::min)({a.z,b.z,c.z})};
        const Vec3 high{(std::max)({a.x,b.x,c.x}),(std::max)({a.y,b.y,c.y}),(std::max)({a.z,b.z,c.z})};
        const auto centre=(low+high)*.5f,size=high-low;
        const auto unpacked=UnpackNormalSnorm8(normal);const Vec3 n{unpacked.x,unpacked.y,unpacked.z};
        const auto firstIndex=result.indices.size();
        for(const auto p:{a,b,c}) {
            Vec2 uv{p.x*.2f,p.y*.2f};
            if(settings.authoredUv){const Vec3 local{size.x>0?(p.x-centre.x)/size.x:0,size.y>0?(p.y-centre.y)/size.y:0,size.z>0?(p.z-centre.z)/size.z:0};uv=wallArt?RangerHutWornPlankUv(local,n,size,partNumber):RangerHutFaceUv(local,n,size,partNumber);}
            result.vertices.push_back({p.x,p.y,p.z,normal,PackColorRgba8(wallArt?Color{.92f,.91f,.87f,1}:Color{.59f,.54f,.46f,1}),uv.x,uv.y});
        }
        result.indices.insert(result.indices.end(),{base,base+1,base+2});
        if(parts)parts->push_back({RangerHutPartKind::Gable,firstIndex,3,centre});++partNumber;
    };
    for(const float side:{-1.f,1.f}) {
        // 山牆與屋面使用同一坡度；底帶略微交疊，避免檐口留下亮縫。
        append({0,2.88f,side*2.945f},{4.96f,.065f,.07f},{.59f,.54f,.46f,1});
        const Vec3 a{-2.48f,2.91f,side*2.98f},b{2.48f,2.91f,side*2.98f},c{0,3.76f,side*2.98f};
        const Vec3 d{-2.48f,2.91f,side*2.91f},e{2.48f,2.91f,side*2.91f},f{0,3.76f,side*2.91f};
        if(side>0){triangle(a,b,c);triangle(d,f,e);}
        else {triangle(a,c,b);triangle(d,e,f);}
        for(const auto face:std::array<std::array<Vec3,4>,3>{{{a,d,e,b},{a,c,f,d},{b,e,f,c}}}) {
            if(side>0){triangle(face[0],face[1],face[2]);triangle(face[0],face[2],face[3]);}
            else {triangle(face[0],face[2],face[1]);triangle(face[0],face[3],face[2]);}
        }
    }
    if(wallArt) {
        // 四根完整外角木接到原柱與上下牆帶；末尾追加，不改任何原部件或UV相位。
        // 80mm真木料與20mm上下搭接不是射線容差，端點直接取原來源邊界。
        const auto corner=[&](Vec3 low,Vec3 high) {
            const auto center=(low+high)*.5f,size=high-low;
            const auto base=static_cast<std::uint32_t>(result.vertices.size());
            const auto firstIndex=result.indices.size();
            for(auto v:box.vertices) {
                const Vec3 local{v.x,v.y,v.z};const auto n=UnpackNormalSnorm8(v.normal);
                // 不用center±half二次捨入；明確端點封閉整個結構角隙。
                v.x=local.x<0?low.x:high.x;v.y=local.y<0?low.y:high.y;v.z=local.z<0?low.z:high.z;
                v.color=PackColorRgba8({.74f,.73f,.68f,1});
                const auto uv=RangerHutWornPlankUv(local,{n.x,n.y,n.z},size,partNumber);v.u=uv.x;v.v=uv.y;
                result.vertices.push_back(v);
            }
            for(const auto index:box.indices)result.indices.push_back(base+index);
            if(parts)parts->push_back({RangerHutPartKind::CornerStile,firstIndex,36,center});++partNumber;
        };
        for(const std::size_t lowerIndex:{3u,8u}) {
            const auto lower=solids[lowerIndex].center-Session::rangerLog;
            const auto upper=solids[lowerIndex+1].center-Session::rangerLog;
            const float bottom=lower.y+solids[lowerIndex].extents.y-.02f;
            const float top=upper.y-solids[lowerIndex+1].extents.y+.02f;
            for(const unsigned end:{0u,1u}) {
                const auto postIndex=lowerIndex+(end==0?2u:4u);
                const float z=solids[postIndex].center.z-Session::rangerLog.z;
                const float halfDepth=(solids[postIndex].extents.z*2.f-.06f)*.5f;
                const float outer=lower.x+(end==0?-solids[lowerIndex].extents.x:solids[lowerIndex].extents.x);
                const float left=end==0?outer:outer-.08f,right=end==0?outer+.08f:outer;
                corner({left,bottom,z-halfDepth},{right,top,z+halfDepth});
            }
        }
    }
    if(wallArt) {
        // 四條連續140mm簷口木料，在原±2.5×±3屋頂腳印內；不加窗內料、不增加屋簷碰撞。
        // 兩側沿Z、南北沿X，取代只靠細黑邊讀屋頂的設計；最低點2.85m不侵入原入口。
        for(const float side:{-1.f,1.f})
            append({side*2.44f,2.92f,0},{.12f,.14f,5.988f},{},{},RangerHutPartKind::Fascia);
        for(const float side:{-1.f,1.f})
            append({0,2.92f,side*2.94f},{4.88f,.14f,.08f},{},{},RangerHutPartKind::Fascia);
    }
    return result;
}
inline Aevrynd::BoxCollider3D RangerHutCameraRoofSolid() {
    // 只供鏡頭使用的保守屋面包圍盒；角色碰撞、屋內淨高及存檔不變。
    return {Session::rangerLog+Aevrynd::Vec3{0,3.35f,0},{2.5f,.5f,3.f}};
}
}
