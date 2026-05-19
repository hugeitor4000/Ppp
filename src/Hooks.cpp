//
// Hooks.cpp  —  versión limpia compatible con levilamina 26.10.x
//
// Solo hookeamos funciones que EXISTEN en esta versión:
//   1. Mob::die      → restaura HP, cancela muerte
//   2. Actor::remove → bloquea eliminación del mundo
//   3. Actor::despawn → bloquea despawn natural
//
// El daño ya está bloqueado por ActorHurtEvent (cancellable) en ImmortalZombie.cpp
//

#include "ImmortalZombie.h"

#include <ll/api/memory/Hook.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorDamageSource.h>
#include <mc/world/actor/Mob.h>

using namespace immortal_zombie;

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 1 — Mob::die
//   Si el motor llega a ejecutar die() (por /kill, void, etc.),
//   restauramos HP y salimos sin llamar a origin().
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalDieHook,
    ll::memory::HookPriority::Highest,
    Mob,
    &Mob::die,
    void,
    ActorDamageSource const& source
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        setHealth(getMaxHealth());
        return; // NO llamamos origin() → muerte cancelada
    }
    origin(source);
}

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 2 — Actor::remove
//   Bloquea cualquier intento de eliminar el zombie del mundo.
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalRemoveHook,
    ll::memory::HookPriority::Highest,
    Actor,
    &Actor::remove,
    void
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        setPersistent(); // forzar persistencia cada vez que algo intente eliminarlo
        return;
    }
    origin();
}

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 3 — Actor::despawn
//   Bloquea el despawn natural por distancia/tiempo.
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalDespawnHook,
    ll::memory::HookPriority::Highest,
    Actor,
    &Actor::despawn,
    void
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        return;
    }
    origin();
}
