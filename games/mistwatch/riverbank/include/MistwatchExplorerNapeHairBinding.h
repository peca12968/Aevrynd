#pragma once

namespace Mistwatch {
struct Explorer;
namespace ExplorerNapeHairBinding {
// 正式玩家上傳前僅呼叫一次；拒絕非原來源，不修改姿態或渲染資源。
void Apply(Explorer& explorer);
}
}
