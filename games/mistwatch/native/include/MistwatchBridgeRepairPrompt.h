#pragma once
#include "MistwatchSession.h"
namespace Mistwatch {
// 這不是碰撞通行證；型別刻意沒有 Ready 或 CommitAllowed 狀態。
enum class BridgeRepairIntent { None, AttemptRequiresAuthoritativeCheck };
inline BridgeRepairIntent EvaluateBridgeRepairIntent(const Session& current) {
    if(!current.Valid()||!current.Alive()||current.CurrentStage()!=Stage::Bridge)return BridgeRepairIntent::None;
    auto sessionPreview=current;
    return sessionPreview.RepairBridge()?BridgeRepairIntent::AttemptRequiresAuthoritativeCheck:BridgeRepairIntent::None;
}
// 每次從目前 Session 取值，無世界／profile／terrain／讀檔／鏡頭證據可跨幀沿用。
// 真正的修後世界重建與相機碰撞證明仍只在正式 E 事件執行，成功後原子提交。
}
