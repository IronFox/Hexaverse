<!-- Generated from documentation/documentation/concepts/idle.txt (DokuWiki syntax). -->

# Idle Units

Different requirements apply for a unit to be considered *idle*:

- A mobile unit has run out of tasks and is awake.
- A factory has run out of production tasks, is awake, and can be [enabled](compound.md).
- A mine is currently disabled, is awake, and can be [enabled](compound.md).
- A [nexus port](../units/structures/nexus-port.md) is owned, disabled, awake, and can be [enabled](compound.md).

Units can be [put to sleep](../controls.md) to avoid them appearing in the idle queue.

During [command phase](round.md) the ***space key*** can be used to go through the list of idle units. This feature is very useful to not miss units without jobs.
If no unit is left in the idle queue, the [end round button](../controls.md) will flash.
If [Auto End Round](../controls.md) is active, and no disrupting actions have happened during the last round, the player round will be ended automatically.

---

[Game documentation index](../README.md)
