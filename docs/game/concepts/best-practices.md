<!-- Generated from documentation/documentation/concepts/best practices.txt (DokuWiki syntax). -->

# Best Practices

Effective tactics will be listed here, as they become common place.
Different terrain and unit constellations will require adjustments, particularly if there is no core, or the planet is covered by lava, but some tactics have generally been shown to be more effective than others.

Even so, these are not fixed rules (even if they sound like it).
If the situation warrants it, unconventional approaches may give you an edge.
When you are reading this, however, you are probably new to the game, and should start by following these tactics until you have a deeper understanding of why they work (most of the time).

## Early Game

This is when you lack pretty much everything, particularly ![Time](../../../documentation/images/Time.png).
You're living off your ![Minerals](../../../documentation/images/Minerals.png) reserves, have little knowledge of the surrounding [natural resources](resources.md),
lack ![Energy](../../../documentation/images/Energy.png) and ![Fuel](../../../documentation/images/Fuel.png), and [see virtually nothing of the terrain](scanning-range.md).
Many matches are decided during this stage.

### Crucial Tactics

- Start building a [vehicle plant](../units/structures/vehicle-plant.md) early, ideally in round 0.
   - => Alternate between building [terraformer](../units/turrets/terraformer.md)s and [cannon](../units/turrets/cannon.md)s. You will need both.
- Your initial [generator](../units/structures/generator.md) can supply only one more active building (luckily the vehicle plant doesn't need ![Energy](../../../documentation/images/Energy.png), but additional [mines](../units/structures/mine.md) will). Start building one or two additional generators, ideally surrounding a single clear cell of same height, to eventually place a [reactor core](../units/structures/reactor-core.md) there.
- Build **one** [laboratory](../units/structures/laboratory.md) such that you can later place at least two more next to it to form a triangle. Due to the proximity bonus of labs, a triangle is the smallest benefitial constellation, but building more than one at this stage is usually a dangerous waste of resources.
   - => [Research](research.md) [Launchers](../units/turrets/missile-launcher.md), [Reactor Cores](../units/structures/reactor-core.md), [Heavy](../units/vbases/heavy-base.md) or [Cat Bases](../units/vbases/catamaran-base.md)(depending on planet type), and finally the [Artillery Turret](../units/turrets/artillery.md). Different scenarios may necessitate a different order.
- Build defense towers early, possibly to the point where you have one [terraformer](../units/turrets/terraformer.md) dedicated to this task from the start. Observe the range of your turrets. Make sure defense towers can [see](scanning-range.md) where they can [hit](attack-range.md).
- Build lots of [mines](../units/structures/mine.md), but do not risk your [terraformers](../units/turrets/terraformer.md) (and/or mines) by leaving them alone in [unobserved territory](scanning-range.md). A lost terraformer is a terrible toll during this stage. Distant [resource](resources.md) clusters might be too exposed/unknown to claim, even if your [surveyor](../units/turrets/surveyor.md) somehow managed to get through unscathed. Generally, if assumed reachable by your enemies, build towers or deploy tanks first, then construct the mines. Such expansion is always a gamble but you can tweak the odds.
- Tanks attack on their own if an attack is considered safe. Unless you require a particular attack pattern due to [initiative](initiative.md) or focused fire, it is often better to issue movement [commands](commands.md) only. [Cannon](../units/turrets/cannon.md) tanks will always fire at the start of a [round](round.md) if the attack on  a hostile unit is safe, regardless of whether they are on the move ([cannon](../units/turrets/cannon.md) attacks don't hinder motion, and vice versa). [Artillery](../units/turrets/artillery.md) on the other hand requires the [vehicle](vehicle.md) to remain stationary. If issueing attack commands manually, keep in mind that some (e.g. [cannon](../units/turrets/cannon.md)) tanks can fire multiple times per round.

### Situational Tactics

- [Tank trajectories](attack-range.md) benefit from elevation. Even one level of height difference compared to your opponent can make all the difference in the world. Sometimes it can be benefitial to build artificial elevations, and put your tanks on top.
- Manually navigate your [surveyor](../units/turrets/surveyor.md) if you are in proximity of assumed dangerous areas. The automatic routing can only react to things you can actually see. A red Nexus base marker is not regarded by surveyor routing.
- Build a [fuel container](../units/structures/fuel-container.md) to bridge shortage of fuel, and (temporarily) run more [generators](../units/structures/generator.md). Backup storage is generally cheap to build, and may be useful early on.
- Claim nearby mountains, and place tanks on top, even (or especially) if it requires terraforming to get up there. These can be very easy to defend, particularly once you have [artillery](../units/turrets/artillery.md) tanks.
- Dwarven mentality: Dig down early (4 layers or more), and build your entire base beneath the ground. Hard to launch an offense from, harder to find and attack. In highly competitive scenarios this tactic can be used to simply disappear early on, having others fight it out above ground while you can undermine their [mines](../units/structures/mine.md), and eventually detonate their bases from below. Note that this tactic is slow, and not useful if you have just one enemy that anticipates such. It does pretty much lock out the AI, however. Beware of [lava](../terrain.md).
- Walls (two levels or higher) represent an effective means to keep the AI out. Everything except [surveyors](../units/turrets/surveyor.md) can see about one cell behind a wall, but not farther (**Important:** this also applies to your own units). Vehicles cannot climb more than one level so they cannot get over the wall, and the AI will not consider attacking the wall itself unless it detects your units on (or directly behind) the wall. Later on, you will want to place towers on the wall. This transition can be tricky.
- [Vehicle stores](../units/structures/vehicle-store.md) enable instant vehicle transportation to remote stores. They are also cheap to build, and preserve stored units upon destruction if enough other stores remain. It is often useful to build one directly next to your [vehicle plant](../units/structures/vehicle-plant.md)(s) and set its/their gathering point to the store.
- Claim [Nexus Ports](../units/structures/nexus-port.md) beneath blue goal markers. Build next to them to conquer them. Apart from being a game goal, they also provide useful resources. If distant, assume others had the same idea. Note that until it is yours, the port is owned by the Nexus (AI) faction, and it will see your construction attempt. If the AI already has mobile units, and is not tied up elsewhere, it **will** move in your direction.
- It may not appear that way, but the [radar](../units/turrets/radar.md) vehicle is the single most dangerous unit type the AI has. Everything it discovers is memorized. A hoverrader suddenly standing in the midst of your base is a harbinger of doom. Keep them at range, actively hunt them if necessary.

### Don'ts

- Stay clear of red Nexus Base markers; at least until you have a working defense perimeter. Give them a wide berth (about the distance between any two markers/[Nexus Ports](../units/structures/nexus-port.md)).
- Do not send your [terraformers](../units/turrets/terraformer.md) alone into distant, unexplored terrain. This is a recipe for terraformer loss, and possibly disaster.
- Do not try to build planes early on. While this may appear to give you an immense edge, planes are simply too expensive to build at this stage, and will cost precious research time to even be able to build. A [terraformer](../units/turrets/terraformer.md) building a [plane factory](../units/structures/plane-factory.md) will be tied up for a long time, and not contribute to your station security.
- Do not enter [alliances](faction.md) with people you do not trust. As soon as you accept an invitation, every alliance member will know where you are. The alliance does not protect you, even if it is not immediately dissolved.
- While this may appear promising or even crucial, rushing way out of your defense perimeter into distant, possibly [unobserved territory](scanning-range.md) to claim contested [Nexus Ports](../units/structures/nexus-port.md) usually ends badly. It's always better to be the one with the tank on the nearby mountain than the one that hastily built a storage next to the port but failed to establish sufficient defense in time. Everyone can see where the Nexus Ports are. Assume your enemies expect you to go for them.

## Coverage and Positioning

These principles apply throughout the match. Fights are won or lost by defense coverage, and coverage starts with what you can actually [see](scanning-range.md).

### Three Kinds of Sensory

- **Ground** coverage comes from [radars](../units/turrets/radar.md) and, later, [sensor drones](../units/planes/sensor-drone.md). Radar is the most economical early detection, but it cannot see planes, does not penetrate the ground by more than one or two cells, and rock casts shadows into its coverage. Sensor drones mostly supersede radar later on, but they can never peer into the ground, and planes cannot look into caves.
- **Air** coverage comes from planes and [anti aircraft](../units/turrets/anti-aircraft.md). AA mostly looks up. A bomber you cannot see is extremely lethal.
- **Underground** coverage comes from [surveyor](../units/turrets/surveyor.md) (scanner) vehicles and [towers](../units/structures/surveyor-tower.md). Scanners are the only sensory without any obstruction, reaching deep into the ground, but they are directed mostly down and do not extend far horizontally. If you suspect someone might undermine you, a surveyor tower is a good investment.

### The Defense Ladder

Every layer of defense you add shifts your opponent to the next attack vector:

- A radar without [AA](../units/turrets/anti-aircraft.md) or hovering [raptors](../units/planes/raptor.md) will be attacked from the air sooner or later.
- Place AA near it, and your next problem is tanks.
- Place a [missile launcher](../units/structures/missile-launcher-tower.md) next to it, and your next problem is heavy tanks.
- Once that is covered, the next problem comes from underground.

Tanks become relatively cheap later on, so an attacker can always try to overwhelm a proper defense with more units.
Against the Nexus AI, anti aircraft is essential: it starts building planes fairly early.

### Moving and Firing

Units react differently to being moved. [Cannon](../units/turrets/cannon.md) turrets and airborne [raptors](../units/planes/raptor.md) fire independently of motion. Most other vehicles do not fire in a round in which they also moved.
A [cannon](../units/turrets/cannon.md) tank can simply be moved around, while an [artillery](../units/turrets/artillery.md) tank is best placed on a mountain and left stationary.

### Artillery on High Ground

Altitude doesn't tell you what you are up against. Look at the turret that fired: an [artillery](../units/turrets/artillery.md) tank on a high cliff shoots from far above, but it is a ground unit, and [AA](../units/turrets/anti-aircraft.md) cannot touch it.
If a hostile faction has placed artillery high up, sending more tanks there is not a good idea. They are shelled one by one on the way up. Keep your units out of its range instead, and pick a counter:

- Planes, such as a [bomber](../units/planes/bomber.md), fly over cliffs and walls. This is usually the best answer.
- Tunnels: [terraformers](../units/turrets/terraformer.md) can dig up to it from below, out of sight.
- Only if neither works, rush it with many units at once. That is a valid albeit lousy strategy, and it may not be possible at all: if the attacker has walled off the cliff, tanks cannot get up there.

### Vehicle Stores

[Vehicle stores](../units/structures/vehicle-store.md) are interconnected. They are very useful as storage for backup units that you can offload wherever you have a vehicle store. Planes can be loaded into them, too.

### Power

Power generation is most efficient when built in a circle around a [reactor core](../units/structures/reactor-core.md). Until you have the technology and the uranium for a core, build additional [generators](../units/structures/generator.md) in a circle, keeping the center free and accessible.
A faction whose energy output drops to zero is as good as dead, and very rarely recovers. Keep enough minerals to rebuild at least one generator and one [fuel mine](../units/structures/mine.md). With those, survival is still possible, for example by having your [terraformers](../units/turrets/terraformer.md) dig into the ground, close the shaft behind them, and build an underground city.

### Improvement Research

Endless improvement [research](research.md) captures whatever research capacity your unlock topics leave over, so keep some in your queue at all times. It amplifies whichever tactic you are using, and has the most impact when focused on what you use most:

- **Health** on the base that suits the planet: the [catamaran base](../units/vbases/catamaran-base.md) on water planets, the [heavy base](../units/vbases/heavy-base.md) on solid ones. Only on very mixed planets is [hoverbase](../units/vbases/hoverbase.md) health worth it.
- **Scanning range** on [radars](../units/turrets/radar.md), some plane types, or in odd cases the [scanner](../units/turrets/surveyor.md) turret.
- **Attack range** on whichever turret you use most.

### Terraforming and Constrained Space

[Heavy](../units/vbases/heavy-base.md) and [catamaran](../units/vbases/catamaran-base.md) bases terraform about twice as fast. With catamarans you can build waterways, and with heavy bases roads and tunnels go up very quickly. Both can construct submerged mines where space is constrained. This is useful on planets with floating islands: if there is water underneath, mines can be placed on it. If there is only air, mines must stay above. Mines do not fall, but non-plane vehicles do.

Roads are really effective, yet even human players seldom build them. A one-time investment in proper infrastructure makes everything a lot faster afterwards.
Keep in mind that hostile encounters tend to rip holes into the ground, so roads through contested areas do not last. Also, [terraformers](../units/turrets/terraformer.md) are priority targets for the Nexus AI, because they can be used to take over a hub. Sending one in first to claim a spot can go either way: in the best case it builds the towers to defend the spot before anything gets to it, in the worst case it does not.

### Air and Ground

Successful matches usually involve both ground and air battles. Planes fly over walls and cliffs and are invisible to regular radar, so only [AA](../units/turrets/anti-aircraft.md) can answer them. [Bombers](../units/planes/bomber.md) hit what is directly beneath them, [raptors](../units/planes/raptor.md) hunt other planes, and [sensor drones](../units/planes/sensor-drone.md) keep the map in view. A [plane factory](../units/structures/plane-factory.md) consumes fuel and minerals every round, so it needs a running economy. Even moving a mobile [AA](../units/turrets/anti-aircraft.md) vehicle right under a bomber can make sense.

### Going Underground

A base below ground is hard to find: sensors don't see through rock. One way to play it is to dig in right away, demolish the surface structures, and rebuild them underground. Sealing the entry is really important: if the shaft stays open, other factions just walk right in. Seal it fast: if it takes you 35 rounds, you're toast. Dig a small first chamber, get the terraformers down, seal, and widen the cavern behind the seal. The strategy works as long as hostile [surveyors](../units/turrets/surveyor.md) don't survive long: they look into the ground and give the base away, so shoot them first. Against a single enemy the outcome is open: it depends on what happens sooner, surface artillery breaking through or bombs destroying the other's base. Mines can be undermined (placed beneath a surface mine), Nexus hubs cannot.
Successful underground operations need [surveyors](../units/turrets/surveyor.md) and [terraformers](../units/turrets/terraformer.md) working together: the surveyor looks ahead into the ground, the terraformer digs. Lava is a real issue if you run into it.
Dig large, roughly spherical caverns, sometimes two layers high. In narrow tunnels a parked vehicle blocks the way: stationary vehicles count as obstacles for route finding, moving ones don't.
Building underground takes more planning than on the surface. Rather than building wherever there is room, plan each structure: have [terraformers](../units/turrets/terraformer.md) carve out the space, then build into it, and make sure the terraformer can still get out. How fast the base grows depends on how well you organize your terraformers.
Bombs are the only weapon you really have underground: there is no room to use a tank's range. [Bomb](../units/turrets/bomb.md) vehicles it is. Towers below ground (cannon or better) only matter once someone has broken in, and by then you are lost anyway. Towers are also two layers high: to use them underground, build them submerged. Since the Nexus AI does not dig tunnels, subterranean warfare is never necessary against it.

### Plantations and T2 Units

Plantations need some space around them to grow plants and crystals. Without earth or granite/lava, both [compound](compound.md) types can be manufactured and combined into the material needed to elevate units to T2.
T2 upgrades are done by [mechanic](../units/turrets/mechanic.md) units. T2 units are tie-breakers: they gain experience from destroying enemy units and can become so powerful that a single one can decimate hostile installations without any doomsday weaponry.

### Judgement Calls

There is no "always do this when that". Every decision is a judgement call: how badly do you need planes right now, how badly do you need that mine, how badly do you need that Nexus hub? The same move can be right in one match and fatal in the next.
There are signs that you weighed something wrong. If a bomber blows up your towers, your weight for AA was clearly too low. Less obviously, if your enemy has no AA and you build no planes, your weight for planes was too low.
Any weakness in a defense is a fatal vulnerability, and every failure to exploit one can decide the match.
How much of something you already have matters: zero makes it far more significant than some. One of something in the right spot is always better than zero, but a second is only better if the location or the action can use it. Tanks hinder each other, towers block access, and everything competes for space. The winning faction usually picked the right amount of assets in the right spot. Mass tank rushes are a lot harder in this game, but numbers still give you a blanket of meat shields and sensor coverage.
There is also a trade-off in how close together you keep things. An AA tower next to a radar tower next to a missile tower can be very effective against most threats, but an elevated [artillery](../units/turrets/artillery.md) tank that can shoot at one of them will likely hit all three and wipe them out in a couple of rounds. Spread out once artillery is in play.
Check how exposed a location is. There is a pretty wide radius around mountains where a tower is almost always a bad idea, because artillery on the mountain rains down death. The tricky part is that an adversary can build high ground: if they build a large ramp and move an artillery tank up there, you have a problem. Watch hostile terraformers near your base.
This gives each location a strategic value. A location in a valley with no natural resources is worthless. A mountain peak can be game deciding. A valley with lots of natural resources is much more complicated: you might want to conquer all the mountains around it, or build your mines down in the ground where nobody can outright see them. A mountain deep inside your fortified region is probably (but not guaranteed) less relevant to defend. A [vehicle store](../units/structures/vehicle-store.md) on top holds it with no standing garrison: if anything comes close, offload an artillery tank or three and you are good.
Maintaining sensor coverage even of occupied terrain is important: if you cannot see part of your land, you cannot be sure nothing slipped through. In player-versus-player matches, a ring of [surveyor towers](../units/structures/surveyor-tower.md) along your border makes sure nothing drills in. With proper research, their range can rival a radar's.
Late game, [sensor drones](../units/planes/sensor-drone.md) are easily the best way to keep large land masses covered. They can also see planes, so they are arguably better than a normal radar. Both radar and AA are limited to their own layer: to get the same sensor coverage you have to place them in pairs, which you would only do if you cannot build sufficient drones.
Fortifying with towers is the usual way to hold a location, but a mobile complement can secure it too. Two mobile [AA](../units/turrets/anti-aircraft.md) vehicles can leapfrog: one moves while the other stays put and covers it, since a vehicle that must stand still to fire cannot shoot in a round in which it moves.

## Mid to End Game

Usually, when you get here, you have a decent tower network, [vehicle plants](../units/structures/vehicle-plant.md), lots of [mines](../units/structures/mine.md), and start wondering what to build with that [terraformer](../units/turrets/terraformer.md) that just completed its jobs.
Usually you have a good idea of where your enemies are, your defense/offense is somewhat balanced, and the battle fronts are semi-stable.

### Tactics

- Put units to [sleep](idle.md) that you don't need right now, so you can focus on units that really need your attention (use the space key a lot). If available, put mobile units into [vehicle stores](../units/structures/vehicle-store.md) so you can deploy them instantly at any other store.
- Make sure your air defense is solid (place enough [anti aircraft](../units/turrets/anti-aircraft.md) vehicles/towers).
- Build a [plane factory](../units/structures/plane-factory.md) or two.
   - Try to gain air-superiority by deploying [raptors](../units/planes/raptor.md).
   - Observe large areas by placing a carpet of [sensor drones](../units/planes/sensor-drone.md).
   - ![Uranium](../../../documentation/images/Uranium.png)300 (depending on configuration) enable nuclear air-to-ground attacks. If you fight above ground (and even if you don't), try to carve out a path to deploy [bombers](../units/planes/bomber.md) above hostile terrain.
   - Planes can be spawned, or landed into [vehicle stores](../units/structures/vehicle-store.md). This way, they can be deployed in remote locations on demand.
- Use [surveyor](../units/turrets/surveyor.md) vehicles and towers to make sure nobody undermines your base.
- If you intend to do so with your enemies, now is a good time to start digging, and researching the [bomb](../units/turrets/bomb.md) turret. Dig deep (5 levels+), so regular hostile units can't see you. Use [surveyors](../units/turrets/surveyor.md) to scan into the ground. Beware of enemy surveyors.
- Start accumulating ![Uranium](../../../documentation/images/Uranium.png) and ![Fuel](../../../documentation/images/Fuel.png) in [uranium storages](../units/structures/uranium-storage.md) and [fuel containers](../units/structures/fuel-container.md). Both are prone to explode if destroyed while non-empty, so either place them away from your core structures, or such that they are near impossible to attack.
- If you have sufficient ![Uranium](../../../documentation/images/Uranium.png), [EMP launchers](../units/turrets/emp.md) can be very effective against enemy radars and launchers.
- Build roads and bridges to connect your empire. [Vehicle stores](../units/structures/vehicle-store.md) may get blown to bits. [Cargo planes](../units/planes/cargo-plane.md) can also be used to quickly transport mobile units.
- Build at least some [heavy](../units/vbases/heavy-base.md)/[cat](../units/vbases/catamaran-base.md) [terraformers](../units/turrets/terraformer.md) as they build/dig twice as fast.
- Have a standing [heavy](../units/vbases/heavy-base.md)/[cat](../units/vbases/catamaran-base.md) [artillery](../units/turrets/artillery.md)+[cannon](../units/turrets/cannon.md) (+[radar](../units/turrets/radar.md)) army, even if it isn't needed right now. [Cannon tanks](../units/turrets/cannon.md) are the most effective hunters, and tend to not live long, so build most of those. Store them in [vehicle stores](../units/structures/vehicle-store.md).
- Multiple [vehicle plants](../units/structures/vehicle-plant.md) are a good thing to have right now.
- Construct large areas filled with [laboratories](../units/structures/laboratory.md). Once done with unlock-research, configure the four most important endless topics (usually [radar](../units/turrets/radar.md)/[artillery](../units/turrets/artillery.md)/[missile launcher](../units/turrets/missile-launcher.md) range, or [heavy](../units/vbases/heavy-base.md)/[cat](../units/vbases/catamaran-base.md) health).
- Claim as many mountains as you can by putting [artillery](../units/turrets/artillery.md)+[vehicle store](../units/structures/vehicle-store.md)+[AA](../units/turrets/anti-aircraft.md) on top. If you drill caves into them from a well protected side, those places are easily accessible (by you), well protected, and hard to observe from the outside. For perfect security add a couple [sensor drones](../units/planes/sensor-drone.md), and have a [raptor](../units/planes/raptor.md) on stand-by.
- To crack such a location there are a number of (bad) options:
   - [Research](research.md) [heavy base](../units/vbases/heavy-base.md) or [hoverbase](../units/vbases/hoverbase.md) health to rediculous levels, then rush a larger number of [cannon](../units/turrets/cannon.md) tanks up the mountain, possibly in secluded paths constructed by a [terraformer](../units/turrets/terraformer.md). The mountain peak can hold a limited number of tanks, each with a limited number of attacks per round. Also, the sensory of the defending party down to the valley may be partially blocked by the mountain itself. If the defending party has [bombers](../units/planes/bomber.md) on stand-by, this is a lot harder, possibly requiring mobile [AA](../units/turrets/anti-aircraft.md) or [raptors](../units/planes/raptor.md).
   - Try to position an [EMP](../units/turrets/emp.md) tank such that the defending party cannot see it, but the top of the mountain is in attack range. Fire blindly. Good luck.
   - If the mountain is high enough to be in near-horizontal range of [raptors](../units/planes/raptor.md), and the defending party cannot prevent the approach via their own, your raptor(s) can easily outgun even stationary [AA towers](../units/structures/anti-aircraft-tower.md).
   - Dig a tunnel, then [nuke](../units/turrets/bomb.md) the mountain.
   - Try to determine the range of enemy [AA](../units/turrets/anti-aircraft.md), possibly via [sensor drones](../units/planes/sensor-drone.md), then position a [bomber](../units/planes/bomber.md) just out of range of it. Nuke the mountain. If the defending party notices this, most likely the bomber will be attacked by a [raptor](../units/planes/raptor.md) before it can fire. Otherwise, expect the mountain to be gone alongside your bomber.
   - All of the above.

---

[Game documentation index](../README.md)
