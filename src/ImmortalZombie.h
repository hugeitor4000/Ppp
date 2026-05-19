#pragma once

#include <ll/api/mod/NativeMod.h>
#include <ll/api/event/ListenerBase.h>
#include <mc/world/actor/ActorUniqueID.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

namespace immortal_zombie {

// ─────────────────────────────────────────────────────────────────────────────
// ImmortalZombie
//   Singleton mod class.
//   Tracks one zombie by ActorUniqueID and keeps it:
//     • Immune to all damage  (ActorHurtEvent cancel + Actor::hurt hook)
//     • Immune to death       (Mob::die hook restores HP instead)
//     • Immune to removal     (Actor::remove hook skips removal)
//     • Immune to despawn     (Actor::despawn hook skips despawn)
//     • Immune to derender    (LevelTickEvent forces chunk loaded every second)
// ─────────────────────────────────────────────────────────────────────────────
class ImmortalZombie {
public:
    static ImmortalZombie& getInstance();

    ll::mod::NativeMod& getSelf() const;

    // Mod lifecycle ─────────────────────────────────────────────────────────
    bool load();
    bool enable();
    bool disable();
    bool unload();

    // Zombie management ─────────────────────────────────────────────────────
    void   setImmortalId(ActorUniqueID id);
    void   clearImmortalId();
    bool   hasImmortal() const;
    bool   isImmortal(ActorUniqueID id) const;
    ActorUniqueID getImmortalId() const;   // only valid when hasImmortal()

private:
    ImmortalZombie();
    ~ImmortalZombie();

    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace immortal_zombie
