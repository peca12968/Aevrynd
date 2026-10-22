#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Aevrynd {
struct ClosedAabb3D {std::array<double,3> minimum{},maximum{};};
enum class StaticAabbQueryStatus3D {Invalid,Disjoint,Overlapping};
struct StaticAabbQueryResult3D {StaticAabbQueryStatus3D status{StaticAabbQueryStatus3D::Invalid};std::size_t nodesTested{},boxesTested{};};
// 擁有不可變雙精度閉區間；合併只取min/max，不量化或縮小邊界，相觸仍算重疊。
class StaticAabbIndex3D final {
    struct Entry {ClosedAabb3D bounds;std::uint32_t ordinal{};};
    struct Node {ClosedAabb3D bounds;std::uint32_t first{},count{},left{},right{};};
    std::vector<Entry> entries_;
    std::vector<Node> nodes_;
    bool valid_{};
    static bool ValidBounds(const ClosedAabb3D& b)noexcept{
        for(unsigned a=0;a<3;++a)if(!std::isfinite(b.minimum[a])||!std::isfinite(b.maximum[a])||b.minimum[a]>b.maximum[a])return false;
        return true;
    }
    static bool Separated(const ClosedAabb3D& a,const ClosedAabb3D& b)noexcept{
        for(unsigned axis=0;axis<3;++axis)if(a.maximum[axis]<b.minimum[axis]||b.maximum[axis]<a.minimum[axis])return true;
        return false;
    }
    std::uint32_t Build(std::uint32_t first,std::uint32_t count){
        auto bounds=entries_[first].bounds;
        for(std::uint32_t i=1;i<count;++i)for(unsigned a=0;a<3;++a){
            bounds.minimum[a]=(std::min)(bounds.minimum[a],entries_[first+i].bounds.minimum[a]);
            bounds.maximum[a]=(std::max)(bounds.maximum[a],entries_[first+i].bounds.maximum[a]);
        }
        const auto id=static_cast<std::uint32_t>(nodes_.size());nodes_.push_back({bounds,first,count,0,0});
        if(count<=8)return id;
        unsigned axis{};for(unsigned a=1;a<3;++a)if(bounds.maximum[a]-bounds.minimum[a]>bounds.maximum[axis]-bounds.minimum[axis])axis=a;
        const auto middle=first+count/2;
        std::nth_element(entries_.begin()+first,entries_.begin()+middle,entries_.begin()+first+count,[axis](const Entry& a,const Entry& b){
            const double ca=a.bounds.minimum[axis]*.5+a.bounds.maximum[axis]*.5,cb=b.bounds.minimum[axis]*.5+b.bounds.maximum[axis]*.5;
            return ca<cb||(ca==cb&&a.ordinal<b.ordinal);
        });
        const auto left=Build(first,count/2),right=Build(middle,count-count/2);
        nodes_[id].left=left;nodes_[id].right=right;nodes_[id].count=0;return id;
    }
public:
    static constexpr std::size_t MaximumBounds=65536;
    explicit StaticAabbIndex3D(std::span<const ClosedAabb3D> boxes){
        if(boxes.size()>MaximumBounds)return;
        for(const auto& box:boxes)if(!ValidBounds(box))return;
        entries_.reserve(boxes.size());nodes_.reserve(boxes.size()*2);
        for(std::size_t i=0;i<boxes.size();++i)entries_.push_back({boxes[i],static_cast<std::uint32_t>(i)});
        if(!entries_.empty())Build(0,static_cast<std::uint32_t>(entries_.size()));valid_=true;
    }
    [[nodiscard]] bool Valid()const noexcept{return valid_;}
    [[nodiscard]] std::size_t Size()const noexcept{return entries_.size();}
    [[nodiscard]] std::size_t Nodes()const noexcept{return nodes_.size();}
    // 回傳完整命中的原始來源序號；來源容量與每次工作區容量分開，不放寬MaximumBounds。
    // scratch可被覆寫；失敗時output/count/statistics保持原狀，不截斷成假的完整結果。
    // 與既有Query共用同一棵索引，閉區間相觸仍保留，序號按原始來源排序。
    [[nodiscard]] bool QueryOrdinals(const ClosedAabb3D& query,std::span<std::uint32_t> scratch,
        std::span<std::uint32_t> output,std::size_t& count,StaticAabbQueryResult3D* statistics=nullptr)const noexcept{
        if(!valid_||!ValidBounds(query))return false;
        if(!scratch.empty()&&!output.empty()){
            const auto a=reinterpret_cast<std::uintptr_t>(scratch.data()),b=reinterpret_cast<std::uintptr_t>(output.data());
            if(a<=b?b-a<scratch.size_bytes():a-b<output.size_bytes())return false;
        }
        StaticAabbQueryResult3D result;result.status=StaticAabbQueryStatus3D::Disjoint;
        const auto capacity=(std::min)(scratch.size(),output.size());std::size_t found{};
        if(!nodes_.empty()){
            std::array<std::uint32_t,64> stack{};std::size_t size=1;
            while(size){const auto& node=nodes_[stack[--size]];++result.nodesTested;
                if(Separated(query,node.bounds))continue;
                if(node.count){for(std::uint32_t i=0;i<node.count;++i){const auto& entry=entries_[node.first+i];++result.boxesTested;
                    if(!Separated(query,entry.bounds)){if(found==capacity)return false;scratch[found++]=entry.ordinal;}}
                }else{
                    if(size+2>stack.size())return false;
                    stack[size++]=node.right;stack[size++]=node.left;
                }
            }
        }
        if(found>1)std::sort(scratch.begin(),scratch.begin()+found);
        if(found)std::copy_n(scratch.begin(),found,output.begin());
        count=found;result.status=found?StaticAabbQueryStatus3D::Overlapping:StaticAabbQueryStatus3D::Disjoint;
        if(statistics)*statistics=result;return true;
    }
    [[nodiscard]] StaticAabbQueryResult3D Query(const ClosedAabb3D& query)const noexcept{
        StaticAabbQueryResult3D result;
        if(!valid_||!ValidBounds(query))return result;
        result.status=StaticAabbQueryStatus3D::Disjoint;if(nodes_.empty())return result;
        // 中位數二分且最多65536葉，深度有界；溢位仍回Invalid，絕不當作可通行。
        std::array<std::uint32_t,64> stack{};std::size_t size=1;
        while(size){const auto& node=nodes_[stack[--size]];++result.nodesTested;if(Separated(query,node.bounds))continue;
            if(node.count){for(std::uint32_t i=0;i<node.count;++i){++result.boxesTested;
                if(!Separated(query,entries_[node.first+i].bounds)){result.status=StaticAabbQueryStatus3D::Overlapping;return result;}}
            }else{
                if(size+2>stack.size()){result.status=StaticAabbQueryStatus3D::Invalid;return result;}
                stack[size++]=node.right;stack[size++]=node.left;
            }
        }
        return result;
    }
};
}
