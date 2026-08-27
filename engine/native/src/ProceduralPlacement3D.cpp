#include "Aevrynd/ProceduralPlacement3D.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Aevrynd {
namespace {

struct Random3D {
    std::uint64_t state;
    explicit Random3D(std::uint32_t seed):state((std::uint64_t(seed)<<1)|1u){}
    std::uint32_t Next(){state^=state>>12;state^=state<<25;state^=state>>27;return std::uint32_t((state*2685821657736338717ull)>>32);}
    float Unit(){return float(Next()>>8)*(1.f/16777216.f);}
};

bool FiniteBounds(const PlacementBounds3D& bounds){return std::isfinite(bounds.minimum.x)&&std::isfinite(bounds.minimum.y)&&std::isfinite(bounds.minimum.z)&&std::isfinite(bounds.maximum.x)&&std::isfinite(bounds.maximum.y)&&std::isfinite(bounds.maximum.z)&&bounds.maximum.x>bounds.minimum.x&&bounds.maximum.z>bounds.minimum.z&&bounds.maximum.y>=bounds.minimum.y;}

}

ProceduralPlacementResult3D GenerateProceduralPlacement3D(const ProceduralPlacementSettings3D& settings){
    ProceduralPlacementResult3D result;
    if(!FiniteBounds(settings.bounds)||!std::isfinite(settings.minimumSpacing)||settings.minimumSpacing<=0||!std::isfinite(settings.minimumScale)||!std::isfinite(settings.maximumScale)||settings.minimumScale<=0||settings.maximumScale<settings.minimumScale||settings.attemptsPerInstance==0||settings.maximumInstances==0||settings.maximumGridCells==0)return result;
    const double width=double(settings.bounds.maximum.x)-settings.bounds.minimum.x,depth=double(settings.bounds.maximum.z)-settings.bounds.minimum.z,area=width*depth;
    if(!std::isfinite(area)||area<=0||(!std::isfinite(settings.densityPerSquareUnit)&&settings.targetCount==0)||settings.densityPerSquareUnit<0)return result;
    std::size_t target=settings.targetCount;if(target==0){const double requested=area*double(settings.densityPerSquareUnit);if(!std::isfinite(requested)||requested>double(settings.maximumInstances))return result;target=std::size_t(requested+.5);}
    if(target==0)return result;if(target>settings.maximumInstances||target>std::size_t((std::numeric_limits<std::int32_t>::max)())||target>(std::numeric_limits<std::size_t>::max)()/settings.attemptsPerInstance)return result;
    constexpr double inverseSqrtTwo=.7071067811865475244;const double cellSize=double(settings.minimumSpacing)*inverseSqrtTwo;
    const double columnsValue=std::ceil(width/cellSize),rowsValue=std::ceil(depth/cellSize);if(columnsValue<1||rowsValue<1||columnsValue>double(settings.maximumGridCells)||rowsValue>double(settings.maximumGridCells))return result;
    const std::size_t columns=std::size_t(columnsValue),rows=std::size_t(rowsValue);if(columns>(std::numeric_limits<std::size_t>::max)()/rows||columns*rows>settings.maximumGridCells)return result;
    const std::size_t attemptLimit=target*settings.attemptsPerInstance;std::vector<std::int32_t> heads(columns*rows,-1),next;next.reserve(target);result.instances.reserve(target);Random3D random(settings.seed);const double minimumDistanceSquared=double(settings.minimumSpacing)*settings.minimumSpacing;
    while(result.instances.size()<target&&result.attempts<attemptLimit){++result.attempts;const float x=settings.bounds.minimum.x+float(width)*random.Unit(),z=settings.bounds.minimum.z+float(depth)*random.Unit();bool accepted=true;
        for(const PlacementExclusion3D& exclusion:settings.exclusions){if(!std::isfinite(exclusion.center.x)||!std::isfinite(exclusion.center.z)||!std::isfinite(exclusion.radius)||exclusion.radius<0)return {};const double dx=double(x)-exclusion.center.x,dz=double(z)-exclusion.center.z;if(dx*dx+dz*dz<double(exclusion.radius)*exclusion.radius){accepted=false;break;}}
        float y=settings.bounds.minimum.y;if(accepted&&settings.heightCallback&&!settings.heightCallback(x,z,y,settings.heightContext))accepted=false;if(accepted&&(!std::isfinite(y)||y<settings.bounds.minimum.y||y>settings.bounds.maximum.y))accepted=false;if(!accepted)continue;
        const std::size_t cellX=(std::min)(columns-1,std::size_t((double(x)-settings.bounds.minimum.x)/cellSize)),cellZ=(std::min)(rows-1,std::size_t((double(z)-settings.bounds.minimum.z)/cellSize));const std::size_t minX=cellX>2?cellX-2:0,maxX=(std::min)(columns-1,cellX+2),minZ=cellZ>2?cellZ-2:0,maxZ=(std::min)(rows-1,cellZ+2);
        for(std::size_t neighborZ=minZ;accepted&&neighborZ<=maxZ;++neighborZ)for(std::size_t neighborX=minX;accepted&&neighborX<=maxX;++neighborX)for(std::int32_t index=heads[neighborZ*columns+neighborX];index>=0;index=next[std::size_t(index)]){const Vec3 point=result.instances[std::size_t(index)].position;const double dx=double(x)-point.x,dz=double(z)-point.z;if(dx*dx+dz*dz<minimumDistanceSquared)accepted=false;}
        if(!accepted)continue;const float scale=settings.minimumScale+(settings.maximumScale-settings.minimumScale)*random.Unit(),yaw=random.Unit()*6.2831853071795864769f;const std::size_t gridIndex=cellZ*columns+cellX;next.push_back(heads[gridIndex]);heads[gridIndex]=std::int32_t(result.instances.size());result.instances.push_back({{x,y,z},scale,yaw});
    }
    result.exhausted=result.instances.size()<target;return result;
}

}
