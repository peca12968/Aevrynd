#pragma once
#include "MistwatchLoadPreparation.h"
#include "MistwatchPairContact.h"

namespace Mistwatch {
enum class PairLoadPreflightReason {
    Accepted, PairDisabled, NoLiveNpc, InvalidSnapshot, PlayerSnapshotMismatch,
    InvalidPlayerVolume, QueryUnresolved, LiveNpcOverlap
};
struct PairLoadPreflightResult {
    PairLoadPreflightReason reason{PairLoadPreflightReason::InvalidSnapshot};
    explicit operator bool()const noexcept {
        return reason==PairLoadPreflightReason::Accepted||reason==PairLoadPreflightReason::PairDisabled||
            reason==PairLoadPreflightReason::NoLiveNpc;
    }
};

// 僅附加於成功 PrepareSavedGame 的候選；F5 寫檔前及 F9 交換前共用。
// 不移動存檔、不推進遭遇戰，也不清除 live／候選的預覽權杖、輸入或相機。
inline PairLoadPreflightResult ValidatePreparedPairLoad(const PreparedSavedGame& prepared,bool enabled=true)noexcept {
    if(!enabled)return {PairLoadPreflightReason::PairDisabled};
    const auto& snapshot=prepared.snapshot;
    if(!snapshot.Valid())return {PairLoadPreflightReason::InvalidSnapshot};
    // 沿用原載入契約的數值相等；合法舊檔的正負零不可新增為讀檔失敗。
    if(!LoadPreparationDetail::Same(prepared.controller.Position(),snapshot.session.feet))
        return {PairLoadPreflightReason::PlayerSnapshotMismatch};
    const PairContact::PlayerBox player{prepared.controller.Center(),prepared.controller.Extents()};
    if(!PairContact::ValidBox(player))return {PairLoadPreflightReason::InvalidPlayerVolume};
    const auto body=PairContact::LiveBody(snapshot.session,snapshot.encounter);
    // 合法敗敵當下即不再有活體；不把其舊姿態或上一幀暫存的 body 當障礙。
    if(!body)return {PairLoadPreflightReason::NoLiveNpc};
    Aevrynd::BoxCylinderContact3D contact;
    if(!Aevrynd::QueryBoxCylinderContact3D(player.center,player.extents,{},*body,contact))
        return {PairLoadPreflightReason::QueryUnresolved};
    // 精確邊界接觸與真分離由同一正式窄相位分類；不另加容差或方盒替代。
    if(contact.kind!=Aevrynd::BoxCylinderContactKind3D::Clear)
        return {PairLoadPreflightReason::LiveNpcOverlap};
    return {PairLoadPreflightReason::Accepted};
}
}
