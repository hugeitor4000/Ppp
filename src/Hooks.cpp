#include "ImmortalZombie.h"

#include <ll/api/memory/Hook.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorDamageSource.h>
#include <mc/world/actor/Mob.h>

using namespace immortal_zombie;

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 1 — Mob::$die  (virtual → prefijo $)
//   Cancela la muerte. El tick en ImmortalZombie.cpp se encarga de sanar.
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalDieHook,
    ll::memory::HookPriority::Highest,
    Mob,
    &Mob::$die,
    void,
    ActorDamageSource const& source
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        return; // cancelar muerte — el tick loop restaura HP
    }
    origin(source);
}

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 2 — Actor::$remove  (virtual → prefijo $)
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalRemoveHook,
    ll::memory::HookPriority::Highest,
    Actor,
    &Actor::$remove,
    void
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        setPersistent();
        return;
    }
    origin();
}

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 3 — Actor::$despawn  (virtual → prefijo $)
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalDespawnHook,
    ll::memory::HookPriority::Highest,
    Actor,
    &Actor::$despawn,
    void
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        return;
    }
    origin();
}
