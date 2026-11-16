#pragma once
#include "RiverbankClass.h"
#ifdef AEVRYND_MISTWATCH_BATTLE
#include "RiverbankBattleWire.h"
#include "RiverbankFieldWire.h"
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
#include "RiverbankGuildWire.h"
#include "RiverbankTerritory.h"
#include "RiverbankExpeditionWire.h"
#include "RiverbankPartyWire.h"
#include "RiverbankGearWire.h"
#include "RiverbankTrailSuppliesWire.h"
#endif
#include <Aevrynd/ReplicationProtocol3D.h>
#include <memory>

namespace Mistwatch {
class LocalWorldClient {
public:
    enum class Status { Connecting, ChoosingClass, Joining, Ready, Reconnecting, Disconnected };
    explicit LocalWorldClient(unsigned short port);
    ~LocalWorldClient();
    LocalWorldClient(const LocalWorldClient&) = delete;
    LocalWorldClient& operator=(const LocalWorldClient&) = delete;

    Status Poll(std::int16_t moveX, std::int16_t moveZ, bool sprint = false);
    bool CanSprint() const noexcept;
    bool ChooseClass(RiverbankClass playerClass);
    RiverbankClass SelectedClass() const noexcept;
#ifdef AEVRYND_MISTWATCH_BATTLE
    bool RequestBattleAction(RiverbankAction action);
    bool CanReturnToCamp() const noexcept;
    const RiverbankBattleWire::Encounter* Battle() const noexcept;
    bool RequestFieldHunt();
    const RiverbankFieldWire::Reply* FieldHunt() const noexcept;
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
    bool CanUseGuild() const noexcept;
    bool CanUseTrailSupplies() const noexcept;
    bool RequestTrailSupplies(TrailSupplyAction action);
    const RiverbankTrailSuppliesWire::Reply* TrailSupplies() const noexcept;
    bool RequestGuild(GuildAction action, std::uint64_t target = 0, std::string_view name = {});
    const RiverbankGuildWire::Response* GuildReply() const noexcept;
    bool RequestTerritory();
    const TerritoryState* Territory() const noexcept;
    bool RequestGear(GearAction action);
    const RiverbankGearWire::Reply* Gear() const noexcept;
    bool RequestParty(PartyAction action, std::uint32_t target = 0);
    const RiverbankPartyWire::Reply* Party() const noexcept;
    bool RequestExpedition(bool interact);
    const RiverbankExpeditionWire::Reply* Expedition() const noexcept;
#endif
    std::uint32_t PlayerId() const noexcept;
    const Aevrynd::ReplicationSnapshot3D* Snapshot() const noexcept;

private:
    struct State;
    std::unique_ptr<State> state_;
};
}
