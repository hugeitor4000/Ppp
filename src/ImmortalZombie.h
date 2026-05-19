#pragma once

#include <ll/api/mod/NativeMod.h>
#include <ll/api/event/ListenerBase.h>
#include <mc/world/actor/Actor.h>

#include <atomic>
#include <memory>

namespace immortal_zombie {

class ImmortalZombie {
public:
    static ImmortalZombie& getInstance();

    ll::mod::NativeMod& getSelf() const;

    bool load();
    bool enable();
    bool disable();
    bool unload();

    void setImmortalId(ActorUniqueID id);
    void clearImmortalId();
    bool hasImmortal() const;
    bool isImmortal(ActorUniqueID id) const;
    ActorUniqueID getImmortalId() const;

private:
    ImmortalZombie();
    ~ImmortalZombie();

    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

} // namespace immortal_zombie
