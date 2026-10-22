#pragma once
#include <Aevrynd/Input.h>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>

namespace Mistwatch {
enum class FrameEdge : unsigned {Evade,Jump,Interact,Attack,Tune1,Tune2,Tune3,Save};
struct FrameEdgeBatch {
    unsigned mask{};
    Aevrynd::Vec3 dodgeDirection{};
    bool deferredAttack{};
    bool Pressed(FrameEdge edge)const noexcept{return(mask&(1u<<unsigned(edge)))!=0;}
};
// 只佇列離散邊沿；held移動永遠讀當前設備，不重播拒絕幀的dt。
class FrameInputLatch {
public:
    static constexpr unsigned Capacity=16;
    bool Observe(const Aevrynd::Input& input,Aevrynd::Vec3 dodge)noexcept{
        CameraBatch camera;if(!PrepareCamera(input,camera))return false;
        FrameEdgeBatch batch;batch.dodgeDirection=dodge;
        constexpr std::array<unsigned,8> keys{'Q',VK_SPACE,'E','F','1','2','3',VK_F5};
        for(unsigned i=0;i<keys.size();++i)if(input.Pressed(keys[i]))batch.mask|=1u<<i;
        if(batch.mask&&count_==Capacity){overflow_=true;unacceptedMask_=batch.mask;return false;}
        ApplyCamera(camera);
        if(batch.mask){queue_[(head_+count_)%Capacity]=batch;++count_;}
        return true;
    }
    bool ObserveCamera(const Aevrynd::Input& input)noexcept{
        CameraBatch camera;if(!PrepareCamera(input,camera))return false;
        ApplyCamera(camera);return true;
    }
    FrameEdgeBatch Front()const noexcept{return count_?queue_[head_]:FrameEdgeBatch{};}
    void CommitWorld(bool retainAttack=false)noexcept{
        if(count_){auto& front=queue_[head_];
            if(retainAttack&&front.Pressed(FrameEdge::Attack)&&!front.deferredAttack){
                // 只留下原邊沿的一次後續機會，不重播同批互動、閃避或其他操作。
                front.mask=1u<<unsigned(FrameEdge::Attack);front.deferredAttack=true;
            }else{head_=(head_+1)%Capacity;--count_;}
        }
        CommitCamera();
    }
    void CommitCamera()noexcept{dragX_=dragY_=wheel_=0;recenter_=false;}
    void Clear()noexcept{head_=count_=unacceptedMask_=0;overflow_=false;CommitCamera();}
    bool Overflowed()const noexcept{return overflow_;}
    unsigned Count()const noexcept{return count_;}
    unsigned UnacceptedMask()const noexcept{return unacceptedMask_;}
    Aevrynd::Vec2 Drag()const noexcept{return {float(dragX_),float(dragY_)};}
    float Wheel()const noexcept{return float(wheel_);}
    bool RecenterRequested()const noexcept{return recenter_;}
private:
    struct CameraBatch{double x{},y{},wheel{};bool recenter{};};
    bool PrepareCamera(const Aevrynd::Input& input,CameraBatch& batch)noexcept{
        if(overflow_)return false;
        const auto drag=input.LeftMouseDragDelta();const auto wheel=input.MouseWheel();
        batch.x=dragX_+drag.x;batch.y=dragY_+drag.y;batch.wheel=wheel_+wheel;
        if(!std::isfinite(batch.x)||!std::isfinite(batch.y)||!std::isfinite(batch.wheel)||
            std::abs(batch.x)>(std::numeric_limits<float>::max)()||std::abs(batch.y)>(std::numeric_limits<float>::max)()||
            std::abs(batch.wheel)>(std::numeric_limits<float>::max)()){
            overflow_=true;return false;
        }
        // 手動操作取消回正意圖，包含同時按住左右而淨轉向為零的情形。
        const bool manual=drag.x!=0||drag.y!=0||input.Down(VK_LEFT)||input.Down(VK_RIGHT);
        batch.recenter=!manual&&(recenter_||input.Pressed('C'));return true;
    }
    void ApplyCamera(const CameraBatch& batch)noexcept{
        dragX_=batch.x;dragY_=batch.y;wheel_=batch.wheel;recenter_=batch.recenter;
    }
    std::array<FrameEdgeBatch,Capacity> queue_{};
    unsigned head_{},count_{},unacceptedMask_{};bool overflow_{};
    double dragX_{},dragY_{},wheel_{};bool recenter_{};
};
}
