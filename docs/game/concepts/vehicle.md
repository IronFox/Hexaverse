<!-- Generated from documentation/documentation/concepts/vehicle.txt (DokuWiki syntax). -->

# Vehicle

Vehicles are ground-based mobile units built by [vehicle plant](../units/structures/vehicle-plant.md)s. They require at least a ledge to stand on, and cannot stand on lava alone.
All regular vehicles are composed of a vehicle base and turret.
Depending on the combination, different capacities and restrictions apply.

## Weight

Turrets come with an inherent weight which must be carried by the respective vehicle base.
Overweight is the result of turret weight exceeding the carrying capacity of the used base.

For each additional point of overweight, the vehicle will lose 25% of its remaining movement speed, though it will never fall below one step per [round](round.md).
Heavy turrets, such as a [bomb](../units/turrets/bomb.md) or [surveyor](../units/turrets/surveyor.md), are thus better suited atop a [heavy](../units/vbases/heavy-base.md) or [catamaran base](../units/vbases/catamaran-base.md).

## Ships

Vehicles built on a [catamaran base](../units/vbases/catamaran-base.md) are much faster on, but also restricted to water surfaces. They are usually a good choice on planets with extensive water surfaces, but rarely anywhere else.
Naval [terraformer](../units/turrets/terraformer.md)s can dig canals through non-water terrain, but can never leave the water surface.

## Heavy Vehicles

If a [heavy base](../units/vbases/heavy-base.md) is used, then the vehicle cannot savely stand on water alone, and has reduced falling tolerance. However, analogously  to catamaran bases on water, it has increased capacities on solid terrain. Heavy vehicles benefit from flat non-liquid terrain.
Similarly to naval vehicles, [terraformer](../units/turrets/terraformer.md)s can make any terrain suitable for heavy vehicles.

## Sub Orbital Planes

Planes are technically vehicles but lack their composition flexibility, as they share the same base.
A plane's health is determined from the 'turret' health multiplier alone, and their speed equals 100 divided by their weight.

Planes can land and take off vertically, as long as no terrain is in the way.
While landed, they are immobile and visible to regular [sensory](scanning-range.md).
They can fall, however, if the ground is removed beneath them.

While airborne, they are very fast and invisible to regular sensory.
Other planes are required to detect them.

---

[Game documentation index](../README.md)
