<!-- Generated from documentation/documentation/concepts/action pool.txt (DokuWiki syntax). -->

# Action Pool

During the attack and motion [phases](round.md), units execute tasks in order of their [initiative](initiative.md).
Offensive units each have an attack action pool, while mobile units feature a movement action pool.
Offensive mobile units feature both.
With each executed operation the value is reduced. If an operation would reduce it to below 0, then that operation is delayed by one round.

All unit types have the same action capacity of 100%, but some are faster at certain operations than others.
An [artillery](../units/turrets/artillery.md), for instance, will exhaust all its attack action with a single shot, while a [cannon](../units/turrets/cannon.md) will only consume half of it.
Likewise, a [terraformer](../units/turrets/terraformer.md) mounted on a [heavy base](../units/vbases/heavy-base.md) has increased operation speed compared to one based on a [hoverbase](../units/vbases/hoverbase.md).
This may allow the unit to execute more operations during the same round.

## Attack Action

![Heavy tanks with their attack operations visualized as rings.](../../../documentation/images/attack%20action.jpg)

The remaining attack action defines how many attack operations a unit can execute during the next attack phase.
This value is reset to 100% at the beginning of each motion phase (which is ***after*** the attack phase).
Vehicle turret types exhibit different sensibility to vehicle motion, which is declared in the turret help text:

- *Can attack independently of movement action*: The vehicle's attack action is not affected by motion. The vehicle may move as fast as it can, and still attack with full capacity during the next attack phase.
- *Vehicle must remain stationary to fire*: This turret type reacts very sensibly to motion. Even the slightest motion will disallow any attack during the next attack phase.
- *default*: The vehicle will lose attack action depending on how much it moved during the previous motion phase. Moving half of the movement range will cause the subsequent attack action to drop by 50%. A turret that can usually attack twice may be able to attack once under these circumstances.

The number of attacks left at its current attack action level is visualized as rings around each unit. The more rings are solid, the more attacks the unit has left during the next attack phase.

---

[Game documentation index](../README.md)
