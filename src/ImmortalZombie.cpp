#include "ImmortalZombie.h"

#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/mod/NativeMod.h>
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/service/Bedrock.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorType.h>
#include <mc/world/actor/Mob.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/level/Level.h>

#include <atomic>
#include <memory>

namespace immortal_zombie {

// ─── Pimpl ────────────────────────────────────────────────────────────────────
struct ImmortalZombie::Impl {
    std::atomic<bool>    hasZombie{false};
    std::atomic<int64_t> zombieRawId{-1}; // int64_t evita problemas con atomic<ActorUniqueID>
};

// ─── Singleton como valor (no puntero) ────────────────────────────────────────
ImmortalZombie sInstance;  // NOLINT

ImmortalZombie& ImmortalZombie::getInstance() { return sInstance; }
ImmortalZombie::ImmortalZombie()  : mImpl(std::make_unique<Impl>()) {}
ImmortalZombie::~ImmortalZombie() = default;

// NativeMod::current() es el mod activo en el thread actual — no necesitamos
// guardar un puntero manualmente.
ll::mod::NativeMod& ImmortalZombie::getSelf() const {
    return *ll::mod::NativeMod::current();
}

// ─── Gestión del zombie ────────────────────────────────────────────────────────
void    ImmortalZombie::setImmortalRawId(int64_t rawId) {
    mImpl->zombieRawId.store(rawId);
    mImpl->hasZombie.store(true);
}
void    ImmortalZombie::clearImmortalId() {
    mImpl->hasZombie.store(false);
    mImpl->zombieRawId.store(-1);
}
bool    ImmortalZombie::hasImmortal() const      { return mImpl->hasZombie.load(); }
int64_t ImmortalZombie::getImmortalRawId() const { return mImpl->zombieRawId.load(); }
bool    ImmortalZombie::isImmortal(ActorUniqueID id) const {
    return mImpl->hasZombie.load() && mImpl->zombieRawId.load() == id.rawID;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────
bool ImmortalZombie::load() {
    getSelf().getLogger().info("ImmortalZombie cargado.");
    return true;
}

bool ImmortalZombie::enable() {
    auto& mod = getSelf();
    auto& log = mod.getLogger();

    auto& cmd = ll::command::CommandRegistrar::getInstance(mod)
                    .getOrCreateCommand(
                        "immortalzombie",
                        "Controla el zombie inmortal.",
                        CommandPermissionLevel::Admin
                    );

    // /immortalzombie set ─────────────────────────────────────────────────────
    cmd.overload().text("set").execute(
        [](CommandOrigin const& origin, CommandOutput& output) {
            auto* entity = origin.getEntity();
            if (!entity || entity->getEntityTypeId() != ActorType::Player) {
                output.error("Solo jugadores pueden usar este comando.");
                return;
            }
            auto* player = static_cast<Player*>(entity); // NOLINT

            // Buscar objetivo: primero getTarget() (mob target), luego nada
            Actor* target = static_cast<Mob*>(player)->getTarget(); // NOLINT

            if (!target) {
                output.error("Ataca a un zombie primero para seleccionarlo.");
                return;
            }

            ActorType type = target->getEntityTypeId();
            bool isZombie  = (type == ActorType::Zombie)
                          || (type == ActorType::ZombieVillager)
                          || (type == ActorType::Husk)
                          || (type == ActorType::Drowned);

            if (!isZombie) {
                output.error("El objetivo no es un zombie.");
                return;
            }

            int64_t rawId = target->getOrCreateUniqueID().rawID;
            target->setPersistent();
            ImmortalZombie::getInstance().setImmortalRawId(rawId);
            output.success("Zombie {} ahora es INMORTAL.", rawId);
            ImmortalZombie::getInstance().getSelf().getLogger()
                .info("Zombie inmortal registrado: {}", rawId);
        }
    );

    // /immortalzombie clear ───────────────────────────────────────────────────
    cmd.overload().text("clear").execute(
        [](CommandOrigin const&, CommandOutput& output) {
            auto& mod = ImmortalZombie::getInstance();
            if (!mod.hasImmortal()) {
                output.error("No hay zombie inmortal activo.");
                return;
            }
            int64_t id = mod.getImmortalRawId();
            mod.clearImmortalId();
            output.success("Zombie {} ya no es inmortal.", id);
        }
    );

    // /immortalzombie info ────────────────────────────────────────────────────
    cmd.overload().text("info").execute(
        [](CommandOrigin const&, CommandOutput& output) {
            auto& mod = ImmortalZombie::getInstance();
            if (!mod.hasImmortal()) {
                output.success("No hay zombie inmortal activo.");
                return;
            }
            int64_t rawId = mod.getImmortalRawId();
            ActorUniqueID uid;
            uid.rawID = rawId;

            if (auto levelRef = ll::service::getLevel()) {
                Actor* actor = levelRef->fetchEntity(uid, false);
                if (actor) {
                    auto pos = actor->getPosition();
                    output.success(
                        "Zombie {} — pos ({:.1f},{:.1f},{:.1f})",
                        rawId, pos.x, pos.y, pos.z
                    );
                    return;
                }
            }
            output.success("Zombie {} rastreado (chunk no cargado).", rawId);
        }
    );

    log.info("ImmortalZombie activado. Ataca un zombie y usa /immortalzombie set.");
    return true;
}

bool ImmortalZombie::disable() {
    getSelf().getLogger().info("ImmortalZombie desactivado.");
    return true;
}

bool ImmortalZombie::unload() {
    getSelf().getLogger().info("ImmortalZombie descargado.");
    return true;
}

} // namespace immortal_zombie

LL_REGISTER_MOD(immortal_zombie::ImmortalZombie, immortal_zombie::sInstance);
