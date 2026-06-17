# New Game Concept — Brainstorm Document
*Working title: TBD*
*Status: Early ideation. Expand this into a full design doc, then we adapt the build.*

---

## Elevator Pitch (Draft)

A fantasy roguelike where the dungeon goes both ways.
Players descend through a procedurally generated world, looting and fighting.
Between runs — or as a parallel goal — they build and defend their own base
inside a claimed dungeon room. Other players (or NPC raiders) can enter it.
An elaborate crafting and alchemy system lets players make unique weapons,
potions, traps, and tools rather than just finding them.

---

## Core Pillars (As Stated)

- Roguelike dungeon crawl with a fantasy setting
- Player-owned bases ("dungeons") with workstations
- Elaborate crafting system
- Trait-based alchemy (ingredient combination with buffs, debuffs, cancellations)
- Curses as a buff/debuff mechanic
- Traps (craftable, placeable)
- Special weapons and specializations
- Main world with player-created locations embedded in it
- HP as a single visible bar, expandable in menus to show all components

---

## The World

### Main World
A persistent world that connects everything.
Players move through it to reach dungeons, trade posts, wilderness zones.

> **Questions:**
> - Is the main world also procedurally generated, or is it a fixed map?
> - Is it large and open (exploration-focused) or small and hub-like (transit between content)?
> - Does the main world have its own dangers, or is it safe-ish?

### Dungeons
Both a location type AND a player tool.
Natural dungeons exist as content. Players can claim a room within one as a base.

> **Questions:**
> - Are dungeon floors infinite (keep descending)? Or do they have a bottom?
> - Do floors get harder the deeper you go, with better loot as a reward?
> - Is each dungeon unique per player, or shared/instanced?
> - DCC reference: the dungeon floors "reset" periodically in the lore — is there a mechanic like this?

---

## Player Bases

The core "home" mechanic. A player claims a room (or set of rooms) and builds it out.

### Workstations
Each workstation enables a different craft:
- Alchemy table (potions, poisons, curses)
- Forge / anvil (weapons, armor, trap components)
- Enchanting table (adding magical properties to items)
- Trap workshop (assembling trap mechanisms from components)
- Herb garden / reagent rack (growing or storing ingredients)
- Rune desk (writing scrolls, binding spells)

> **Questions:**
> - Are workstations crafted themselves, or purchased/found?
> - Can workstations be upgraded? What does that unlock?
> - Do workstations require power sources (magic crystals, fuel)?

### Base Defense
Other players or NPC raiders can enter a player's base.
Traps, guards, locked doors, and layout all serve as defense.

> **Questions:**
> - Is raiding consensual (opt-in PvP) or always-on?
> - Does losing a raid mean losing items, or just prestige/resources?
> - Can you hire NPC guards?
> - Are there visiting rules — e.g., allied players can enter safely?

---

## Combat and Specializations

### Specialization Trees (Suggested Categories)
Not mutually exclusive — player can mix, but depth rewards focus.

**Combat paths:**
- Warrior — armor, melee, resistance to status effects
- Rogue — stealth, traps, poison application, critical hits
- Mage — spells, curse manipulation, reagent efficiency
- Ranger — ranged, kiting, trap placement from range, animal companions

**Crafting paths:**
- Alchemist — unlocks advanced potion combinations, reduces cancellation chance
- Artificer — better traps, weapon modifications, mechanical companions
- Enchanter — longer-lasting or more powerful enchantments on gear
- Herbalist — grows rarer reagents, identifies unknown ingredients on sight

> **Idea:** Specializations aren't classes selected at start — they're leveled by doing.
> Use alchemy a lot → you naturally progress the Alchemist tree.
> This removes the "pick wrong class" problem.

---

## Alchemy System

### Trait-Based Combination
Each ingredient has one or more **traits** (tags):
`[fire]` `[cold]` `[healing]` `[mind]` `[toxic]` `[spirit]` `[swift]` `[heavy]` etc.

Combinations:
- **Synergy:** Two ingredients share a trait → that effect is amplified
- **Cancellation:** Opposing traits (fire + cold) → both cancel out, or produce a third effect
- **Transformation:** Specific rare combos produce a named unique effect neither ingredient has alone

### Potion Anatomy
Each potion has:
- **Primary effect** (dominant trait wins)
- **Duration** (affected by ingredient potency and quantity)
- **Intensity** (stackable, but overdose can cause negative effects)
- **Side effects** (from mismatched traits that didn't cancel cleanly)

> **Example:**
> Bloodmoss `[healing][fire]` + Coldroot `[cold][swift]`
> Fire + cold cancel. Healing + swift remain.
> Result: Swift Healing Potion — restores HP quickly but briefly.

> **Example:**
> Deathcap `[toxic][mind]` + Moonpetal `[mind][spirit]`
> Mind stacks (amplified). Toxic + spirit don't cancel.
> Result: Mind Shatter Draught — heavy confusion debuff, lingering spirit damage.

### Potions as Weapons
- Throwable (splash area)
- Applied to weapons (coat blade)
- Left as bait (on a container in your base)
- Brewed into poison gas (trap component)

> **Questions:**
> - How many ingredients per potion? Fixed (2-3), or scalable with skill?
> - Can you "discover" new combinations through experimentation and save the recipe?
> - Are ingredients consumed on failed experiments, or only on brews?

---

## Curses

Curses are a distinct mechanic — not just debuffs.

### Core Idea
A curse is a **conditional modifier** — it changes your stats or abilities based on a trigger.

> **Examples:**
> - *Curse of Iron* — cannot wear metal armor. Gain +20% dodge instead.
> - *Bloodthirst Curse* — you deal 50% more damage, but lose 1 HP per second unless you attacked something in the last 10 seconds.
> - *Seer's Burden* — you can see through walls in a radius. You cannot see anything beyond 6 tiles.
> - *Pact of the Pit* — your fire damage is tripled. Water kills you instantly.

### Sources of Curses
- Ancient artifacts (cursed items — equip to receive)
- Boss rewards (choose a curse as a power-up)
- Alchemy (brew a curse tincture, apply to self or enemies)
- Environmental (Scar-equivalent zones — linger too long, pick up a curse)

### Stacking
- Curses can interact: *Bloodthirst + Iron* might produce a unique combined state
- Some curses cancel: *Curse of Sloth* (move slowly, gain def) + *Curse of Haste* (move fast, lose def) = neither applies
- Some curses amplify: two `[fire]` curses might cause *Immolation* (massive fire output, self-damage)

> **Idea:** A specialization that leans into curse stacking — the "Hexblade" or "Pact-Keeper" who
> deliberately accumulates curses for massive power at high risk.

---

## Traps

Craftable from components. Placeable in bases and in the dungeon itself.

### Trap Components
- Trigger mechanism (pressure plate, tripwire, proximity, timed)
- Payload (spike, flame jet, gas cloud, net, alarm, teleport)
- Power source (spring, rune, mana crystal)

Combine: Tripwire + Poison Gas Canister + Mana Crystal = triggered poison cloud that refills.

### Trap Interactions
- Player-placed traps can be detected by high-Perception characters
- Rogues can disarm or re-aim them (flip trap to face other way)
- Traps can be stacked in a room (multiple triggers, chained)
- Traps can be destroyed (fire-based trap + water splash = disabled)

> **Questions:**
> - Does the player set traps only in their base, or anywhere in the dungeon?
> - Are traps persistent across sessions, or reset with the floor?

---

## HP System

### Visible: One Bar
From the outside, health is a single bar. Clear, readable, no cognitive load mid-combat.

### Expanded View (In-Menu)
Opening the status screen shows the full picture:

```
[ HP: 84 / 120 ]
  ├─ Body HP:     50 / 70    [Head: 12  Torso: 22  Arms: 9  Legs: 7]
  ├─ Armor:       20 / 30    [Helm: 5   Chest: 10  Greaves: 5]
  ├─ Ward (magic shield): 14 / 20
  └─ Bonus HP (potion):   0

  Active modifiers:
  ├─ [Bleeding] -2 HP/turn on Body HP only
  ├─ [Fire Ward] Ward regenerates 1/turn while not hit
  └─ [Curse: Bloodthirst] Body HP drains if idle >10s
```

### Body Part Damage
Localized damage has effects:
- Head: confusion chance at low HP
- Arms: reduced attack damage / crafting speed
- Legs: reduced movement speed
- Torso: main pool, death when this hits 0

Armor absorbs hits per-zone before body takes damage.
Magic ward is a global buffer that depletes first.

> **Idea:** "Triage" as a skill — field-dressing a specific body part stops bleed and restores that zone.
> Versus a full heal that restores everything but takes longer.

---

## Items and Weapons

### Weapon Types (Suggested)
- Melee: swords, axes, hammers, daggers, spears (different moveset/range)
- Ranged: bows, crossbows, throwing knives, slings
- Magic: staves, wands, foci (channeling vs. burst)
- Hybrid: runeblades (melee + enchant), venom daggers (melee + alchemy)

### Unique Items
Named items with specific lore and fixed (but unusual) stats.
Not random-rolled. Crafted via rare recipes or found in specific locations.

> **Idea:** Unique items have a "history" that grows as you use them.
> Kill 50 enemies with a sword, it gains a new trait.
> This makes items feel owned rather than found.

### Weapon Modifications
Applied at the forge or via alchemy:
- Poison coatings (alchemy)
- Elemental enchants (enchanting table)
- Weight/balance adjustments (forge, affects speed vs. damage)
- Trap integration (crossbow with net bolt attachment)

---

## Open Questions (To Resolve in the Full Doc)

1. **Singleplayer or multiplayer?** The base-raiding and "main world with player locations" implies MP. Is SP viable without it? Or is this inherently co-op/competitive?

2. **Perspective?** Top-down tile-based (current engine), isometric, or something else?

3. **Progression model?** Permadeath (true roguelike), persistent character with roguelike dungeon, or something in between?

4. **Tone?** DCC is dark comedy. What is this? Grim? Humorous? High fantasy? Low fantasy/gritty?

5. **Win condition?** Is there one? Descend to floor X, kill a final boss, build the "perfect" dungeon? Or is it a sandbox with no end?

6. **Floor resets?** In DCC, the dungeon periodically resets/collapses. Does the dungeon here ever "end"? What happens to player bases when a floor resets?

7. **Economy?** Is there a shared economy (player-to-player trading), or is everything crafted locally? What's the currency?

8. **Setting specifics?** Generic fantasy, or a named world with specific lore? Any prior material to build from?

9. **Scope vs. Outpost 13C?** Is this a pivot from the current project, a parallel project, or a longer-term idea to start planning now?

---

## My Feedback and Ideas

**What's strong:**
- The alchemy system as designed is genuinely interesting. Trait-based cancellation/synergy is more intuitive than percentage math and produces emergent "discovery" moments. Keep it.
- The cursed item system that gives power at a cost is a great player-expression tool. DCC does this well; your version with stacking interactions goes further.
- Bases as dungeons that other players can raid creates a natural content loop: you need to go deeper to get better materials to better defend your base, which you need because better raiders are coming. That's a tight loop.
- Workstation-gated crafting means progression is spatial as well as character-level. Your base layout becomes part of your build.

**Things to think through:**
- The body-part HP system is great on paper but adds cognitive load in real-time combat. The "one bar externally, detailed in menu" approach solves this well. Just make sure triage in the field is simple to execute (one keypress, not a sub-menu).
- Specialization by doing (rather than class selection) is good but needs guardrails: players can accidentally spread themselves too thin. Consider a "focus" slot — you can advance any tree, but one tree advances faster.
- Traps in your own base are fun, but friendly-fire and accidental triggering need design attention early. Your own traps should either be toggleable or recognize your "faction tag."

**Ideas to consider adding:**
- **The Spectator Layer** (from DCC): an in-game "audience" mechanic where NPCs or players can watch dungeon runs. Could feed into economy (betting) or reputation (fame stat).
- **Dungeon Reputation / Notoriety**: your base gets a "danger rating" based on traps, mobs, layout. Higher danger = better NPC contracts / more valuable loot found by raiders.
- **Reagent Scarcity**: some alchemy ingredients only appear on deep floors, making the dungeon crawl necessary for the crafting game. Keeps both systems linked.
- **Curse Markets**: NPCs who trade in curses — apply one to yourself for a payment, or sell a curse you found to them. Creates strange economy.
- **Living Dungeons**: the dungeon itself slowly "reacts" to player presence — more dangerous over time if you linger, or starts generating new rooms. Keeps exploration from feeling static.

---

*Return to this document once the full vision is locked down.*
*Next step: expand into final product spec, then evaluate whether to adapt the Outpost 13C engine or start fresh.*
