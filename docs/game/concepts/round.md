<!-- Generated from documentation/documentation/concepts/round.txt (DokuWiki syntax). -->

# Rounds

Hexaverse is round based. During each round a number of phases are processed until the next round begins.
All players (and AI clients) experience each phase simultaneously. As such all players may issue [commands](commands.md) to their units at the same time.

## Phases

1. **Command Phase**: During this phase all players add or remove unit [commands](commands.md). With few exceptions, commands are not executed immediately. Once done, all connected players are expected to request a new round by clicking the [end round button](../controls.md) (or by pressing the return key). As long as at least one connected player has not yet requested a new round, it is possible to resume an ended round by clicking this button again. This phase is typically [configured](../config/server.md) to have a maximum time window. During the first round, and after the disconnection of the last player, any timer is halted until a new round is explicitly requested by connected clients.
1. **Structure Phase**: Implicit computation phase. Executed the moment a new round is computed. [Resources](resources.md) are distributed across [compounds](compound.md) and productions advance.
1. **Attack Phase**: [Attack commands](commands.md) are executed in order of their respective units' [initiative](initiative.md). Offensive units may fill and execute their attack queue automatically if hostile targets are in range, and an attack is considered safe. This phase ends when all units have exhausted their command lists or [attack action](action-pool.md).
1. **Motion Phase**: All non-attack [commands](commands.md) are issued during this phase. This includes standard motion as well as vehicle operations such as terraforming, construction, or [repairing](../units/turrets/mechanic.md) units. Both motion and queued operations are executed in the order of the respective unit's [initiative](initiative.md). Towers may automatically fire at trespassing hostile units during this phase. This phase ends when all units have exhausted their command lists or [movement action](action-pool.md).
1. **Animation Phase**: While the server waits, changes generated during the previous phases are properly animated by all clients. Independent events are visualized in parallel, although the server does not actually execute that way. The animation phase may last at most 10 seconds, and will be accellerated if necessary.

At the end of the animation phase, the next command phase starts.

---

[Game documentation index](../README.md)
