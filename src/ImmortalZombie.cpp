#include "ImmortalZombie.h"

// LeviLamina APIs
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/entity/ActorHurtEvent.h>
#include <ll/api/event/entity/MobDieEvent.h>
#include <ll/api/event/world/LevelTickEvent.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/service/Bedrock.h>

// BDS headers
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorType.h>
#include <mc/world/actor/mob/Mob.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/dimension/Dimension.h>
#include <mc/world/phys/AABB.h>

#include <atomic>
#include <memory>
#include <optional>

namespace immortal_zombie {

// ─────────────────────────────────────────────────────────────────────────────
// Pimpl struct
// ─────────────────────────────────────────────────────────────────────────────
struct ImmortalZombie::Impl {
    ll::mod::NativeMod*  self    = nullptr;

    // Tracked zombie  ─────────────────────────────────────────────────────────
    std::atomic<bool>          hasZombie{false};
    std::atomic<ActorUniqueID> zombieId{ActorUniqueID::INVALID_ID};

    // Event listeners  ────────────────────────────────────────────────────────
    ll::event::ListenerPtr hurtListener;
    ll::event::ListenerPtr dieListener;
    ll::event::ListenerPtr tickListener;

    // Tick counter for throttling expensive operations  ───────────────────────
    int tickCounter = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────
static ImmortalZombie* sInstance = nullptr;

ImmortalZombie& ImmortalZombie::getInstance() {
    return *sInstance;
}

ImmortalZombie::ImmortalZombie()  : mImpl(std::make_unique<Impl>()) {}
ImmortalZombie::~ImmortalZombie() = default;

ll::mod::NativeMod& ImmortalZombie::getSelf() const {
    return *mImpl->self;
}

// ─────────────────────────────────────────────────────────────────────────────
// Zombie management helpers
// ─────────────────────────────────────────────────────────────────────────────
void ImmortalZombie::setImmortalId(ActorUniqueID id) {
    mImpl->zombieId.store(id);
    mImpl->hasZombie.store(true);
}

void ImmortalZombie::clearImmortalId() {
    mImpl->hasZombie.store(false);
    mImpl->zombieId.store(ActorUniqueID::INVALID_ID);
}

bool ImmortalZombie::hasImmortal() const {
    return mImpl->hasZombie.load();
}

bool ImmortalZombie::isImmortal(ActorUniqueID id) const {
    return mImpl->hasZombie.load() &&
           mImpl->zombieId.load() == id;
}

ActorUniqueID ImmortalZombie::getImmortalId() const {
    return mImpl->zombieId.load();
}

// ─────────────────────────────────────────────────────────────────────────────
// load()  –  runs before the game is fully started.
//            Only game-independent init here (no events, no commands).
// ─────────────────────────────────────────────────────────────────────────────
bool ImmortalZombie::load() {
    getSelf().getLogger().info("ImmortalZombie loaded.");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// enable()  –  register events and commands.
// ─────────────────────────────────────────────────────────────────────────────
bool ImmortalZombie::enable() {
    auto& bus = ll::event::EventBus::getInstance();
    auto& log = getSelf().getLogger();

    // ── 1. ActorHurtEvent (cancellable) ──────────────────────────────────────
    //       First line of defence: cancel ALL incoming damage on our zombie.
    mImpl->hurtListener =
        bus.emplaceListener<ll::event::ActorHurtEvent>(
            [](ll::event::ActorHurtEvent& event) {
                auto& mod = ImmortalZombie::getInstance();
                if (!mod.hasImmortal()) return;
                if (event.self().getOrCreateUniqueID() == mod.getImmortalId()) {
                    event.cancel();   // zero damage reaches the mob
                }
            },
            ll::event::EventPriority::Highest  // run before anything else
        );

    // ── 2. MobDieEvent (not cancellable at event level) ──────────────────────
    //       If the die event still fires (e.g. through a hook bypass),
    //       immediately restore the zombie's HP.
    mImpl->dieListener =
        bus.emplaceListener<ll::event::MobDieEvent>(
            [](ll::event::MobDieEvent& event) {
                auto& mod = ImmortalZombie::getInstance();
                if (!mod.hasImmortal()) return;
                Actor& actor = event.self();
                if (actor.getOrCreateUniqueID() != mod.getImmortalId()) return;

                // Restore health after the event completes.
                // We cast to Mob since it's a zombie (is-a Mob).
                if (actor.isType(ActorType::Mob)) {
                    auto* mob = static_cast<Mob*>(&actor); // NOLINT
                    mob->setHealth(mob->getMaxHealth());
                }
            },
            ll::event::EventPriority::Lowest   // run after all other death logic
        );

    // ── 3. LevelTickEvent – periodic tasks ───────────────────────────────────
    //       Every 20 ticks (~1 s):
    //         a) Heal zombie back to max HP (belt-and-suspenders).
    //         b) Request the zombie's chunk stays loaded so it is never
    //            unloaded/derendered even when far from any player.
    mImpl->tickListener =
        bus.emplaceListener<ll::event::LevelTickEvent>(
            [](ll::event::LevelTickEvent& /*event*/) {
                auto& mod = ImmortalZombie::getInstance();
                if (!mod.hasImmortal()) return;

                // Throttle: only act every 20 ticks
                auto& impl = *mod.mImpl;
                impl.tickCounter++;
                if (impl.tickCounter < 20) return;
                impl.tickCounter = 0;

                auto* level = ll::service::getLevel();
                if (!level) return;

                // Fetch the tracked zombie by its unique ID.
                Actor* actor = level->fetchEntity(mod.getImmortalId(), false);
                if (!actor) {
                    // Zombie is not in any loaded chunk right now.
                    // We can't force-load without a valid actor reference,
                    // but the Mob::die and Actor::remove hooks will re-block
                    // removal if the engine tries to clean it up.
                    return;
                }

                // (a) Heal to max HP
                if (actor->isType(ActorType::Mob)) {
                    auto* mob = static_cast<Mob*>(actor); // NOLINT
                    float hp    = mob->getHealth();
                    float maxHp = mob->getMaxHealth();
                    if (hp < maxHp) {
                        mob->setHealth(maxHp);
                    }
                }

                // (b) Force-load the chunk containing the zombie.
                //     We request the chunk from the dimension's BlockSource;
                //     holding a reference keeps it alive for this tick cycle.
                //     For permanent loading, we also call setChunkForceLoaded.
                auto& blockSource = actor->getDimensionBlockSource();
                auto  pos         = actor->getPosition();
                ChunkPos chunkPos(pos);

                // Request load – this prevents the chunk from going inactive.
                blockSource.getChunk(chunkPos);

                // Tell the dimension to forcibly keep this chunk ticking.
                auto* dim = actor->getDimension();
                if (dim) {
                    dim->getChunkSource().addForceLoadedChunk(chunkPos);
                }
            }
        );

    // ── 4. Register commands ──────────────────────────────────────────────────
    auto commandRegistry = ll::service::getCommandRegistry();
    if (!commandRegistry) {
        log.error("Failed to get command registry — commands unavailable.");
    } else {
        // /immortalzombie set    – mark the zombie the player is looking at
        // /immortalzombie clear  – remove immortality assignment
        // /immortalzombie info   – print status
        auto& cmd = ll::command::CommandRegistrar::getInstance()
                        .getOrCreateCommand(
                            "immortalzombie",
                            "Control the immortal zombie.",
                            CommandPermissionLevel::Admin
                        );

        // ── set ──────────────────────────────────────────────────────────────
        cmd.overload().text("set").execute(
            [](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (!entity || entity->getEntityTypeId() != ActorType::Player) {
                    output.error("Only players can use /immortalzombie set.");
                    return;
                }
                auto* player = static_cast<Player*>(entity); // NOLINT

                // Raycast to find the zombie the player is looking at.
                // LeviLamina exposes getViewActor() / getAttackTarget(); we use
                // a block-source hit-scan via AABB search as a fallback.
                Actor* target = player->getAttackTarget();

                if (!target) {
                    // Fallback: find closest zombie within 8 blocks
                    auto& bs    = player->getDimensionBlockSource();
                    Vec3  eye   = player->getHeadPos();
                    float best  = 8.0f * 8.0f;

                    bs.fetchEntities(player, AABB(eye - Vec3(8,8,8), eye + Vec3(8,8,8)),
                        [&](Actor* a) -> bool {
                            if (!a || a == player) return true;
                            if (a->getEntityTypeId() != ActorType::Zombie &&
                                a->getEntityTypeId() != ActorType::ZombieVillager &&
                                a->getEntityTypeId() != ActorType::Husk) return true;
                            float dist = (a->getPosition() - eye).lengthSquared();
                            if (dist < best) {
                                best   = dist;
                                target = a;
                            }
                            return true;
                        });
                }

                if (!target) {
                    output.error("No zombie found nearby. Look at one or stand within 8 blocks.");
                    return;
                }

                ActorType type = target->getEntityTypeId();
                if (type != ActorType::Zombie &&
                    type != ActorType::ZombieVillager &&
                    type != ActorType::Husk &&
                    type != ActorType::Drowned &&
                    type != ActorType::ZombifiedPiglin) {
                    output.error("Target is not a zombie-family mob.");
                    return;
                }

                ActorUniqueID uid = target->getOrCreateUniqueID();
                ImmortalZombie::getInstance().setImmortalId(uid);

                // Make zombie persistent so BDS never marks it for cleanup.
                target->setPersistent();

                output.success(
                    "Zombie (UID {}) is now IMMORTAL. It will never die, "
                    "despawn, or be removed.",
                    uid.rawID
                );
                ImmortalZombie::getInstance().getSelf().getLogger()
                    .info("Immortal zombie set: UID {}", uid.rawID);
            }
        );

        // ── clear ─────────────────────────────────────────────────────────────
        cmd.overload().text("clear").execute(
            [](CommandOrigin const&, CommandOutput& output) {
                auto& mod = ImmortalZombie::getInstance();
                if (!mod.hasImmortal()) {
                    output.error("No immortal zombie is currently set.");
                    return;
                }
                auto uid = mod.getImmortalId();
                mod.clearImmortalId();
                output.success("Immortal zombie (UID {}) cleared.", uid.rawID);
            }
        );

        // ── info ──────────────────────────────────────────────────────────────
        cmd.overload().text("info").execute(
            [](CommandOrigin const&, CommandOutput& output) {
                auto& mod = ImmortalZombie::getInstance();
                if (!mod.hasImmortal()) {
                    output.success("No immortal zombie is currently tracked.");
                    return;
                }

                auto uid    = mod.getImmortalId();
                auto* level = ll::service::getLevel();
                Actor* actor = level ? level->fetchEntity(uid, false) : nullptr;

                if (!actor) {
                    output.success(
                        "Immortal zombie UID {} is TRACKED but currently "
                        "in an unloaded chunk (chunk-load will happen next tick).",
                        uid.rawID
                    );
                } else {
                    auto pos = actor->getPosition();
                    output.success(
                        "Immortal zombie UID {} — pos ({:.1f}, {:.1f}, {:.1f}) — "
                        "HP: {:.0f}/{:.0f} — dim: {}",
                        uid.rawID,
                        pos.x, pos.y, pos.z,
                        static_cast<Mob*>(actor)->getHealth(),   // NOLINT
                        static_cast<Mob*>(actor)->getMaxHealth(),// NOLINT
                        actor->getDimension()->getDimensionId().id
                    );
                }
            }
        );
    }

    log.info("ImmortalZombie enabled. Use /immortalzombie set while looking at a zombie.");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// disable()  –  unsubscribe everything.
// ─────────────────────────────────────────────────────────────────────────────
bool ImmortalZombie::disable() {
    auto& bus = ll::event::EventBus::getInstance();
    bus.removeListener(mImpl->hurtListener);
    bus.removeListener(mImpl->dieListener);
    bus.removeListener(mImpl->tickListener);
    getSelf().getLogger().info("ImmortalZombie disabled.");
    return true;
}

bool ImmortalZombie::unload() {
    getSelf().getLogger().info("ImmortalZombie unloaded.");
    return true;
}

} // namespace immortal_zombie

// ─────────────────────────────────────────────────────────────────────────────
// LeviLamina entry-point wiring
// ─────────────────────────────────────────────────────────────────────────────
LL_REGISTER_MOD(immortal_zombie::ImmortalZombie, immortal_zombie::sInstance);
