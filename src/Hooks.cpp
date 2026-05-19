//
// Hooks.cpp
//   Hooks de bajo nivel que interceptan funciones internas de BDS
//   para garantizar la inmortalidad total del zombie rastreado.
//
//   Capas de protección:
//     1. Actor::hurt        → devuelve false (sin daño)
//     2. Mob::die           → restaura HP en vez de matar
//     3. Actor::remove      → bloquea la eliminación del mundo
//     4. Actor::despawn     → bloquea el despawn natural
//     5. Actor::onRemoved   → bloquea limpieza post-muerte
//

#include "ImmortalZombie.h"

#include <ll/api/memory/Hook.h>

#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorDamageSource.h>
#include <mc/world/actor/mob/Mob.h>

using namespace immortal_zombie;

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 1 — Actor::hurt
//   Primera barrera: antes de que cualquier daño se compute,
//   devolvemos false para el zombie inmortal.
//   Cubre fuego, explosiones, void, flechas, magia, todo.
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalHurtHook,
    ll::memory::HookPriority::Highest,
    Actor,
    &Actor::hurt,
    bool,
    ActorDamageSource const& source,
    float                    damage,
    bool                     knock,
    bool                     ignite
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        // Absorb todo el daño silenciosamente
        return false;
    }
    return origin(source, damage, knock, ignite);
}

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 2 — Mob::die
//   Segunda barrera: si por cualquier razón el motor llama a die()
//   (daño de servidor, comandos /kill, etc.), restauramos HP y
//   cancelamos la secuencia de muerte.
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
        // Restaurar HP máxima y NO llamar a origin() → la muerte no ocurre
        setHealth(getMaxHealth());
        return;
    }
    origin(source);
}

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 3 — Actor::remove
//   Tercera barrera: impide que el motor elimine la entidad del mundo.
//   Cubre: desconexión de chunk, /kill que ya superó Mob::die,
//   y cualquier ruta de code que llame remove() directamente.
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalRemoveHook,
    ll::memory::HookPriority::Highest,
    Actor,
    &Actor::remove,
    void
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        // No eliminamos al zombie inmortal; sólo aseguramos que siga persistente
        setPersistent();
        return;
    }
    origin();
}

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 4 — Actor::despawn
//   Cuarta barrera: el motor llama despawn() para limpieza natural
//   cuando el mob lleva mucho tiempo lejos de jugadores.
//   Para nuestro zombie lo ignoramos completamente.
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalDespawnHook,
    ll::memory::HookPriority::Highest,
    Actor,
    &Actor::despawn,
    void
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        return; // bloquear despawn
    }
    origin();
}

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 5 — Actor::onRemoved
//   Quinta barrera: callback de post-eliminación.
//   Si el engine llegó hasta aquí con nuestro zombie, lo reintroducimos
//   en el nivel en el siguiente tick (vía setPersistent + heal).
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalOnRemovedHook,
    ll::memory::HookPriority::Highest,
    Actor,
    &Actor::onRemoved,
    void
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        // No ejecutar lógica de limpieza; asegurar persistencia
        setPersistent();
        if (isType(ActorType::Mob)) {
            auto* mob = static_cast<Mob*>(this); // NOLINT
            mob->setHealth(mob->getMaxHealth());
        }
        return;
    }
    origin();
}

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 6 — Actor::shouldDespawn (si existe como virtual)
//   Fuerza que el zombie nunca sea candidato al despawn natural.
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalShouldDespawnHook,
    ll::memory::HookPriority::Highest,
    Actor,
    &Actor::shouldDespawn,
    bool
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        return false; // NUNCA despawn
    }
    return origin();
}

// ─────────────────────────────────────────────────────────────────────────────
// HOOK 7 — Actor::isRemoved
//   Si algo consulta si el actor fue eliminado, devolvemos false
//   para que el sistema lo siga tratando como vivo.
// ─────────────────────────────────────────────────────────────────────────────
LL_AUTO_TYPE_INSTANCE_HOOK(
    ImmortalIsRemovedHook,
    ll::memory::HookPriority::Highest,
    Actor,
    &Actor::isRemoved,
    bool
) {
    if (ImmortalZombie::getInstance().isImmortal(getOrCreateUniqueID())) {
        return false; // siempre "no eliminado"
    }
    return origin();
}
