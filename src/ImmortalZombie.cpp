#include "ImmortalZombie.h"

#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
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

struct ImmortalZombie::Impl {
    ll::mod::NativeMod*        self = nullptr;
    std::atomic<bool>          hasZombie{false};
    std::atomic<ActorUniqueID> zombieId{ActorUniqueID::INVALID_ID};
};

static ImmortalZombie* sInstance = nullptr;

ImmortalZombie& ImmortalZombie::getInstance() { return *sInstance; }
ImmortalZombie::ImmortalZombie()  : mImpl(std::make_unique<Impl>()) {}
ImmortalZombie::~ImmortalZombie() = default;

ll::mod::NativeMod& ImmortalZombie::getSelf() const { return *mImpl->self; }

void ImmortalZombie::setImmortalId(ActorUniqueID id) {
    mImpl->zombieId.store(id);
    mImpl->hasZombie.store(true);
}
void ImmortalZombie::clearImmortalId() {
    mImpl->hasZombie.store(false);
    mImpl->zombieId.store(ActorUniqueID::INVALID_ID);
}
bool          ImmortalZombie::hasImmortal() const    { return mImpl->hasZombie.load(); }
ActorUniqueID ImmortalZombie::getImmortalId() const  { return mImpl->zombieId.load(); }
bool ImmortalZombie::isImmortal(ActorUniqueID id) const {
    return mImpl->hasZombie.load() && mImpl->zombieId.load() == id;
}

bool ImmortalZombie::load() {
    getSelf().getLogger().info("ImmortalZombie loaded.");
    return true;
}

bool ImmortalZombie::enable() {
    auto& log = getSelf().getLogger();

    auto commandRegistry = ll::service::getCommandRegistry();
    if (!commandRegistry) {
        log.error("Failed to get command registry.");
        return false;
    }

    auto& cmd = ll::command::CommandRegistrar::getInstance()
                    .getOrCreateCommand(
                        "immortalzombie",
                        "Control the immortal zombie.",
                        CommandPermissionLevel::Admin
                    );

    // /immortalzombie set
    cmd.overload().text("set").execute(
        [](CommandOrigin const& origin, CommandOutput& output) {
            auto* entity = origin.getEntity();
            if (!entity || entity->getEntityTypeId() != ActorType::Player) {
                output.error("Solo jugadores pueden usar este comando.");
                return;
            }
            auto* player = static_cast<Player*>(entity); // NOLINT
            Actor* target = player->getAttackTarget();

            if (!target) {
                output.error("Mira a un zombie o ponte a menos de 8 bloques.");
                return;
            }

            ActorType type = target->getEntityTypeId();
            if (type != ActorType::Zombie       &&
                type != ActorType::ZombieVillager &&
                type != ActorType::Husk          &&
                type != ActorType::Drowned       &&
                type != ActorType::ZombifiedPiglin) {
                output.error("El objetivo no es un zombie.");
                return;
            }

            ActorUniqueID uid = target->getOrCreateUniqueID();
            target->setPersistent();
            ImmortalZombie::getInstance().setImmortalId(uid);
            output.success("Zombie UID {} ahora es INMORTAL.", uid.rawID);
            ImmortalZombie::getInstance().getSelf().getLogger()
                .info("Inmortal zombie registrado: UID {}", uid.rawID);
        }
    );

    // /immortalzombie clear
    cmd.overload().text("clear").execute(
        [](CommandOrigin const&, CommandOutput& output) {
            auto& mod = ImmortalZombie::getInstance();
            if (!mod.hasImmortal()) {
                output.error("No hay zombie inmortal activo.");
                return;
            }
            auto uid = mod.getImmortalId();
            mod.clearImmortalId();
            output.success("Zombie UID {} ya no es inmortal.", uid.rawID);
        }
    );

    // /immortalzombie info
    cmd.overload().text("info").execute(
        [](CommandOrigin const&, CommandOutput& output) {
            auto& mod = ImmortalZombie::getInstance();
            if (!mod.hasImmortal()) {
                output.success("No hay zombie inmortal activo.");
                return;
            }
            auto  uid   = mod.getImmortalId();
            auto* level = ll::service::getLevel();
            Actor* actor = level ? level->fetchEntity(uid, false) : nullptr;
            if (!actor) {
                output.success("Zombie UID {} rastreado (chunk no cargado).", uid.rawID);
            } else {
                auto pos = actor->getPosition();
                output.success(
                    "Zombie UID {} — pos ({:.1f},{:.1f},{:.1f})",
                    uid.rawID, pos.x, pos.y, pos.z
                );
            }
        }
    );

    log.info("ImmortalZombie activado. Usa /immortalzombie set mirando un zombie.");
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
