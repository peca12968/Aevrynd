#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace Aevrynd::FeasibleCircleArcs2D {
inline constexpr double Pi=3.1415926535897932384626433832795,Tau=2*Pi;
inline constexpr std::size_t MaximumBlockers=2048,MaximumIntervals=4096;
inline constexpr unsigned MaximumProposals=24000,MaximumOperations=250000;
// radius 已包括原植被間距，不包括新候選冠幅；不得改成只算樹幹。
struct Disc {double x{},z{},radius{};std::uint32_t id{};};
struct Interval {double begin{},end{};};
enum class CandidateKind {Midpoint,Begin,End};
struct Candidate {double x{},z{},angle{};std::size_t arcIndex{};CandidateKind kind{};};
struct Settings {double candidateRadius{},positiveMargin{.005};unsigned maximumProposals{MaximumProposals};};
struct Result {
    std::vector<Interval> blockedIntervals,freeArcs;
    std::vector<Candidate> candidates;
    unsigned discChecks{},operations{},proposals{};
    bool truncated{};
};
inline void Validate(const Disc& d) {
    if(!std::isfinite(d.x)||!std::isfinite(d.z)||!std::isfinite(d.radius)||
       std::abs(d.x)>10000||std::abs(d.z)>10000||d.radius<0||d.radius>10000)
        throw std::invalid_argument("ARC_DISC_INVALID");
}
inline double NormalizeAngle(double a) noexcept {
    const double r=std::fmod(a,Tau);return r<0?r+Tau:r;
}
inline Result Compute(const Disc& anchor,std::span<const Disc> blockers,Settings settings) {
    Validate(anchor);
    if(!std::isfinite(settings.candidateRadius)||settings.candidateRadius<=0||settings.candidateRadius>1000||
       !std::isfinite(settings.positiveMargin)||settings.positiveMargin<=0||settings.positiveMargin>1||
       settings.maximumProposals>MaximumProposals)
        throw std::invalid_argument("ARC_SETTINGS_INVALID");
    if(blockers.size()>MaximumBlockers)throw std::invalid_argument("ARC_BLOCKER_LIMIT");
    Result result;
    const auto op=[&]{if(++result.operations>MaximumOperations)throw std::runtime_error("ARC_OPERATION_LIMIT");};
    // 先檢查全部輸入，不能因前方已全遮蔽便吞掉後方非法資料。
    for(const auto& b:blockers){op();Validate(b);}
    const double radius=anchor.radius+settings.candidateRadius+settings.positiveMargin;
    std::vector<Interval> intervals;intervals.reserve(blockers.size()*2);
    bool fullyBlocked=false;
    for(const auto& b:blockers){
        op();++result.discChecks;
        const double dx=b.x-anchor.x,dz=b.z-anchor.z,d=std::hypot(dx,dz);
        const double expanded=b.radius+settings.candidateRadius+settings.positiveMargin;
        // 全包含含孤立內切點採保守全遮蔽；零長度外切不占弧長。
        if(expanded>=d+radius){fullyBlocked=true;continue;}
        if(d>=radius+expanded||radius>=d+expanded)continue;
        if(d==0)continue; // 同心且未全包含已由前面的包含關係處理。
        const double cosine=((d-expanded)*(d+expanded)+radius*radius)/(2*d*radius);
        if(cosine<=-1){fullyBlocked=true;continue;}
        if(cosine>=1)continue;
        const double half=std::acos(cosine),centre=NormalizeAngle(std::atan2(dz,dx));
        const double begin=centre-half,end=centre+half;
        if(begin<0){intervals.push_back({0,end});intervals.push_back({begin+Tau,Tau});}
        else if(end>Tau){intervals.push_back({begin,Tau});intervals.push_back({0,end-Tau});}
        else intervals.push_back({begin,end});
        if(intervals.size()>MaximumIntervals)throw std::runtime_error("ARC_INTERVAL_LIMIT");
    }
    if(fullyBlocked){result.blockedIntervals.push_back({0,Tau});return result;}
    std::sort(intervals.begin(),intervals.end(),[&](const Interval& a,const Interval& b){
        op();return a.begin!=b.begin?a.begin<b.begin:a.end<b.end;
    });
    for(const auto& interval:intervals){
        op();
        if(result.blockedIntervals.empty()||interval.begin>result.blockedIntervals.back().end)
            result.blockedIntervals.push_back(interval);
        else result.blockedIntervals.back().end=(std::max)(result.blockedIntervals.back().end,interval.end);
    }
    double cursor=0;
    for(const auto& interval:result.blockedIntervals){
        op();if(cursor<interval.begin)result.freeArcs.push_back({cursor,interval.begin});cursor=interval.end;
    }
    if(cursor<Tau)result.freeArcs.push_back({cursor,Tau});
    const auto propose=[&](double angle,std::size_t arcIndex,CandidateKind kind){
        op();
        if(result.proposals==settings.maximumProposals){result.truncated=true;return;}
        // 實際 root 一經構造即消耗名額，這裡不執行世界驗收、更不刪除失敗候選。
        ++result.proposals;angle=NormalizeAngle(angle);
        const double x=anchor.x+radius*std::cos(angle),z=anchor.z+radius*std::sin(angle);
        if(!std::isfinite(x)||!std::isfinite(z))throw std::runtime_error("ARC_ROOT_NONFINITE");
        result.candidates.push_back({x,z,angle,arcIndex,kind});
    };
    for(std::size_t i=0;i<result.freeArcs.size();++i){const auto& arc=result.freeArcs[i];
        propose((arc.begin+arc.end)*.5,i,CandidateKind::Midpoint);
        propose(arc.begin,i,CandidateKind::Begin);
        // 完整一圈的 2pi 與 0 是同一個解析端點，不重複構造。
        if(!(arc.begin==0&&arc.end==Tau))propose(arc.end,i,CandidateKind::End);
    }
    return result;
}
}
