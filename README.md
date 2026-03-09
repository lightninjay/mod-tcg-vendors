<div align="center">
  <H1><b>TCG Vendors</b></H1><br>
  <img src="banner.png" alt="TCG Vendor Banner" width="800px">

An [AzerothCore](https://www.azerothcore.org/) module that implements fully functional item
redemption<br> for five World of Warcraft promotional item NPCs present in the 3.3.5a client.

| NPC | Location | Items |
|-----|----------|-------|
| **Landro Longshot** | Booty Bay | TCG card set rewards |
| **Ransin Donner** | Ironforge — The Forlorn Cavern | Blizzcon promotional items |
| **Zas'Tysh** | Orgrimmar — Valley of Heroes | Blizzcon promotional items |
| **Garel Redrock** | Ironforge — The Forlorn Cavern | Murloc companions & additional promotional items |
| **Tharl Stonebleeder** | Orgrimmar — Valley of Heroes | Murloc companions & additional promotional items |

Landro Longshot, Ransin Donner, and Zas'Tysh exist in the default AzerothCore database as
stubs with no item delivery logic. Garel Redrock and Tharl Stonebleeder are their counterpart
NPCs — Tharl is present in the 3.3.5a client data, while Garel is a custom spawn that
completes the Alliance side. This module brings all five NPCs to life with configurable
redemption modes, correct item entry IDs, and a full GM delivery and management toolset.

> **Note:** Developed against the
> [mod-playerbots fork](https://github.com/liyunfan1223/azerothcore-wotlk) of AzerothCore.
> Should be compatible with standard AzerothCore mainline. See
> [Compatibility](#compatibility) for details.

---

## Item Catalogue

<details>
<summary><strong>TCG Expansions — Landro Longshot (Booty Bay)</strong></summary>

| Expansion | Item | Entry | Type |
|-----------|------|-------|------|
| Heroes of Azeroth | Tabard of Flame | 23705 | Unique |
| Heroes of Azeroth | Hippogryph Hatchling | 23713 | Unique |
| Heroes of Azeroth | Riding Turtle | 23720 | Unique |
| Through the Dark Portal | Picnic Basket | 32566 | Unique |
| Through the Dark Portal | Banana Charm | 32588 | Unique |
| Through the Dark Portal | Imp in a Ball | 32542 | Unique |
| Fires of Outland | Goblin Gumbo Kettle | 33219 | Unique |
| Fires of Outland | Fishing Chair | 33223 | Unique |
| Fires of Outland | Reins of the Spectral Tiger *(both variants)* | 33224 + 33225 | Unique |
| March of the Legion | Paper Flying Machine Kit | 34499 | Unique |
| March of the Legion | Rocket Chicken | 34492 | Unique |
| March of the Legion | Dragon Kite | 34493 | Unique |
| Servants of the Betrayer | X-51 Nether-Rocket *(both variants)* | 35225 + 35226 | Unique |
| Servants of the Betrayer | Papa Hummel's Old-Fashioned Pet Biscuit | 35223 | Consumable |
| Servants of the Betrayer | Goblin Weather Machine - Prototype 01-B | 35227 | Unique |
| Hunt for Illidan | Path of Illidan | 38233 | Consumable |
| Hunt for Illidan | D.I.S.C.O. | 38301 | Unique |
| Hunt for Illidan | Soul-Trader Beacon | 38050 | Unique |
| Drums of War | Party G.R.E.N.A.D.E. | 38577 | Consumable |
| Drums of War | The Flag of Ownership | 38578 | Unique |
| Drums of War | Big Battle Bear | 38576 | Unique |
| Blood of Gladiators | Sandbox Tiger | 45047 | Consumable |
| Blood of Gladiators | Epic Purple Shirt | 45037 | Unique |
| Blood of Gladiators | Foam Sword Rack | 45063 | Unique |
| Fields of Honor | Path of Cenarius | 46779 | Consumable |
| Fields of Honor | Ogre Pinata | 46780 | Unique |
| Fields of Honor | Magic Rooster Egg | 46778 | Unique |
| Scourgewar | Scourgewar Mini-Mount *(faction-aware)* | 49288 / 49289 | Consumable |
| Scourgewar | Tuskarr Kite | 49287 | Unique |
| Scourgewar | Spectral Tiger Cub | 49343 | Unique |
| Wrathgate | Landro's Gift Box *(configurable)* | 54218 | Unique† |
| Wrathgate | Instant Statue Pedestal | 54212 | Unique |
| Wrathgate | Blazing Hippogryph | 54069 | Unique |
| Icecrown | Paint Bomb | 54455 | Consumable |
| Icecrown | Ethereal Portal | 54452 | Unique |
| Icecrown | Wooly White Rhino | 54068 | Unique |
| Points Redemption | Tabard of Frost | 23709 | Unique |
| Points Redemption | Perpetual Purple Firework | 23714 | Unique |
| Points Redemption | Carved Ogre Idol | 23716 | Unique |
| Points Redemption | Tabard of the Arcane | 38310 | Unique |
| Points Redemption | Tabard of Brilliance | 38312 | Unique |
| Points Redemption | Tabard of the Defender | 38314 | Unique |
| Points Redemption | Tabard of Fury | 38313 | Unique |
| Points Redemption | Tabard of Nature | 38309 | Unique |
| Points Redemption | Tabard of the Void | 38311 | Unique |
| Points Redemption | Landro's Pet Box *(configurable)* | 50301 | Unique† |

† Behaviour controlled by `TCGVendors.LandroBoxesMultiRedeem`. Default is one-time per character.

</details>

<details>
<summary><strong>Blizzcon Promotional Items — Ransin Donner (Ironforge) / Zas'Tysh (Orgrimmar)</strong></summary>

| Item | Entry | Type |
|------|-------|------|
| Murky (Blue Murloc Egg) | 20371 | Unique |
| Murloc Costume | 33079 | Unique |
| Big Blizzard Bear | 43599 | Unique |

</details>

<details>
<summary><strong>Murloc Companions — Garel Redrock (Ironforge) / Tharl Stonebleeder (Orgrimmar)</strong></summary>

| Item | Entry | Type |
|------|-------|------|
| Gurky (Pink Murloc Egg) | 22114 | Unique |
| Orange Murloc Egg | 20651 | Unique |
| White Murloc Egg | 22780 | Unique |
| Heavy Murloc Egg | 46802 | Unique |
| Murkimus' Little Spear | 45180 | Unique |

</details>

<details>
<summary><strong>Classic & Special Promotions — Garel Redrock / Tharl Stonebleeder</strong></summary>

| Item | Entry | Type | Notes |
|------|-------|------|-------|
| Zergling Leash | 13582 | Unique | WoW Collector's Edition |
| Panda Collar | 13583 | Unique | WoW Collector's Edition |
| Diablo Stone | 13584 | Unique | WoW Collector's Edition |
| Netherwhelp's Collar | 25535 | Unique | TBC Collector's Edition |
| Frosty's Collar | 39286 | Unique | WotLK Collector's Edition |
| Tyrael's Hilt | 39656 | Unique | Blizzard promotional |
| Warbot Ignition Key | 46767 | Unique | Mountain Dew Promotion |
| Red War Fuel | 46766 | Consumable | Requires Warbot companion (spell 65682) |
| Blue War Fuel | 46765 | Consumable | Requires Warbot companion (spell 65682) |

> **War Fuel:** Freely redeemable in any server mode with no code required. The character
> must have already learned the Warbot companion pet. Both fuel types are unlimited.

</details>

<details>
<summary><strong>Blizzard Store — Garel Redrock / Tharl Stonebleeder</strong></summary>

| Item | Entry | Type |
|------|-------|------|
| Enchanted Onyx | 48527 | Unique |
| Core Hound Pup | 49646 | Unique |
| Gryphon Hatchling | 49662 | Unique |
| Wind Rider Cub | 49663 | Unique |
| Pandaren Monk | 49665 | Unique |

</details>

<details>
<summary><strong>Special Events & Tournaments — Garel Redrock / Tharl Stonebleeder</strong></summary>

| Item | Entry | Type |
|------|-------|------|
| Lil' Phylactery | 49693 | Unique |
| Lil' XT | 54847 | Unique |
| Mini Thor | 56806 | Unique |
| Onyxian Whelpling | 49362 | Unique |

</details>

---

## Features

- **Four configurable operation modes** covering every server style, from fully free to
  retail-authentic code redemption.
- **74 reward items** spanning all 13 TCG expansion sets, three Blizzcon promotional items,
  and the full Garel/Tharl promotional catalogue.
- **Multi-item set support** — Spectral Tiger and X-51 Nether-Rocket each award both mount
  variants in a single redemption.
- **Faction-aware delivery** — the Scourgewar Mini-Mount awards the Horde or Alliance variant
  automatically based on the receiving character's race.
- **Unique vs. consumable item classification** — permanent unlocks are one-time per
  character; charged or stackable toys are unlimited.
- **Companion-gated items** — Red War Fuel and Blue War Fuel are freely redeemable in any
  mode, any number of times, but only to characters who have already learned the Warbot
  companion (spell 65682).
- **Inventory fallback** — if a player's bags are full at redemption, all items are mailed
  to the character rather than being blocked or lost.
- **GM free-browse menu** in all modes (including Disabled), allowing Game Masters with GM
  mode active to browse the full catalogue and deliver any item to any character by name.
- **GM force-delivery override** — if a target character has already received a unique item,
  the GM is presented with an override confirmation dialog rather than a silent block.
- **GM redemption flag management** — a dedicated menu option clears all
  `character_tcg_redeemed` records for a named character, resetting their unique-item
  eligibility.
- **Configurable Landro's Box behaviour** — Landro's Gift Box and Landro's Pet Box can be
  set to one-time or unlimited per character independently of all other items.
- **Crash-safe code redemption** — codes are marked used before item delivery so a partial
  delivery on server crash cannot be replayed.
- **Interactive code generator dashboard** (`tools/generate_codes.py`) with a terminal UI
  and full non-interactive CLI support for scripted workflows.

---

## Operation Modes

Set via `TCGVendors.Mode` in `mod-tcg-vendors.conf`.

| Mode | Name | Description |
|------|------|-------------|
| `0` | **Disabled** | Module registers but defers entirely to default database gossip. No items are offered by the scripts. Useful for temporarily suspending the system without reverting any SQL. |
| `1` | **Free** | No codes required. All players browse the full catalogue and claim items directly from the gossip menu. Unique items are one-time per character; consumables are unlimited. |
| `2` | **Blizz-like** *(default)* | Players enter a pre-generated `XXXX-XXXX-XXXX-XXXX` code at the NPC root dialog. Each code is single-use, tracked at account level. The reward is determined entirely by the code — the player does not browse to an item first. Blizzcon vendors show items directly with per-item code dialogs in this mode. |
| `3` | **Item-Specific Code** | Players browse to a specific item and enter a code for that item. A valid code for a *different* item produces a mismatch error. Closest to the original retail experience. |

> **Note on companion-gated items (War Fuel):** Red War Fuel and Blue War Fuel bypass the
> mode setting entirely. They are always presented as a free confirmation click — no code is
> ever generated or required. The only gate is whether the character has already learned the
> Warbot companion pet.

---

## Requirements

- [AzerothCore](https://www.azerothcore.org/) 3.3.5a (or the
  [mod-playerbots fork](https://github.com/liyunfan1223/azerothcore-wotlk))
- Python 3.8+ *(for `tools/generate_codes.py` only — server compilation does not require Python)*

---

## Installation

### 1. Place the module

Clone or copy the `mod-tcg-vendors` folder into your AzerothCore `modules` directory:

```
azerothcore-wotlk/
└── modules/
    └── mod-tcg-vendors/     ← folder must be named exactly this
        ├── conf/
        ├── sql/
        ├── src/
        └── tools/
```

> **Important:** If you downloaded a ZIP from GitHub, remove any `-main`, `-master`, or
> similar suffix from the folder name. The folder must be named exactly `mod-tcg-vendors` or
> the build system will generate a mismatched script registration function name and fail to link.

### 2. Apply the SQL

**Characters database** — creates the two tables this module uses:

```bash
mysql -u root -p acore_characters < sql/characters/base/create_tcg_redeemed_table.sql
mysql -u root -p acore_characters < sql/characters/base/create_tcg_codes_table.sql
```

**World database** — sets `ScriptName` on the five NPC entries, adds NPC greeting texts,
ensures the gossip flag is set on each NPC, and handles all creature spawns:

```bash
mysql -u root -p acore_world < sql/world/base/zzz_tcg_vendors_setup.sql
```

The world SQL handles the following automatically — no manual in-game steps required:
- Spawns **Garel Redrock** in The Forlorn Cavern next to Ransin Donner
- Spawns a **Gurky** companion murloc next to Garel, mirroring Tharl and Gurky in Orgrimmar
- Repositions **Murky** in Ironforge to stand next to Ransin Donner in the correct orientation

The SQL does not modify any item templates or other existing world data beyond these NPC rows.

> **Load order:** The file is prefixed `zzz_` so that the DB auto-updater applies it after
> all other module SQL patches, preventing other modules from inadvertently overwriting these
> creature or NPC text entries.

### 3. Recompile

Reconfigure CMake and rebuild the core. The module is detected and compiled automatically.
Verify it appears in the CMake output:

```
* Modules configuration
  ...
  + mod-tcg-vendors
```

### 4. Configure

Copy `conf/mod-tcg-vendors.conf.dist` to your server's conf directory and rename it:

```bash
cp conf/mod-tcg-vendors.conf.dist /path/to/server/conf/mod-tcg-vendors.conf
```

Edit the file and set your preferred mode and options. See [Configuration](#configuration)
for all available options.

### 5. Populate redemption codes (Modes 2 and 3 only)

If you are running Mode 2 or Mode 3, generate codes using the included tool and import them
into your characters database before players begin redeeming:

```bash
# Launch the interactive dashboard (recommended for first-time setup)
python3 tools/generate_codes.py
```
See [Code Generator](#code-generator) for full usage.

---

## Configuration

All options live in `mod-tcg-vendors.conf`.

---

## GM Tools

Game Masters with GM mode active (`.gm on`) receive a special menu at all five NPCs in every
operation mode, including Mode 0. GMs playing without GM mode active see the same menu as
regular players.

### Item Delivery

At Landro Longshot, the GM browses through the full expansion tree. At the Blizzcon and
Promo vendors, all items are listed directly. Clicking any item opens a text input prompting
for a character name. The item is then mailed to that character — online or offline — without
consuming a code or checking bag space.

### Force-Delivery Override

If the target character has already received a unique item, the GM is shown an override
confirmation dialog. Typing the character's name a second time confirms the forced
re-delivery, bypassing the redemption flag check entirely.

### Clear Redemption Flags

A dedicated menu option at each NPC prompts for a character name and deletes all rows in
`character_tcg_redeemed` for that character, resetting their eligibility for all unique items.
The count of cleared records is reported back as a whisper.

---

## Code Generator

`tools/generate_codes.py` generates cryptographically random redemption codes in
`XXXX-XXXX-XXXX-XXXX` format using an unambiguous character set (no `I`, `L`, `O`, `0`, or
`1`). Output is `INSERT IGNORE INTO account_tcg_codes` SQL statements ready to apply directly
to your characters database.

**No external dependencies required.** Uses only the Python standard library.

> **Note:** Red War Fuel and Blue War Fuel are intentionally absent from the code generator.
> No codes exist for these items — they are always redeemed freely at the NPC.

### Interactive Dashboard

```bash
python3 tools/generate_codes.py
```

| Option | Description |
|--------|-------------|
| **Single Item** | Select one item, enter a count and optional output path. |
| **Multi-Select** | `Space` to toggle items, `A` to select all, `N` to clear, `Enter` to confirm. |
| **Full Batch** | Generates codes for all code-redeemable items. Prompts for count and output path. |
| **Quick Batch** | Full batch with a single count prompt — fastest option for populating a new server. |
| **Exit** | Quit. |

### Non-Interactive CLI

```bash
# List all valid reward group keys
python3 tools/generate_codes.py --list-groups

# Generate 10 codes for the Riding Turtle
python3 tools/generate_codes.py --reward-group TCG_RIDING_TURTLE --count 10

# Generate codes for multiple items in one run
python3 tools/generate_codes.py --reward-group TCG_RIDING_TURTLE TCG_SPECTRAL_TIGER --count 5

# Generate codes for every available item and write to a file
python3 tools/generate_codes.py --all --count 10 --output full_batch.sql

# Preview codes without generating SQL
python3 tools/generate_codes.py --reward-group BLIZZCON_MURKY --count 3 --dry-run

# Force the interactive dashboard even if other flags are present
python3 tools/generate_codes.py --interactive
```

### Applying Codes

```bash
mysql -u root -p acore_characters < full_batch.sql
```
---

## Database Tables

Both tables are created in the **characters** database.

### `character_tcg_redeemed`

Tracks which unique items have been delivered to each character. Prevents a character from
receiving the same permanent unlock more than once, regardless of how many valid codes are
presented.

| Column | Type | Description |
|--------|------|-------------|
| `guid` | `INT UNSIGNED` | Character GUID |
| `item_entry` | `MEDIUMINT UNSIGNED` | Item template entry ID |
| `redeemed_date` | `TIMESTAMP` | When the item was delivered |

Primary key: `(guid, item_entry)`

### `account_tcg_codes`

Stores pre-generated redemption codes. Populated by `tools/generate_codes.py`.

| Column | Type | Description |
|--------|------|-------------|
| `code` | `VARCHAR(19)` | The code in `XXXX-XXXX-XXXX-XXXX` format |
| `reward_group` | `VARCHAR(64)` | Identifies which item(s) the code awards |
| `redeemed` | `TINYINT(1)` | `0` = unused, `1` = redeemed |
| `account_id` | `INT UNSIGNED` | Account that redeemed the code (set on use) |
| `character_guid` | `INT UNSIGNED` | Character that received the items (set on use) |
| `redeemed_date` | `TIMESTAMP` | When the code was consumed (set on use) |

Primary key: `code`

The `reward_group` values match the keys shown by `--list-groups` exactly. To add codes
manually without using the generator, insert rows with a valid `reward_group` key and
`redeemed = 0`.

---

## Technical Notes

### Inventory Full — Mail Fallback

If a player's bags are full at the time of redemption, all items for that redemption are
mailed to the character rather than blocking the delivery. The redemption is recorded
immediately so a full-bags situation cannot be exploited to obtain items repeatedly. The
player receives a whisper directing them to a mailbox.

### Crash Safety (Modes 2 and 3)

In code-entry modes, the `account_tcg_codes` row is marked as redeemed *before* items are
delivered. In the unlikely event of a server crash between the database update and the item
delivery, a Game Master can verify the code row and use the GM delivery menu to re-deliver
without consuming an additional code.

### Unique vs. Consumable — Companion Items

Several items have `spellcharges = -1` in `item_template` (the flag that makes an item
destroy itself on use), which might suggest they are consumable. However, items that teach
a **permanent companion spell** are classified as **Unique** (one-time per character),
because the spell is learned once and the player has no reason to receive the item again.
This applies to Soul-Trader Beacon (38050), Banana Charm (32588), and Tuskarr Kite (49287),
among others. True consumables — Papa Hummel's Pet Biscuit, Path of Illidan, Sandbox Tiger,
Paint Bomb, and similar charged toys — are items where the player genuinely benefits from
having more than one.

### Scourgewar Mini-Mount Faction Handling

The Scourgewar Mini-Mount has two item entries: Little Ivory Raptor Whistle (49288) for
Horde and Little White Stallion Bridle (49289) for Alliance. The delivery system resolves
the correct variant from the *receiving character's* race — not the GM's or the redeeming
player's. The redemption key stored in `character_tcg_redeemed` is always 49288 regardless
of which variant was delivered, so the per-character gate works correctly for both factions.

---

## Compatibility

| Core | Status |
|------|--------|
| [mod-playerbots fork](https://github.com/liyunfan1223/azerothcore-wotlk) | ✅ Developed and tested here |
| [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk) mainline | ✅ Should be compatible |

The script registration function is named `Addmod_tcg_vendorsScripts()`, following the
Playerbots fork build system conventions. This name is also valid on standard AzerothCore
(the convention was standardised in a 2022 core update), so no changes should be required
for either target.

---

## Credits

- [AzerothCore](https://www.azerothcore.org/) — the core emulator and module framework.
- The AzerothCore community for module conventions and reference implementations.
- Blizzard Entertainment — original designers of the WoW TCG promotional item system and the
  NPCs this module brings to life.

---

## License

This module is released under the [MIT License](LICENSE).

---

*AzerothCore: [repository](https://github.com/azerothcore/azerothcore-wotlk) •
[website](https://www.azerothcore.org/) •
[Discord](https://discord.gg/gkt4y2x)*

</div>
