#pragma once

#include <ll/api/mod/NativeMod.h>
#include <mc/world/actor/Actor.h>

#include <atomic>
#include <cstdint>

namespace immortal_zombie {

class ImmortalZombie {
public:
    // Constructor público — requerido por LL_REGISTER_MOD
    ImmortalZombie() = default;

    static ImmortalZombie& getInstance();

    // getSelf() usa NativeMod::current() — no necesita puntero interno
    ll::mod::NativeMod& getSelf() const {
        return ll::mod::NativeMod::current();
    }

    // Lifecycle (requeridos por el concepto Loadable)
    bool load();
    bool enable();
    bool disable();
    bool unload();

    // Zombie tracking
    void    setImmortalRawId(int64_t rawId);
    void    clearImmortalId();
    bool    hasImmortal() const;
    bool    isImmortal(ActorUniqueID id) const;
    int64_t getImmortalRawId() const;

private:
    std::atomic<bool>    mHasZombie{false};
    std::atomic<int64_t> mZombieRawId{-1};
};

} // namespace immortal_zombie
