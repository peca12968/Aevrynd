#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace Mistwatch {
// 只有設備調整鍵1／2／3，位元0／1／2；回傳值亦為本tick應調整的位元遮罩。
// 確認、攻擊、閃避不屬於此API；呼叫者失焦、暫停、離站或載入時須Reset或傳context=0。
class TuningInputRepeat {
public:
    void Reset() noexcept {
        context_=0;
        armedMask_=0;
        remaining_.fill(0);
    }

    unsigned Tick(double dt,std::uint64_t context,unsigned pressedMask,unsigned downMask) noexcept {
        // 非法輸入整批清除；不截斷dt或遮掉非法鍵位，避免留下幽靈連發。
        if(!std::isfinite(dt)||dt<0||dt>.1||((pressedMask|downMask)&~7u)!=0||context==0){
            Reset();return 0;
        }
        if(context!=context_){Reset();context_=context;}
        unsigned emitted{};
        for(unsigned key=0;key<3;++key){
            const unsigned bit=1u<<key;
            if((pressedMask&bit)!=0){
                // 新按立即執行，包括同tick已放開；只有仍按住才啟用後續連發。
                emitted|=bit;
                if((downMask&bit)!=0){armedMask_|=bit;remaining_[key]=.35L;}
                else Clear(key);
                // 新按發生前的dt不能算入本鍵的長按時間。
                continue;
            }
            if((downMask&bit)==0){Clear(key);continue;}
            // 只看到Down、沒有本context內的新Pressed，不可自動啟用連發。
            if((armedMask_&bit)==0)continue;
            const auto step=static_cast<long double>(dt);
            if(step<remaining_[key])remaining_[key]-=step;
            else {
                emitted|=bit;
                // dt<=.1<.12，一tick最多跨一次門；保留餘數而不補發burst。
                remaining_[key]=.12L-(step-remaining_[key]);
            }
        }
        return emitted;
    }

    unsigned ArmedMask() const noexcept{return armedMask_;}
    // key為0／1／2；未啟用或索引越界回傳0。僅供診斷，不提供外部改寫狀態。
    long double Remaining(unsigned key) const noexcept {
        return key<3&&(armedMask_&(1u<<key))!=0?remaining_[key]:0;
    }

private:
    void Clear(unsigned key) noexcept {armedMask_&=~(1u<<key);remaining_[key]=0;}
    std::array<long double,3> remaining_{};
    std::uint64_t context_{};
    unsigned armedMask_{};
};
}
