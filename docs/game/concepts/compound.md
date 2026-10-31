<!-- Generated from documentation/documentation/concepts/compound.txt (DokuWiki syntax). -->

# Compound

![Disconnected Compounds](../../../documentation/images/isolated%20compounds.jpg)

Neighboring structures of at most one height layer of difference form compounds.
Within a compound, working structures auto-balance [resource](resources.md) supply and demand. Passive structures may function as relays between distant structures.
Select any structure of a compound to see its resource balance in the bottom left screen corner.
Its detailed resource supply and demand is listed on the unit panel in the right screen section.

In all current variances of Hexaverse all structures of one [faction](faction.md) form an implicit compound regardless of physical connection.
In older versions or custom builds [connectors](../units/structures/connector.md) are required to connect distant structures.

## Enabling Structures

Many structure types can be enabled or disabled.
Depending on the state of their compound, it may be impossible to enable a structure without first disabling some other structure.
In particular, if a structure consumes limited resources, it may block other structures in the same compound from being enabled.

The game tries to balance supply and demand automatically.
If, for instance, insufficient ![Energy](../../../documentation/images/Energy.png) energy is available to explicitly enable a certain structure, but [generators](../units/structures/generator.md) could be enabled, then it will do so automatically.
This, in turn, might enable ![Fuel](../../../documentation/images/Fuel.png) fuel [mines](../units/structures/mine.md) that have not been running at this point.

Rapid change (such as [connectors](../units/structures/connector.md) being destroyed) will cause the minimal number of structures being disabled to balance out [resource](resources.md) supply and demand.

---

[Game documentation index](../README.md)
