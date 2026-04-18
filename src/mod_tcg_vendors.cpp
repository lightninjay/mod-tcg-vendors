#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GossipDef.h"
#include "Group.h"
#include "Item.h"
#include "Log.h"
#include "LootMgr.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

// ============================================================
//  Operation modes — mirror TCGVendors.Mode config values
// ============================================================
enum TCGVendorMode : int
{
    MODE_DISABLED  = 0,
    MODE_FREE      = 1,
    MODE_BLIZZLIKE = 2,
    MODE_ITEM_CODE = 3,
};

// ============================================================
//  NPC Entry IDs
// ============================================================
enum TCGNPCEntries : uint32
{
    NPC_LANDRO_LONGSHOT      = 17249,
    NPC_RANSIN_DONNER        = 2943,
    NPC_ZAS_TYSH             = 7951,
    NPC_THARL_STONEBLEEDER   = 16076,
    NPC_GAREL_REDROCK        = 16070,
    NPC_EDWARD_CAIRN         = 29095,
    NPC_IAN_DRAKE            = 29093,
};

// ============================================================
//  NPC greeting text IDs  (see sql/world/base/tcg_vendors_setup.sql)
// ============================================================
enum TCGNpcTextIds : uint32
{
    NPC_TEXT_LANDRO   = 90001,
    NPC_TEXT_BLIZZCON = 90002,
    NPC_TEXT_PROMO    = 90003,   // Garel Redrock / Tharl Stonebleeder
    NPC_TEXT_WWI      = 90004,   // Edward Cairn / Ian Drake (Worldwide Invitational)
};

// ============================================================
//  Gossip sender values
// ============================================================
enum GossipSenders : uint32
{
    SENDER_CODE_ENTRY = 0,
    SENDER_MAIN       = 1,
    SENDER_HOA        = 2,
    SENDER_TDP        = 3,
    SENDER_FOO        = 4,
    SENDER_MOTL       = 5,
    SENDER_SOTB       = 6,
    SENDER_HFI        = 7,
    SENDER_DOW        = 8,
    SENDER_BOG        = 9,
    SENDER_FOH        = 10,
    SENDER_SW         = 11,
    SENDER_WG         = 12,
    SENDER_IC         = 13,
    SENDER_PR         = 14,

    // Sender for the GM "Clear redemption flags" text-input button.
    SENDER_GM_CLEAR    = 15,

    // Sender for the GM "Force re-deliver" override confirmation dialog.
    SENDER_GM_FORCE    = 16,

    // Sender for the GM "Send a code to a player" browse path and final
    // text-input submission.  Sits between force-delivery (16) and promo (18-21).
    SENDER_GM_SEND_CODE = 17,

    // Promo vendor (Garel Redrock / Tharl Stonebleeder) category sub-menu senders.
    SENDER_PROMO_MURLOC  = 18,
    SENDER_PROMO_CLASSIC = 19,
    SENDER_PROMO_STORE   = 20,
    SENDER_PROMO_EVENTS  = 21,

    // Action used on the root menu "Browse by expansion set" button.
    ACTION_OPEN_BROWSE    = 99,
    ACTION_OPEN_SEND_CODE = 98,   // Root "[GM] Send a code to a player..." button
};

// ============================================================
//  Item entry IDs for Landro's boxes — referenced by the
//  LandroBoxesMultiRedeem config check.
// ============================================================
static constexpr uint32 ITEM_LANDROS_GIFT_BOX = 54218;
static constexpr uint32 ITEM_LANDROS_PET_BOX  = 50301;

// Spell taught by the Warbot Ignition Key.  Red/Blue War Fuel are only
// displayed (and redeemable) after the player has learned this companion.
static constexpr uint32 WARBOT_PET_SPELL = 65682;

// ============================================================
//  TCGItem  —  one entry in a gossip browse sub-menu
//
//  isConsumable   When true in Mode 1: skip character_tcg_redeemed
//                 so the player can take the item repeatedly.
//                 In Mode 2 the code is still consumed; isConsumable
//                 only removes the per-character uniqueness gate,
//                 allowing a second legitimate code for the same
//                 consumable to be redeemed on the same character.
//
//  factionMount   Horde gets entries[0], Alliance gets entries[1].
//                 entries[0] is always the redemption key.
// ============================================================================
struct TCGItem
{
    std::string         displayName;
    std::vector<uint32> entries;
    bool                factionMount = false;
    bool                isConsumable = false;
    std::string         rewardGroupKey; // Mode 3: expected reward group key
    uint32              requiredSpell = 0; // 0 = no condition; else player->HasSpell() must be true
};

// ============================================================
//  RewardGroup  —  what a single code awards (Mode 2)
//  String key must match account_tcg_codes.reward_group and
//  the REWARD_GROUPS dict in tools/generate_codes.py.
// ============================================================
struct RewardGroup
{
    std::string         displayName;
    std::vector<uint32> itemEntries;
    bool                factionMount = false;
    bool                isConsumable = false;
};

// ============================================================
//  Master reward group catalog  (Mode 2 code path)
// ============================================================
static const std::map<std::string, RewardGroup> REWARD_GROUPS =
{
    // --- Heroes of Azeroth ---
    { "TCG_TABARD_OF_FLAME",           { "Tabard of Flame",                             { 23705 }        } },
    { "TCG_HIPPOGRYPH_HATCHLING",      { "Hippogryph Hatchling",                        { 23713 }        } },
    { "TCG_RIDING_TURTLE",             { "Riding Turtle",                               { 23720 }        } },

    // --- Through the Dark Portal ---
    { "TCG_PICNIC_BASKET",             { "Picnic Basket",                               { 32566 }        } },
    { "TCG_BANANA_CHARM",              { "Banana Charm",                                { 32588 }        } },
    { "TCG_IMP_IN_A_BALL",             { "Imp in a Ball",                               { 32542 }        } },

    // --- Fires of Outland ---
    { "TCG_GOBLIN_GUMBO_KETTLE",       { "Goblin Gumbo Kettle",                         { 33219 }        } },
    { "TCG_FISHING_CHAIR",             { "Fishing Chair",                               { 33223 }        } },
    { "TCG_SPECTRAL_TIGER",            { "Reins of the Spectral Tiger (both variants)", { 33224, 33225 } } },

    // --- March of the Legion ---
    { "TCG_PAPER_FLYING_MACHINE",      { "Paper Flying Machine Kit",                    { 34499 }        } },
    { "TCG_ROCKET_CHICKEN",            { "Rocket Chicken",                              { 34492 }        } },
    { "TCG_DRAGON_KITE",               { "Dragon Kite",                                 { 34493 }        } },

    // --- Servants of the Betrayer ---
    { "TCG_X51_NETHER_ROCKET",         { "X-51 Nether-Rocket (both variants)",          { 35225, 35226 } } },
    { "TCG_PET_BISCUIT",               { "Papa Hummel's Old-Fashioned Pet Biscuit",     { 35223 }, false, true } },
    { "TCG_GOBLIN_WEATHER_MACHINE",    { "Goblin Weather Machine - Prototype 01-B",     { 35227 }        } },

    // --- Hunt for Illidan ---
    { "TCG_PATH_OF_ILLIDAN",           { "Path of Illidan",                             { 38233 }, false, true } },
    { "TCG_DISCO",                     { "D.I.S.C.O.",                                  { 38301 }        } },
    { "TCG_SOUL_TRADER_BEACON",        { "Soul-Trader Beacon",                          { 38050 }        } },

    // --- Drums of War ---
    { "TCG_PARTY_GRENADE",             { "Party G.R.E.N.A.D.E.",                        { 38577 }, false, true } },
    { "TCG_FLAG_OF_OWNERSHIP",         { "The Flag of Ownership",                       { 38578 }        } },
    { "TCG_BIG_BATTLE_BEAR",           { "Big Battle Bear",                             { 38576 }        } },

    // --- Blood of Gladiators ---
    { "TCG_SANDBOX_TIGER",             { "Sandbox Tiger",                               { 45047 }, false, true } },
    { "TCG_EPIC_PURPLE_SHIRT",         { "Epic Purple Shirt",                           { 45037 }        } },
    { "TCG_FOAM_SWORD_RACK",           { "Foam Sword Rack",                             { 45063 }        } },

    // --- Fields of Honor ---
    { "TCG_PATH_OF_CENARIUS",          { "Path of Cenarius",                            { 46779 }, false, true } },
    { "TCG_OGRE_PINATA",               { "Ogre Pinata",                                 { 46780 }        } },
    { "TCG_MAGIC_ROOSTER_EGG",         { "Magic Rooster Egg",                           { 46778 }        } },

    // --- Scourgewar ---
    { "TCG_SCOURGEWAR_MINIMOUNT",      { "Scourgewar Mini-Mount",                       { 49288, 49289 }, true, true } },
    { "TCG_TUSKARR_KITE",              { "Tuskarr Kite",                                { 49287 }        } },
    { "TCG_SPECTRAL_TIGER_CUB",        { "Spectral Tiger Cub",                          { 49343 }        } },

    // --- Wrathgate ---
    { "TCG_LANDROS_GIFT_BOX",          { "Landro's Gift Box",                           { 54218 }        } },
    { "TCG_INSTANT_STATUE_PEDESTAL",   { "Instant Statue Pedestal",                     { 54212 }        } },
    { "TCG_BLAZING_HIPPOGRYPH",        { "Blazing Hippogryph",                          { 54069 }        } },

    // --- Icecrown ---
    { "TCG_PAINT_BOMB",                { "Paint Bomb",                                  { 54455 }, false, true } },
    { "TCG_ETHEREAL_PORTAL",           { "Ethereal Portal",                             { 54452 }        } },
    { "TCG_WOOLY_WHITE_RHINO",         { "Wooly White Rhino",                           { 54068 }        } },

    // --- Points Redemption ---
    { "TCG_TABARD_OF_FROST",           { "Tabard of Frost",                             { 23709 }        } },
    { "TCG_PERPETUAL_PURPLE_FIREWORK", { "Perpetual Purple Firework",                   { 23714 }        } },
    { "TCG_CARVED_OGRE_IDOL",          { "Carved Ogre Idol",                            { 23716 }        } },
    { "TCG_TABARD_OF_THE_ARCANE",      { "Tabard of the Arcane",                        { 38310 }        } },
    { "TCG_TABARD_OF_BRILLIANCE",      { "Tabard of Brilliance",                        { 38312 }        } },
    { "TCG_TABARD_OF_THE_DEFENDER",    { "Tabard of the Defender",                      { 38314 }        } },
    { "TCG_TABARD_OF_FURY",            { "Tabard of Fury",                              { 38313 }        } },
    { "TCG_TABARD_OF_NATURE",          { "Tabard of Nature",                            { 38309 }        } },
    { "TCG_TABARD_OF_THE_VOID",        { "Tabard of the Void",                          { 38311 }        } },
    { "TCG_LANDROS_PET_BOX",           { "Landro's Pet Box",                            { 50301 }        } },

    // --- Blizzcon promotional ---
    { "BLIZZCON_MURKY",                { "Murky (Blue Murloc Egg)",                     { 20371 }        } },
    { "BLIZZCON_MURLOC_COSTUME",       { "Murloc Costume",                              { 33079 }        } },
    { "BLIZZCON_BIG_BLIZZARD_BEAR",    { "Big Blizzard Bear",                           { 43599 }        } },

    // --- Murloc companion eggs ---
    { "PROMO_GURKY",             { "Gurky (Pink Murloc Egg)",   { 22114 }        } },
    { "PROMO_ORANGE_MURLOC_EGG", { "Orange Murloc Egg",         { 20651 }        } },
    { "PROMO_WHITE_MURLOC_EGG",  { "White Murloc Egg",          { 22780 }        } },
    { "PROMO_HEAVY_MURLOC_EGG",  { "Heavy Murloc Egg",          { 46802 }        } },
    { "PROMO_MURKIMUS_SPEAR",    { "Murkimus' Little Spear",    { 45180 }        } },

    // --- Classic & Special Promotions ---
    { "PROMO_ZERGLING_LEASH",    { "Zergling Leash",            { 13582 }        } },
    { "PROMO_PANDA_COLLAR",      { "Panda Collar",              { 13583 }        } },
    { "PROMO_DIABLO_STONE",      { "Diablo Stone",              { 13584 }        } },
    { "PROMO_NETHERWHELP",       { "Netherwhelp's Collar",      { 25535 }        } },
    { "PROMO_FROSTYS_COLLAR",    { "Frosty's Collar",           { 39286 }        } },
    { "WWI_TYRAELS_HILT",      { "Tyrael's Hilt",             { 39656 }        } },
    { "PROMO_WARBOT_KEY",        { "Warbot Ignition Key",       { 46767 }        } },

    // --- Blizzard Store ---
    { "PROMO_ENCHANTED_ONYX",    { "Enchanted Onyx",            { 48527 }        } },
    { "PROMO_CORE_HOUND_PUP",    { "Core Hound Pup",            { 49646 }        } },
    { "PROMO_GRYPHON_HATCHLING", { "Gryphon Hatchling",         { 49662 }        } },
    { "PROMO_WIND_RIDER_CUB",    { "Wind Rider Cub",            { 49663 }        } },
    { "PROMO_PANDAREN_MONK",     { "Pandaren Monk",             { 49665 }        } },

    // --- Special Events & Tournaments ---
    { "PROMO_LIL_PHYLACTERY",    { "Lil' Phylactery",           { 49693 }        } },
    { "PROMO_LIL_XT",            { "Lil' XT",                   { 54847 }        } },
    { "PROMO_MINI_THOR",         { "Mini Thor",                 { 56806 }        } },
    { "PROMO_ONYXIAN_WHELPLING", { "Onyxian Whelpling",         { 49362 }        } },
};

// ============================================================
//  Landro gossip catalog  (Mode 1 all-player browse,
//                          Mode 2 GM-only browse,
//                          Mode 3 item-specific code entry)
// ============================================================
static const std::map<uint32, std::vector<TCGItem>> LANDRO_CATALOG =
{
    { SENDER_HOA,  {
        { "Tabard of Flame",                             { 23705 },        false, false, "TCG_TABARD_OF_FLAME"           },
        { "Hippogryph Hatchling",                        { 23713 },        false, false, "TCG_HIPPOGRYPH_HATCHLING"      },
        { "Riding Turtle",                               { 23720 },        false, false, "TCG_RIDING_TURTLE"             },
    }},
    { SENDER_TDP,  {
        { "Picnic Basket",                               { 32566 },        false, false, "TCG_PICNIC_BASKET"             },
        { "Banana Charm",                                { 32588 },        false, false, "TCG_BANANA_CHARM"              },
        { "Imp in a Ball",                               { 32542 },        false, false, "TCG_IMP_IN_A_BALL"             },
    }},
    { SENDER_FOO,  {
        { "Goblin Gumbo Kettle",                         { 33219 },        false, false, "TCG_GOBLIN_GUMBO_KETTLE"       },
        { "Fishing Chair",                               { 33223 },        false, false, "TCG_FISHING_CHAIR"             },
        { "Reins of the Spectral Tiger (both variants)", { 33224, 33225 }, false, false, "TCG_SPECTRAL_TIGER"            },
    }},
    { SENDER_MOTL, {
        { "Paper Flying Machine Kit",                    { 34499 },        false, false, "TCG_PAPER_FLYING_MACHINE"      },
        { "Rocket Chicken",                              { 34492 },        false, false, "TCG_ROCKET_CHICKEN"            },
        { "Dragon Kite",                                 { 34493 },        false, false, "TCG_DRAGON_KITE"               },
    }},
    { SENDER_SOTB, {
        { "X-51 Nether-Rocket (both variants)",          { 35225, 35226 }, false, false, "TCG_X51_NETHER_ROCKET"         },
        { "Papa Hummel's Old-Fashioned Pet Biscuit",    { 35223 },        false, true,  "TCG_PET_BISCUIT"               },
        { "Goblin Weather Machine - Prototype 01-B",     { 35227 },        false, false, "TCG_GOBLIN_WEATHER_MACHINE"    },
    }},
    { SENDER_HFI,  {
        { "Path of Illidan",                             { 38233 },        false, true,  "TCG_PATH_OF_ILLIDAN"           },
        { "D.I.S.C.O.",                                  { 38301 },        false, false, "TCG_DISCO"                     },
        { "Soul-Trader Beacon",                          { 38050 },        false, false, "TCG_SOUL_TRADER_BEACON"        },  // permanent companion
    }},
    { SENDER_DOW,  {
        { "Party G.R.E.N.A.D.E.",                        { 38577 },        false, true,  "TCG_PARTY_GRENADE"             },
        { "The Flag of Ownership",                       { 38578 },        false, false, "TCG_FLAG_OF_OWNERSHIP"         },
        { "Big Battle Bear",                             { 38576 },        false, false, "TCG_BIG_BATTLE_BEAR"           },
    }},
    { SENDER_BOG,  {
        { "Sandbox Tiger",                               { 45047 },        false, true,  "TCG_SANDBOX_TIGER"             },
        { "Epic Purple Shirt",                           { 45037 },        false, false, "TCG_EPIC_PURPLE_SHIRT"         },
        { "Foam Sword Rack",                             { 45063 },        false, false, "TCG_FOAM_SWORD_RACK"           },
    }},
    { SENDER_FOH,  {
        { "Path of Cenarius",                            { 46779 },        false, true,  "TCG_PATH_OF_CENARIUS"          },
        { "Ogre Pinata",                                 { 46780 },        false, false, "TCG_OGRE_PINATA"               },
        { "Magic Rooster Egg",                           { 46778 },        false, false, "TCG_MAGIC_ROOSTER_EGG"         },
    }},
    { SENDER_SW,   {
        { "Scourgewar Mini-Mount",                       { 49288, 49289 }, true,  true,  "TCG_SCOURGEWAR_MINIMOUNT"      },
        { "Tuskarr Kite",                                { 49287 },        false, false, "TCG_TUSKARR_KITE"              },
        { "Spectral Tiger Cub",                          { 49343 },        false, false, "TCG_SPECTRAL_TIGER_CUB"        },
    }},
    { SENDER_WG,   {
        { "Landro's Gift Box",                          { 54218 },        false, false, "TCG_LANDROS_GIFT_BOX"          },
        { "Instant Statue Pedestal",                     { 54212 },        false, false, "TCG_INSTANT_STATUE_PEDESTAL"   },
        { "Blazing Hippogryph",                          { 54069 },        false, false, "TCG_BLAZING_HIPPOGRYPH"        },
    }},
    { SENDER_IC,   {
        { "Paint Bomb",                                  { 54455 },        false, true,  "TCG_PAINT_BOMB"                },
        { "Ethereal Portal",                             { 54452 },        false, false, "TCG_ETHEREAL_PORTAL"           },
        { "Wooly White Rhino",                           { 54068 },        false, false, "TCG_WOOLY_WHITE_RHINO"         },
    }},
    { SENDER_PR,   {
        { "Tabard of Frost",                             { 23709 },        false, false, "TCG_TABARD_OF_FROST"           },
        { "Perpetual Purple Firework",                   { 23714 },        false, false, "TCG_PERPETUAL_PURPLE_FIREWORK" },
        { "Carved Ogre Idol",                            { 23716 },        false, false, "TCG_CARVED_OGRE_IDOL"          },
        { "Tabard of the Arcane",                        { 38310 },        false, false, "TCG_TABARD_OF_THE_ARCANE"      },
        { "Tabard of Brilliance",                        { 38312 },        false, false, "TCG_TABARD_OF_BRILLIANCE"      },
        { "Tabard of the Defender",                      { 38314 },        false, false, "TCG_TABARD_OF_THE_DEFENDER"    },
        { "Tabard of Fury",                              { 38313 },        false, false, "TCG_TABARD_OF_FURY"            },
        { "Tabard of Nature",                            { 38309 },        false, false, "TCG_TABARD_OF_NATURE"          },
        { "Tabard of the Void",                          { 38311 },        false, false, "TCG_TABARD_OF_THE_VOID"        },
        { "Landro's Pet Box",                           { 50301 },        false, false, "TCG_LANDROS_PET_BOX"           },
    }},
};

static const std::map<uint32, std::string> EXPANSION_NAMES =
{
    { SENDER_HOA,  "Heroes of Azeroth"        },
    { SENDER_TDP,  "Through the Dark Portal"  },
    { SENDER_FOO,  "Fires of Outland"         },
    { SENDER_MOTL, "March of the Legion"      },
    { SENDER_SOTB, "Servants of the Betrayer" },
    { SENDER_HFI,  "Hunt for Illidan"         },
    { SENDER_DOW,  "Drums of War"             },
    { SENDER_BOG,  "Blood of Gladiators"      },
    { SENDER_FOH,  "Fields of Honor"          },
    { SENDER_SW,   "Scourgewar"               },
    { SENDER_WG,   "Wrathgate"                },
    { SENDER_IC,   "Icecrown"                 },
    { SENDER_PR,   "Points Redemption"        },
};

// ============================================================
//  Worldwide Invitational catalog  (Edward Cairn / Ian Drake)
//
//  These two vendors handle Tyrael's Hilt exclusively — the reward
//  granted at the 2008 Worldwide Invitational event in Paris.
// ============================================================
static const std::vector<TCGItem> TYRAELS_CATALOG =
{
    { "Tyrael's Hilt", { 39656 }, false, false, "WWI_TYRAELS_HILT" },
};

static const std::map<uint32, std::string> PROMO_CATEGORY_NAMES =
{
    { SENDER_PROMO_MURLOC,  "Murloc Companions"             },
    { SENDER_PROMO_CLASSIC, "Classic & Special Promotions"  },
    { SENDER_PROMO_STORE,   "Blizzard Store"                },
    { SENDER_PROMO_EVENTS,  "Special Events & Tournaments"  },
};

// ============================================================
//  Blizzcon vendor catalog  (Ransin Donner / Zas'Tysh)
// ============================================================
static const std::vector<TCGItem> BLIZZCON_CATALOG =
{
    { "Murky (Blue Murloc Egg)", { 20371 }, false, false, "BLIZZCON_MURKY"             },
    { "Murloc Costume",          { 33079 }, false, false, "BLIZZCON_MURLOC_COSTUME"    },
    { "Big Blizzard Bear",       { 43599 }, false, false, "BLIZZCON_BIG_BLIZZARD_BEAR" },
};

// ============================================================
//  Promo vendor catalog  (Garel Redrock / Tharl Stonebleeder)
// ============================================================
static const std::map<uint32, std::vector<TCGItem>> PROMO_CATALOG =
{
    { SENDER_PROMO_MURLOC,  {
        { "Gurky (Pink Murloc Egg)",   { 22114 }, false, false, "PROMO_GURKY"              },
        { "Orange Murloc Egg",         { 20651 }, false, false, "PROMO_ORANGE_MURLOC_EGG"  },
        { "White Murloc Egg",          { 22780 }, false, false, "PROMO_WHITE_MURLOC_EGG"   },
        { "Heavy Murloc Egg",          { 46802 }, false, false, "PROMO_HEAVY_MURLOC_EGG"   },
        { "Murkimus' Little Spear",    { 45180 }, false, false, "PROMO_MURKIMUS_SPEAR"     },
    }},
    { SENDER_PROMO_CLASSIC, {
        { "Zergling Leash",            { 13582 }, false, false, "PROMO_ZERGLING_LEASH"     },
        { "Panda Collar",              { 13583 }, false, false, "PROMO_PANDA_COLLAR"       },
        { "Diablo Stone",              { 13584 }, false, false, "PROMO_DIABLO_STONE"       },
        { "Netherwhelp's Collar",      { 25535 }, false, false, "PROMO_NETHERWHELP"        },
        { "Frosty's Collar",           { 39286 }, false, false, "PROMO_FROSTYS_COLLAR"     },
        { "Warbot Ignition Key",        { 46767 }, false, false, "PROMO_WARBOT_KEY"         },
        { "Red War Fuel",              { 46766 }, false, true,  "PROMO_RED_WAR_FUEL",  WARBOT_PET_SPELL },
        { "Blue War Fuel",             { 46765 }, false, true,  "PROMO_BLUE_WAR_FUEL", WARBOT_PET_SPELL },
    }},
    { SENDER_PROMO_STORE,   {
        { "Enchanted Onyx",            { 48527 }, false, false, "PROMO_ENCHANTED_ONYX"     },
        { "Core Hound Pup",            { 49646 }, false, false, "PROMO_CORE_HOUND_PUP"     },
        { "Gryphon Hatchling",         { 49662 }, false, false, "PROMO_GRYPHON_HATCHLING"  },
        { "Wind Rider Cub",            { 49663 }, false, false, "PROMO_WIND_RIDER_CUB"     },
        { "Pandaren Monk",             { 49665 }, false, false, "PROMO_PANDAREN_MONK"      },
    }},
    { SENDER_PROMO_EVENTS,  {
        { "Lil' Phylactery",           { 49693 }, false, false, "PROMO_LIL_PHYLACTERY"     },
        { "Lil' XT",                   { 54847 }, false, false, "PROMO_LIL_XT"             },
        { "Mini Thor",                 { 56806 }, false, false, "PROMO_MINI_THOR"           },
        { "Onyxian Whelpling",         { 49362 }, false, false, "PROMO_ONYXIAN_WHELPLING"  },
    }},
};

// ============================================================
//  C O N F I G   H E L P E R S
// ============================================================

static int GetVendorMode()
{
    int mode = sConfigMgr->GetOption<int>("TCGVendors.Mode", MODE_BLIZZLIKE);
    if (mode < MODE_DISABLED || mode > MODE_ITEM_CODE)
    {
        LOG_WARN("module",
            "mod-tcg-vendors: TCGVendors.Mode has unrecognised value {} — "
            "falling back to Mode 2 (Blizz-like).", mode);
        return MODE_BLIZZLIKE;
    } else {
        return mode;
    }
}
// Returns true when Landro's Gift Box and Pet Box should be treated
// as consumable (multi-redeemable).  Reads TCGVendors.LandroBoxesMultiRedeem.
static bool GetLandroBoxIsConsumable()
{
    return sConfigMgr->GetOption<bool>("TCGVendors.LandroBoxesMultiRedeem", false);
}

// Convenience: given a TCGItem or RewardGroup entry, resolves whether
// the box override applies to it.
static bool IsItemConsumable(uint32 redemptionKey, bool baseConsumable)
{
    if (redemptionKey == ITEM_LANDROS_GIFT_BOX || redemptionKey == ITEM_LANDROS_PET_BOX)
        return GetLandroBoxIsConsumable();
    return baseConsumable;
}

// ============================================================
//  H E L P E R   F U N C T I O N S
// ============================================================

// Helper: Map itemId to vendor name
// Helper: Map an item entry to the NPC vendor name(s) that handle it.
//
// Rather than maintaining a hardcoded list, we scan the three catalogs
// directly — this stays automatically correct as catalogs change.
//
// Blizzcon and Promo items are sold at paired Alliance/Horde vendors;
// we list both so the stationery is useful regardless of the reader's
// faction.
static std::string GetVendorForItem(uint32 itemId)
{
    // TCG expansion items — Landro Longshot, Booty Bay
    for (auto const& [sender, items] : LANDRO_CATALOG)
        for (auto const& tcgItem : items)
            for (uint32 e : tcgItem.entries)
                if (e == itemId)
                    return "Landro Longshot in Booty Bay";

    // Blizzcon promotional items — Ransin Donner (Alliance) / Zas'Tysh (Horde)
    for (auto const& tcgItem : BLIZZCON_CATALOG)
        for (uint32 e : tcgItem.entries)
            if (e == itemId)
                return "Ransin Donner in Ironforge (Alliance) or Zas'Tysh in Orgrimmar (Horde)";

    // Additional promotional items — Garel Redrock (Alliance) / Tharl Stonebleeder (Horde)
    for (auto const& [sender, items] : PROMO_CATALOG)
        for (auto const& tcgItem : items)
            for (uint32 e : tcgItem.entries)
                if (e == itemId)
                    return "Garel Redrock in Ironforge (Alliance) or Tharl Stonebleeder in Orgrimmar (Horde)";

    // WorldWide Invitational — Ian Drake (Alliance) / Edward Cairn (Horde)
    for (auto const& tcgItem : TYRAELS_CATALOG)
        for (uint32 e : tcgItem.entries)
            if (e == itemId)
                return "Ian Drake in Stormwind (Alliance) or Edward Cairn in Undercity (Horde)";

    // Fallback — should not be reached for any configured item
    LOG_WARN("module",
        "mod-tcg-vendors: GetVendorForItem: item {} not found in any catalog. "
        "Check TCGVendors.BossDrop.ItemIds.", itemId);
    return "the TCG vendor NPC";
}

// Helper: Get item name from item ID (fallback to numeric ID string if not found)
static std::string GetItemName(uint32 itemId)
{
    if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId))
        return proto->Name1;
    return std::to_string(itemId);
}

// Helper: Build the flavor text message for a stationery drop
static std::string BuildStationeryText(const std::string& bossName,
                                       const std::string& itemName,
                                       const std::string& code,
                                       uint32             itemId)
{
    std::string vendor = GetVendorForItem(itemId);
    return "Congratulations! You have defeated " + bossName + ".\n\n"
           "Enclosed is a code redeemable for: " + itemName + ".\n\n"
           "Code:\n" + code + "\n\n"
           "To redeem this code, visit " + vendor + " and speak with the TCG vendor NPC.\n\n"
           "This code is single-use. Thank you for playing!";
}

// Helper: Create a stationery item (9311) with text properly set for
// in-inventory readability.
//
// Two things are required for the client to show an item as right-click-
// readable and to query its text:
//
//   1. item_instance.text  — the actual text content, read by the server
//      when the client queries it.  We write this directly via SQL after
//      SaveToDB as a safety net, since Item::SetText may not be included
//      in SaveToDB in all fork variants.
//
//   2. ITEM_FIELD_FLAG_READABLE (0x00000200) — set on the item's flags field.
//      When present, the client shows the right-click-to-read option and
//      sends CMSG_ITEM_TEXT_QUERY.  The server responds with item_instance.text
//      looked up by the item's own GUID.  This fork has no separate
//      ITEM_FIELD_ITEM_TEXT_ID update field; the readable flag is sufficient.
//
// SetText alone is insufficient without the readable flag.
static void StampItemText(Item* item, Player* owner, const std::string& text)
{
    // 1. Set in-memory text — populates m_text for SaveToDB.
    item->SetText(text);

    // 2. Set ITEM_FIELD_FLAG_READABLE (0x00000200) on the item's flags field.
    //    This is what tells the client to show the right-click-to-read option
    //    and to send CMSG_ITEM_TEXT_QUERY.  The server responds to that query
    //    with item_instance.text looked up by the item's GUID.
    //    There is no separate ITEM_FIELD_ITEM_TEXT_ID in this fork.
    item->SetFlag(ITEM_FIELD_FLAGS, ITEM_FIELD_FLAG_READABLE);

    // 3. Mark dirty so the flag update is sent to the client on the next
    //    update cycle and written to item_instance on the next autosave.
    if (owner)
        item->SetState(ITEM_CHANGED, owner);
}

static Item* CreateStationeryWithText(Player* owner, const std::string& text)
{
    Item* item = Item::CreateItem(9311, 1, owner);
    if (!item)
        return nullptr;
    StampItemText(item, nullptr, text);  // no owner yet — SaveToDB will persist
    return item;
}

// After a SaveToDB + CommitTransaction call, write the text directly to
// item_instance.text.  This is a safety net for forks where SaveToDB does
// not include the m_text field in its INSERT/UPDATE statement.
static void DirectWriteItemText(uint32 itemGuidLow, const std::string& text)
{
    std::string escaped = text;
    CharacterDatabase.EscapeString(escaped);
    CharacterDatabase.Execute(
        "UPDATE item_instance SET text = '{}' WHERE guid = {}",
        escaped, itemGuidLow);
}

// Helper: Generate a random code (alphanumeric, 12 chars)
static std::string GenerateRandomCode()
{
    static const char alphanum[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
    std::string raw;
    for (int i = 0; i < 16; ++i)
        raw += alphanum[urand(0, sizeof(alphanum) - 2)];
    // Format as XXXX-XXXX-XXXX-XXXX
    return raw.substr(0,4) + "-" + raw.substr(4,4) + "-" + raw.substr(8,4) + "-" + raw.substr(12,4);
}

// Helper: Find reward_group key for an itemId
static std::string GetRewardGroupForItem(uint32 itemId)
{
    for (const auto& pair : REWARD_GROUPS)
    {
        for (uint32 entry : pair.second.itemEntries)
        {
            if (entry == itemId)
                return pair.first;
        }
    }
    return "";
}

// Helper: Insert code into the module's code tracking table
static void InsertCodeToDatabase(const std::string& code, const std::string& rewardGroup)
{
    std::string escCode = code;
    std::string escGroup = rewardGroup;
    CharacterDatabase.EscapeString(escCode);
    CharacterDatabase.EscapeString(escGroup);
    CharacterDatabase.Execute(
        "INSERT INTO account_tcg_codes (code, reward_group, redeemed, account_id, character_guid, redeemed_date) "
        "VALUES ('{}', '{}', 0, NULL, NULL, NULL)",
        escCode, escGroup);
}

static bool HasRedeemed(uint32 guid, uint32 itemEntry)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT 1 FROM character_tcg_redeemed WHERE guid = {} AND item_entry = {}",
        guid, itemEntry);
    return result != nullptr;
}

static void MarkRedeemed(uint32 guid, uint32 itemEntry)
{
    CharacterDatabase.Execute(
        "INSERT IGNORE INTO character_tcg_redeemed (guid, item_entry) VALUES ({}, {})",
        guid, itemEntry);
}


    uint32 GetPromoStackSize(uint32 itemEntry)
    {
        switch (itemEntry)
        {
            case 46766: // Red War Fuel
            case 46765: // Blue War Fuel
                return 5;

            default:
                return 1;
        }
    }

// Resolve which items to physically hand to the player, handling
// the faction-mount split.
static std::vector<uint32> ResolveItems(Player* player,
                                        const std::vector<uint32>& entries,
                                        bool factionMount)
{
    if (factionMount && entries.size() >= 2)
    {
        bool isHorde = (player->GetTeamId() == TEAM_HORDE);
        return { entries[isHorde ? 0 : 1] };
    }
    return entries;
}

// Attempt to place one item into the player's bags.
// Returns true if it fit, false if bags are full.
static bool TryAddItemToBags(Player* player, uint32 entry)
{
    ItemPosCountVec dest;
    return player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, entry, GetPromoStackSize(entry)) == EQUIP_ERR_OK;
}

// Mail a list of items to the player as a fallback when bags are full.
// Each item is sent in a separate mail so the player can retrieve them
// individually from the mailbox without needing free bag space for all
// of them at once.
static void MailItemsToPlayer(Player* player, Creature* creature,
                              const std::vector<uint32>& entries,
                              const std::string& displayName)
{
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    for (uint32 entry : entries)
    {
        Item* item = Item::CreateItem(entry, GetPromoStackSize(entry), player);
        if (!item)
        {
            LOG_ERROR("module",
                "mod-tcg-vendors: Failed to create item {} for mail fallback "
                "(player guid {}).", entry, player->GetGUID().GetCounter());
            continue;
        }
        item->SaveToDB(trans);

        MailDraft(
            "Your Reward: " + displayName,
            "Your bags were full when you redeemed your item at " +
            std::string(creature->GetName()) + ", so it was mailed to you instead.\n\n"
            "Here you go!  Have a nice day!")
            .AddItem(item)
            .SendMailTo(trans,
                MailReceiver(player, player->GetGUID().GetCounter()),
                MailSender(creature));
    }

    CharacterDatabase.CommitTransaction(trans);
}

// Return values from TryDeliverItem — lets callers send the right whisper
// without needing to re-query the database or add out-parameters.
enum DeliveryResult
{
    DELIVERY_FAILED,      // Already redeemed — nothing was given, whisper already sent
    DELIVERY_BAGS,        // Items placed directly into player's bags
    DELIVERY_MAIL,        // Bags were full — items mailed, whisper already sent
};

// Central delivery function used by all modes and both NPC classes.
static DeliveryResult TryDeliverItem(Player*                    player,
                                     Creature*                  creature,
                                     const std::vector<uint32>& allEntries,
                                     uint32                     redemptionKey,
                                     bool                       factionMount,
                                     bool                       consumable,
                                     const std::string&         displayName)
{
    if (!consumable && HasRedeemed(player->GetGUID().GetCounter(), redemptionKey))
    {
        creature->Whisper(
            "Your character has already received \"" + displayName + "\".",
            LANG_UNIVERSAL, player);
        return DELIVERY_FAILED;
    }

    std::vector<uint32> toGive = ResolveItems(player, allEntries, factionMount);

    // Check whether every item in the set fits in bags right now.
    bool bagsFull = false;
    for (uint32 entry : toGive)
    {
        if (!TryAddItemToBags(player, entry))
        {
            bagsFull = true;
            break;
        }
    }

    if (bagsFull)
    {
        MailItemsToPlayer(player, creature, toGive, displayName);
        if (!consumable)
            MarkRedeemed(player->GetGUID().GetCounter(), redemptionKey);

        creature->Whisper(
            "Your bags are full! \"" + displayName + "\" has been mailed to you. "
            "Visit any mailbox to collect it.",
            LANG_UNIVERSAL, player);
        return DELIVERY_MAIL;
    }

    // Bags have room — deliver directly.
    for (uint32 entry : toGive)
        player->AddItem(entry, GetPromoStackSize(entry));

    if (!consumable)
        MarkRedeemed(player->GetGUID().GetCounter(), redemptionKey);

    return DELIVERY_BAGS;
}

// ============================================================
//  C O D E   R E D E M P T I O N   (Mode 2)
// ============================================================

static std::string NormalizeCode(const std::string& raw)
{
    std::string code = raw;
    code.erase(0, code.find_first_not_of(" \t\r\n"));
    auto last = code.find_last_not_of(" \t\r\n");
    if (last != std::string::npos)
        code.erase(last + 1);
    std::transform(code.begin(), code.end(), code.begin(), ::toupper);
    return code;
}

// Validates XXXX-XXXX-XXXX-XXXX using the same unambiguous charset
// as tools/generate_codes.py (A-Z excl. I,L,O + 2-9 excl. 0,1).
static bool IsValidCodeFormat(const std::string& code)
{
    if (code.size() != 19)
        return false;
    if (code[4] != '-' || code[9] != '-' || code[14] != '-')
        return false;

    static const std::string VALID_CHARS = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
    for (size_t i = 0; i < code.size(); ++i)
    {
        if (i == 4 || i == 9 || i == 14)
            continue;
        if (VALID_CHARS.find(code[i]) == std::string::npos)
            return false;
    }
    return true;
}

static void HandleCodeRedemption(Player*            player,
                                 Creature*          creature,
                                 const std::string& rawCode)
{
    std::string code = NormalizeCode(rawCode);

    // ---- 1. Format check
    if (!IsValidCodeFormat(code))
    {
        creature->Whisper(
            "That doesn't look like a valid code. "
            "Codes follow the format XXXX-XXXX-XXXX-XXXX. "
            "Please check your code and try again.",
            LANG_UNIVERSAL, player);
        return;
    }

    // ---- 2. Database lookup
    std::string escapedCode = code;
    CharacterDatabase.EscapeString(escapedCode);

    QueryResult result = CharacterDatabase.Query(
        "SELECT reward_group, redeemed "
        "FROM account_tcg_codes "
        "WHERE code = '{}'",
        escapedCode);

    if (!result)
    {
        creature->Whisper(
            "That code was not recognised. "
            "Please double-check the code and try again.",
            LANG_UNIVERSAL, player);
        return;
    }

    Field*      fields      = result->Fetch();
    std::string rewardGroup = fields[0].Get<std::string>();
    bool        redeemed    = fields[1].Get<bool>();

    // ---- 3. Already used?
    if (redeemed)
    {
        creature->Whisper(
            "That code has already been redeemed. Each code may only be used once.",
            LANG_UNIVERSAL, player);
        return;
    }

    // ---- 4. Reward group lookup
    auto groupIt = REWARD_GROUPS.find(rewardGroup);
    if (groupIt == REWARD_GROUPS.end())
    {
        creature->Whisper(
            "Your code is valid but references an unknown reward. "
            "Please contact a Game Master for assistance.",
            LANG_UNIVERSAL, player);
        LOG_ERROR("module",
            "mod-tcg-vendors: Code '{}' references unknown reward_group '{}'. "
            "Check the account_tcg_codes table.",
            code, rewardGroup);
        return;
    }

    const RewardGroup& group    = groupIt->second;
    uint32             redeemKey = group.itemEntries[0];

    // ---- 5. Resolve consumable flag (box override applies here)
    bool consumable = IsItemConsumable(redeemKey, group.isConsumable);

    // ---- 6. Attempt delivery
    // Mark the code as used BEFORE delivering items.  If the server were
    // to crash between the UPDATE and AddItem, a GM can verify via the
    // table and use the GM browse path to re-deliver.  This ordering
    // ensures a code cannot be re-used after a partial delivery.
    CharacterDatabase.Execute(
        "UPDATE account_tcg_codes "
        "SET redeemed = 1, account_id = {}, character_guid = {}, redeemed_date = NOW() "
        "WHERE code = '{}'",
        player->GetSession()->GetAccountId(),
        player->GetGUID().GetCounter(),
        escapedCode);

    DeliveryResult deliveryResult = TryDeliverItem(player, creature,
                                                   group.itemEntries, redeemKey,
                                                   group.factionMount, consumable,
                                                   group.displayName);

    if (deliveryResult == DELIVERY_BAGS)
    {
        creature->Whisper(
            "Code accepted! \"" + group.displayName + "\" has been added to your inventory. Enjoy!",
            LANG_UNIVERSAL, player);
    }
}

// ============================================================
//  Item-specific code redemption (Mode 3)
// ============================================================
static void HandleItemSpecificCodeRedemption(Player*            player,
                                              Creature*          creature,
                                              const std::string& rawCode,
                                              const std::string& expectedGroupKey,
                                              const std::string& itemDisplayName)
{
    std::string code = NormalizeCode(rawCode);

    // --- Format check
    if (!IsValidCodeFormat(code))
    {
        creature->Whisper(
            "That doesn't look like a valid code. "
            "Codes follow the format XXXX-XXXX-XXXX-XXXX. "
            "Please check your code and try again.",
            LANG_UNIVERSAL, player);
        return;
    }

    // --- Database lookup
    std::string escapedCode = code;
    CharacterDatabase.EscapeString(escapedCode);

    QueryResult codeResult = CharacterDatabase.Query(
        "SELECT reward_group, redeemed "
        "FROM account_tcg_codes "
        "WHERE code = '{}'",
        escapedCode);

    if (!codeResult)
    {
        creature->Whisper(
            "That code was not recognised. "
            "Please double-check the code and try again.",
            LANG_UNIVERSAL, player);
        return;
    }

    Field*      fields      = codeResult->Fetch();
    std::string rewardGroup = fields[0].Get<std::string>();
    bool        redeemed    = fields[1].Get<bool>();

    // --- Already used?
    if (redeemed)
    {
        creature->Whisper(
            "That code has already been redeemed. Each code may only be used once.",
            LANG_UNIVERSAL, player);
        return;
    }

    // --- Code must match the item the player selected
    if (rewardGroup != expectedGroupKey)
    {
        creature->Whisper(
            "That code is not valid for \"" + itemDisplayName + "\". "
            "Please check that you are entering the correct code for this item.",
            LANG_UNIVERSAL, player);
        return;
    }

    // --- Reward group lookup (should always succeed here)
    auto groupIt = REWARD_GROUPS.find(rewardGroup);
    if (groupIt == REWARD_GROUPS.end())
    {
        creature->Whisper(
            "Your code references an unknown reward. "
            "Please contact a Game Master for assistance.",
            LANG_UNIVERSAL, player);
        LOG_ERROR("module",
            "mod-tcg-vendors: Code '{}' references unknown reward_group '{}'. "
            "Check the account_tcg_codes table.",
            code, rewardGroup);
        return;
    }

    const RewardGroup& group    = groupIt->second;
    uint32             redeemKey = group.itemEntries[0];
    bool               consumable = IsItemConsumable(redeemKey, group.isConsumable);

    // --- Mark code used BEFORE delivering
    CharacterDatabase.Execute(
        "UPDATE account_tcg_codes "
        "SET redeemed = 1, account_id = {}, character_guid = {}, redeemed_date = NOW() "
        "WHERE code = '{}'",
        player->GetSession()->GetAccountId(),
        player->GetGUID().GetCounter(),
        escapedCode);

    DeliveryResult deliveryResult = TryDeliverItem(player, creature,
                                                   group.itemEntries, redeemKey,
                                                   group.factionMount, consumable,
                                                   group.displayName);

    if (deliveryResult == DELIVERY_BAGS)
    {
        creature->Whisper(
            "Code accepted! \"" + group.displayName + "\" has been added to your inventory. Enjoy!",
            LANG_UNIVERSAL, player);
    }
}

// ============================================================
//  B R O W S E   M E N U   H E L P E R S
// ============================================================

// Build a gossip item label, appending "[Already Redeemed]" or
// "[Unlimited]" hints as appropriate.
static std::string BuildItemLabel(const std::string& name,
                                  uint32             redemptionKey,
                                  bool               consumable,
                                  uint32             playerGuid)
{
    if (consumable)
        return name + " [Unlimited]";

    if (HasRedeemed(playerGuid, redemptionKey))
        return name + " [Already Redeemed]";

    return name;
}

// Populate and send the expansion set list for Landro's browse menu.
static void ShowExpansionList(Player* player, Creature* creature, uint32 npcTextId)
{
    ClearGossipMenuFor(player);
    for (auto const& [senderVal, name] : EXPANSION_NAMES)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, name, SENDER_MAIN, senderVal);
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "< Back", SENDER_MAIN, 0);
    SendGossipMenuFor(player, npcTextId, creature->GetGUID());
}

// Populate and send the item list for one expansion set.
// When the browsing player has GM mode active, every item is presented as a
// player-name text input (for GM delivery) rather than a confirmation dialog.
static void ShowExpansionItems(Player* player, Creature* creature,
                               uint32 sender, uint32 npcTextId)
{
    auto catalogIt = LANDRO_CATALOG.find(sender);
    if (catalogIt == LANDRO_CATALOG.end())
    {
        ShowExpansionList(player, creature, npcTextId);
        return;
    }

    bool isGM = player->IsGameMaster();
    int  mode = GetVendorMode();
    ClearGossipMenuFor(player);
    const std::vector<TCGItem>& items = catalogIt->second;
    uint32 playerGuid = player->GetGUID().GetCounter();

    for (uint32 i = 0; i < static_cast<uint32>(items.size()); ++i)
    {
        uint32 redeemKey  = items[i].entries[0];
        bool   consumable = IsItemConsumable(redeemKey, items[i].isConsumable);

        if (isGM)
        {
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] " + items[i].displayName,
                sender, i + 1,
                "Enter character name to deliver \"" + items[i].displayName + "\" to:",
                0, true);
        }
        else
        {
            bool        redeemed = !consumable && HasRedeemed(playerGuid, redeemKey);
            std::string label    = BuildItemLabel(items[i].displayName, redeemKey,
                                                  consumable, playerGuid);
            if (redeemed)
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, label, sender, i + 1);
            }
            else if (mode == MODE_BLIZZLIKE || mode == MODE_ITEM_CODE)
            {
                AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label,
                    sender, i + 1,
                    "Enter your redemption code for \"" + items[i].displayName + "\":",
                    0, true);
            }
            else
            {
                AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label,
                    sender, i + 1,
                    "Receive \"" + items[i].displayName + "\"?",
                    0, false);
            }
        }
    }

    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "< Back to set list", SENDER_MAIN, 0);
    SendGossipMenuFor(player, npcTextId, creature->GetGUID());
}

// Handle a browse-menu item selection (used by both NPC classes).
// Returns true if handled.
static bool HandleBrowseSelect(Player* player, Creature* creature,
                               uint32 action,
                               const std::vector<TCGItem>& items)
{
    uint32 idx = action - 1;
    if (idx >= static_cast<uint32>(items.size()))
    {
        CloseGossipMenuFor(player);
        return true;
    }

    const TCGItem& item      = items[idx];
    uint32         redeemKey = item.entries[0];
    bool           consumable = IsItemConsumable(redeemKey, item.isConsumable);

    DeliveryResult result = TryDeliverItem(player, creature,
                                           item.entries, redeemKey,
                                           item.factionMount, consumable,
                                           item.displayName);

    if (result == DELIVERY_BAGS)
        creature->Whisper("Item granted. Enjoy!", LANG_UNIVERSAL, player);
    return true;
}

// ============================================================
//  GM delivery helpers
// ============================================================

// Returns true on successful delivery (or whisper sent).
// Returns false when the item has already been redeemed and forceOverride is false —
// the caller should then present the SENDER_GM_FORCE override confirmation dialog.
static bool HandleGMDelivery(Player*                    gm,
                             Creature*                  creature,
                             const std::string&         targetName,
                             const std::vector<uint32>& allEntries,
                             bool                       factionMount,
                             bool                       consumable,
                             const std::string&         displayName,
                             bool                       forceOverride)
{
    if (targetName.empty())
    {
        creature->Whisper("Please enter a character name.", LANG_UNIVERSAL, gm);
        return true;
    }

    Player* targetPlayer = ObjectAccessor::FindPlayerByName(targetName);

    ObjectGuid::LowType targetGuid;
    uint8               targetRace;

    if (targetPlayer)
    {
        targetGuid = targetPlayer->GetGUID().GetCounter();
        targetRace = targetPlayer->getRace();
    }
    else
    {
        std::string escapedName = targetName;
        CharacterDatabase.EscapeString(escapedName);

        QueryResult charResult = CharacterDatabase.Query(
            "SELECT guid, race FROM characters WHERE name = '{}'", escapedName);

        if (!charResult)
        {
            creature->Whisper(
                "Character \"" + targetName + "\" was not found. "
                "Check the spelling and try again.",
                LANG_UNIVERSAL, gm);
            return true;
        }

        Field* charFields = charResult->Fetch();
        targetGuid = charFields[0].Get<uint32>();
        targetRace = charFields[1].Get<uint8>();
    }

    // --- Resolve faction-aware items using the target's race, not the GM's ---
    std::vector<uint32> toGive;
    if (factionMount && allEntries.size() >= 2)
    {
        TeamId team = Player::TeamIdForRace(targetRace);
        toGive = { allEntries[team == TEAM_HORDE ? 0 : 1] };
    }
    else
    {
        toGive = allEntries;
    }

    // --- Already-redeemed guard ---
    // If forceOverride is false, return false so the caller can show the
    // SENDER_GM_FORCE confirmation dialog.  If forceOverride is true we
    // skip this check entirely and re-deliver regardless.
    if (!forceOverride && !consumable && HasRedeemed(targetGuid, toGive[0]))
        return false;

    // --- Mail all items directly — no bag-space check ---
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    for (uint32 entry : toGive)
    {
        Item* item = Item::CreateItem(entry, GetPromoStackSize(entry), targetPlayer);
        if (!item)
        {
            LOG_ERROR("module",
                "mod-tcg-vendors: GM delivery failed to create item {} "
                "for character {} (guid {}).",
                entry, targetName, targetGuid);
            continue;
        }
        item->SaveToDB(trans);
        MailDraft(
            "Item Delivery: " + displayName,
            "A Game Master has delivered \"" + displayName + "\" to your character.\n"
            "Visit any mailbox to collect it.")
            .AddItem(item)
            .SendMailTo(trans,
                targetPlayer ? MailReceiver(targetPlayer, targetGuid)
                             : MailReceiver(targetGuid),
                MailSender(creature));
    }
    CharacterDatabase.CommitTransaction(trans);

    // --- Record the delivery for unique items (keyed on target, not GM) ---
    if (!consumable)
        MarkRedeemed(targetGuid, toGive[0]);

    creature->Whisper(
        "[GM] \"" + displayName + "\" has been mailed to \"" + targetName + "\".",
        LANG_UNIVERSAL, gm);
    return true;
}

static void HandleGMClearFlags(Player*            gm,
                               Creature*          creature,
                               const std::string& targetName)
{
    if (targetName.empty())
    {
        creature->Whisper("Please enter a character name.", LANG_UNIVERSAL, gm);
        return;
    }

    std::string escapedName = targetName;
    CharacterDatabase.EscapeString(escapedName);

    QueryResult charResult = CharacterDatabase.Query(
        "SELECT guid FROM characters WHERE name = '{}'", escapedName);

    if (!charResult)
    {
        creature->Whisper(
            "Character \"" + targetName + "\" was not found. "
            "Check the spelling and try again.",
            LANG_UNIVERSAL, gm);
        return;
    }

    ObjectGuid::LowType targetGuid = charResult->Fetch()[0].Get<uint32>();

    // Count existing records so the whisper is informative
    QueryResult countResult = CharacterDatabase.Query(
        "SELECT COUNT(*) FROM character_tcg_redeemed WHERE guid = {}", targetGuid);

    uint32 count = countResult ? countResult->Fetch()[0].Get<uint32>() : 0;

    CharacterDatabase.Execute(
        "DELETE FROM character_tcg_redeemed WHERE guid = {}", targetGuid);

    if (count == 0)
    {
        creature->Whisper(
            "[GM] \"" + targetName + "\" had no TCG redemption records to clear.",
            LANG_UNIVERSAL, gm);
    }
    else
    {
        creature->Whisper(
            "[GM] Cleared " + std::to_string(count) +
            " redemption record(s) for \"" + targetName + "\". "
            "They may now re-receive those items.",
            LANG_UNIVERSAL, gm);
    }
}

// ============================================================
//  GM Send-Code helpers
//
//  "Send a code to a player" generates a fresh redemption code for a
//  chosen item, inserts it into account_tcg_codes as unredeemed, and
//  mails the target a readable stationery item with WoW-flavoured text
//  containing the code and vendor directions.
//
//  The item is NOT delivered directly — the player redeems it at the
//  appropriate vendor NPC using the mailed code.
// ============================================================

static std::string BuildGMCodeText(const std::string& targetName,
                                    const std::string& itemName,
                                    const std::string& code,
                                    uint32             itemId)
{
    std::string vendor = GetVendorForItem(itemId);
    return "Greetings, " + targetName + "!\n\n"
           "A Game Master has arranged a special gift for you!\n\n"
           "You have been awarded a redemption code for:\n"
           + itemName + "\n\n"
           "Your code:\n"
           + code + "\n\n"
           "To claim your reward, visit " + vendor + " and enter this code "
           "when prompted.\n\n"
           "This code is single-use and will be bound to your account upon "
           "redemption.  Keep it safe!\n\n"
           "Good luck on your adventures in Azeroth!";
}

static void HandleGMSendCode(Player*            gm,
                              Creature*          creature,
                              const std::string& targetName,
                              const std::string& displayName,
                              const std::string& rewardGroupKey,
                              uint32             itemId)
{
    if (targetName.empty())
    {
        creature->Whisper("Please enter a character name.", LANG_UNIVERSAL, gm);
        return;
    }

    if (REWARD_GROUPS.find(rewardGroupKey) == REWARD_GROUPS.end())
    {
        creature->Whisper(
            "[GM] Unknown reward group for this item — cannot generate a code.",
            LANG_UNIVERSAL, gm);
        LOG_ERROR("module",
            "mod-tcg-vendors: HandleGMSendCode: unknown reward group '{}'.",
            rewardGroupKey);
        return;
    }

    Player* targetPlayer = ObjectAccessor::FindPlayerByName(targetName);
    ObjectGuid::LowType targetGuid;

    if (targetPlayer)
    {
        targetGuid = targetPlayer->GetGUID().GetCounter();
    }
    else
    {
        std::string escapedName = targetName;
        CharacterDatabase.EscapeString(escapedName);
        QueryResult charResult = CharacterDatabase.Query(
            "SELECT guid FROM characters WHERE name = '{}'", escapedName);
        if (!charResult)
        {
            creature->Whisper(
                "Character \"" + targetName + "\" was not found. "
                "Check the spelling and try again.",
                LANG_UNIVERSAL, gm);
            return;
        }
        targetGuid = charResult->Fetch()[0].Get<uint32>();
    }

    std::string code   = GenerateRandomCode();
    InsertCodeToDatabase(code, rewardGroupKey);

    std::string text   = BuildGMCodeText(targetName, displayName, code, itemId);
    Item*       scroll = CreateStationeryWithText(targetPlayer, text);
    if (!scroll)
    {
        creature->Whisper("[GM] Failed to create stationery item.", LANG_UNIVERSAL, gm);
        return;
    }

    uint32 scrollGuid = scroll->GetGUID().GetCounter();

    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    scroll->SaveToDB(trans);
    MailDraft("A special reward awaits you!", "")
        .AddItem(scroll)
        .SendMailTo(trans,
            targetPlayer ? MailReceiver(targetPlayer, targetGuid)
                         : MailReceiver(targetGuid),
            MailSender(creature));
    CharacterDatabase.CommitTransaction(trans);

    DirectWriteItemText(scrollGuid, text);

    creature->Whisper(
        "[GM] A code for \"" + displayName + "\" has been mailed to \"" +
        targetName + "\".",
        LANG_UNIVERSAL, gm);
}

static void ShowExpansionListForCode(Player* player, Creature* creature, uint32 npcTextId)
{
    ClearGossipMenuFor(player);
    for (auto const& [senderVal, name] : EXPANSION_NAMES)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, name, SENDER_GM_SEND_CODE, senderVal);
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "< Back", SENDER_MAIN, 0);
    SendGossipMenuFor(player, npcTextId, creature->GetGUID());
}

static void ShowExpansionItemsForCode(Player* player, Creature* creature,
                                      uint32 sender, uint32 npcTextId)
{
    auto catalogIt = LANDRO_CATALOG.find(sender);
    if (catalogIt == LANDRO_CATALOG.end())
    {
        ShowExpansionListForCode(player, creature, npcTextId);
        return;
    }
    ClearGossipMenuFor(player);
    const std::vector<TCGItem>& items = catalogIt->second;
    for (uint32 i = 0; i < static_cast<uint32>(items.size()); ++i)
    {
        uint32 encoded = (sender << 8) | (i + 1);
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
            "[GM] Send code: " + items[i].displayName,
            SENDER_GM_SEND_CODE, encoded,
            "Enter character name to send a code for \"" +
            items[i].displayName + "\" to:",
            0, true);
    }
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "< Back to set list",
        SENDER_GM_SEND_CODE, 0);
    SendGossipMenuFor(player, npcTextId, creature->GetGUID());
}

static void ShowPromoCategoryListForCode(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    for (auto const& [senderVal, name] : PROMO_CATEGORY_NAMES)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, name, SENDER_GM_SEND_CODE, senderVal);
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "< Back", SENDER_MAIN, 0);
    SendGossipMenuFor(player, NPC_TEXT_PROMO, creature->GetGUID());
}

static void ShowPromoItemsForCode(Player* player, Creature* creature, uint32 sender)
{
    auto catalogIt = PROMO_CATALOG.find(sender);
    if (catalogIt == PROMO_CATALOG.end())
    {
        ShowPromoCategoryListForCode(player, creature);
        return;
    }
    ClearGossipMenuFor(player);
    const std::vector<TCGItem>& items = catalogIt->second;
    for (uint32 i = 0; i < static_cast<uint32>(items.size()); ++i)
    {
        uint32 encoded = (sender << 8) | (i + 1);
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
            "[GM] Send code: " + items[i].displayName,
            SENDER_GM_SEND_CODE, encoded,
            "Enter character name to send a code for \"" +
            items[i].displayName + "\" to:",
            0, true);
    }
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "< Back",
        SENDER_GM_SEND_CODE, 0);
    SendGossipMenuFor(player, NPC_TEXT_PROMO, creature->GetGUID());
}

// ============================================================
//  n p c _ l a n d r o _ l o n g s h o t
// ============================================================
class npc_landro_longshot : public CreatureScript
{
public:
    npc_landro_longshot() : CreatureScript("npc_landro_longshot") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (player->IsGameMaster())
        {
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] Browse and deliver TCG items...",
                SENDER_MAIN, ACTION_OPEN_BROWSE);
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] Clear character redemption flags",
                SENDER_GM_CLEAR, 0,
                "Enter character name to clear all TCG redemption flags:",
                0, true);
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] Send a code to a player...",
                SENDER_MAIN, ACTION_OPEN_SEND_CODE);
            SendGossipMenuFor(player, NPC_TEXT_LANDRO, creature->GetGUID());
            return true;
        }

        int mode = GetVendorMode();

        // Mode 0: disabled — return false and let the DB gossip handle it
        if (mode == MODE_DISABLED)
            return false;

        if (mode == MODE_FREE || mode == MODE_ITEM_CODE)
        {
            // Mode 1: free browse — items awarded on confirmation click.
            // Mode 3: browse first, then enter a code per item — text
            //         input box appears when the player clicks an item.
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                "Browse TCG items by expansion set...",
                SENDER_MAIN, ACTION_OPEN_BROWSE);
        }
        else  // Mode 2: Blizz-Like
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                "I have a TCG redemption code.",
                SENDER_CODE_ENTRY, 0,
                "Please enter your redemption code:",
                0, true);
        }

        SendGossipMenuFor(player, NPC_TEXT_LANDRO, creature->GetGUID());
        return true;
    }

    bool OnGossipSelectCode(Player* player, Creature* creature,
                            uint32 sender, uint32 action,
                            const char* code) override
    {
        std::string codeStr(code ? code : "");

        // GM: clear redemption flags for named character
        if (sender == SENDER_GM_CLEAR)
        {
            HandleGMClearFlags(player, creature, codeStr);
            OnGossipHello(player, creature);
            return true;
        }

        // GM: force re-delivery override confirmation.
        if (sender == SENDER_GM_FORCE)
        {
            uint32 origSender = (action >> 8) & 0xFF;
            uint32 origAction = action & 0xFF;
            auto catalogIt = LANDRO_CATALOG.find(origSender);
            if (catalogIt != LANDRO_CATALOG.end())
            {
                uint32 idx = origAction - 1;
                const std::vector<TCGItem>& items = catalogIt->second;
                if (idx < static_cast<uint32>(items.size()))
                {
                    uint32 redeemKey  = items[idx].entries[0];
                    bool   consumable = IsItemConsumable(redeemKey, items[idx].isConsumable);
                    HandleGMDelivery(player, creature, codeStr,
                                     items[idx].entries,
                                     items[idx].factionMount,
                                     consumable,
                                     items[idx].displayName,
                                     true /* forceOverride */);
                    ShowExpansionItems(player, creature, origSender, NPC_TEXT_LANDRO);
                    return true;
                }
            }
            CloseGossipMenuFor(player);
            return false;
        }

        // GM send-code path: generate a code and mail stationery to target.
        if (sender == SENDER_GM_SEND_CODE)
        {
            uint32 origSender = (action >> 8) & 0xFF;
            uint32 origAction = action & 0xFF;
            auto catalogIt = LANDRO_CATALOG.find(origSender);
            if (catalogIt != LANDRO_CATALOG.end())
            {
                uint32 idx = origAction - 1;
                const std::vector<TCGItem>& items = catalogIt->second;
                if (idx < static_cast<uint32>(items.size()))
                {
                    HandleGMSendCode(player, creature, codeStr,
                        items[idx].displayName,
                        items[idx].rewardGroupKey,
                        items[idx].entries[0]);
                    ShowExpansionItemsForCode(player, creature, origSender, NPC_TEXT_LANDRO);
                    return true;
                }
            }
            CloseGossipMenuFor(player);
            return false;
        }

        // GM delivery: player entered a character name for an item in the
        // expansion browser.
        if (player->IsGameMaster())
        {
            auto catalogIt = LANDRO_CATALOG.find(sender);
            if (catalogIt != LANDRO_CATALOG.end())
            {
                uint32 idx = action - 1;
                const std::vector<TCGItem>& items = catalogIt->second;
                if (idx < static_cast<uint32>(items.size()))
                {
                    uint32 redeemKey  = items[idx].entries[0];
                    bool   consumable = IsItemConsumable(redeemKey, items[idx].isConsumable);
                    bool delivered = HandleGMDelivery(player, creature, codeStr,
                                                     items[idx].entries,
                                                     items[idx].factionMount,
                                                     consumable,
                                                     items[idx].displayName,
                                                     false /* forceOverride */);
                    if (!delivered)
                    {
                        // Target has already received this item — ask for override.
                        ClearGossipMenuFor(player);
                        uint32 forceAction = (sender << 8) | action;
                        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                            "[GM] Force delivery to \"" + codeStr + "\" anyway",
                            SENDER_GM_FORCE, forceAction,
                            "\"" + codeStr + "\" already has \"" + items[idx].displayName +
                            "\". Enter their name again to confirm forced re-delivery:",
                            0, true);
                        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                            "< Cancel", SENDER_MAIN, 0);
                        SendGossipMenuFor(player, NPC_TEXT_LANDRO, creature->GetGUID());
                    }
                    else
                    {
                        ShowExpansionItems(player, creature, sender, NPC_TEXT_LANDRO);
                    }
                    return true;
                }
            }
            return false;
        }

        // Mode 2 (Blizzlike) item-level code entry — any valid unused code is accepted;
        // the reward is determined by the code's reward_group, not by which item was clicked.
        if (GetVendorMode() == MODE_BLIZZLIKE)
        {
            HandleCodeRedemption(player, creature, codeStr);
            CloseGossipMenuFor(player);
            return true;
        }

        // Mode 3: item-specific code entry — code must match the selected item.
        if (GetVendorMode() == MODE_ITEM_CODE)
        {
            auto catalogIt = LANDRO_CATALOG.find(sender);
            if (catalogIt != LANDRO_CATALOG.end())
            {
                uint32 idx = action - 1;
                const std::vector<TCGItem>& items = catalogIt->second;
                if (idx < static_cast<uint32>(items.size()))
                {
                    HandleItemSpecificCodeRedemption(
                        player, creature, codeStr,
                        items[idx].rewardGroupKey,
                        items[idx].displayName);
                    ShowExpansionItems(player, creature, sender, NPC_TEXT_LANDRO);
                    return true;
                }
            }
        }

        return false;
    }

    bool OnGossipSelect(Player* player, Creature* creature,
                        uint32 sender, uint32 action) override
    {
        if (GetVendorMode() == MODE_DISABLED)
            return false;

        // ---------------------------------------------------------------
        // WoW 3.3.5a gossip protocol note:
        //
        // For hasTextBox = true gossip items (Modes 2/3 code-entry items),
        // OnGossipSelect is NEVER called.  Only OnGossipSelectCode fires
        // when the player submits the text entry.  The confirmation popup
        // and text box are handled entirely client-side.
        //
        // OnGossipSelect IS called for hasTextBox = false items:
        //   - Mode 1 items (free delivery confirmation)
        //   - "[Already Redeemed]" items (added without text box in all modes)
        //
        // The server MUST respond to these clicks or the gossip window freezes.
        // ---------------------------------------------------------------

        ClearGossipMenuFor(player);

        if (sender == SENDER_MAIN)
        {
            if (action == ACTION_OPEN_BROWSE)
            {
                ShowExpansionList(player, creature, NPC_TEXT_LANDRO);
                return true;
            }

            if (action == ACTION_OPEN_SEND_CODE)
            {
                ShowExpansionListForCode(player, creature, NPC_TEXT_LANDRO);
                return true;
            }

            if (action == 0)
            {
                OnGossipHello(player, creature);
                return true;
            }

            ShowExpansionItems(player, creature, action, NPC_TEXT_LANDRO);
            return true;
        }

        // Send-code browse tree navigation.
        if (sender == SENDER_GM_SEND_CODE)
        {
            if (action == 0)
                ShowExpansionListForCode(player, creature, NPC_TEXT_LANDRO);
            else
                ShowExpansionItemsForCode(player, creature, action, NPC_TEXT_LANDRO);
            return true;
        }

        if (action == 0)
        {
            ShowExpansionList(player, creature, NPC_TEXT_LANDRO);
            return true;
        }

        auto catalogIt = LANDRO_CATALOG.find(sender);
        if (catalogIt == LANDRO_CATALOG.end())
        {
            CloseGossipMenuFor(player);
            return true;
        }

        if (GetVendorMode() == MODE_FREE)
            HandleBrowseSelect(player, creature, action, catalogIt->second);

        ShowExpansionItems(player, creature, sender, NPC_TEXT_LANDRO);
        return true;
    }
};

// ============================================================
//  n p c _ b l i z z c o n _ v e n d o r
//  Handles both Ransin Donner (2943) and Zas'Tysh (7951).
// ============================================================
class npc_blizzcon_vendor : public CreatureScript
{
public:
    npc_blizzcon_vendor() : CreatureScript("npc_blizzcon_vendor") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (player->IsGameMaster())
        {
            for (uint32 i = 0; i < static_cast<uint32>(BLIZZCON_CATALOG.size()); ++i)
            {
                AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                    "[GM] " + BLIZZCON_CATALOG[i].displayName,
                    GOSSIP_SENDER_MAIN, i + 1,
                    "Enter character name to deliver \"" +
                    BLIZZCON_CATALOG[i].displayName + "\" to:",
                    0, true);
            }
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] Clear character redemption flags",
                SENDER_GM_CLEAR, 0,
                "Enter character name to clear all TCG redemption flags:",
                0, true);
            for (uint32 i = 0; i < static_cast<uint32>(BLIZZCON_CATALOG.size()); ++i)
            {
                AddGossipItemFor(player, GOSSIP_ICON_TRAINER,
                    "[GM] Send code: " + BLIZZCON_CATALOG[i].displayName,
                    SENDER_GM_SEND_CODE, i + 1,
                    "Enter character name to send a code for \"" +
                    BLIZZCON_CATALOG[i].displayName + "\" to:",
                    0, true);
            }
            SendGossipMenuFor(player, NPC_TEXT_BLIZZCON, creature->GetGUID());
            return true;
        }

        int mode = GetVendorMode();

        if (mode == MODE_DISABLED)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();

        if (mode == MODE_FREE || mode == MODE_BLIZZLIKE || mode == MODE_ITEM_CODE)
        {
            for (uint32 i = 0; i < static_cast<uint32>(BLIZZCON_CATALOG.size()); ++i)
            {
                uint32 redeemKey  = BLIZZCON_CATALOG[i].entries[0];
                bool   consumable = IsItemConsumable(redeemKey, BLIZZCON_CATALOG[i].isConsumable);
                bool   redeemed   = !consumable && HasRedeemed(playerGuid, redeemKey);
                std::string label = BuildItemLabel(BLIZZCON_CATALOG[i].displayName,
                                                   redeemKey, consumable, playerGuid);

                if (redeemed)
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, label,
                        GOSSIP_SENDER_MAIN, i + 1);
                else if (mode == MODE_BLIZZLIKE || mode == MODE_ITEM_CODE)
                    AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label,
                        GOSSIP_SENDER_MAIN, i + 1,
                        "Enter your redemption code for \"" +
                        BLIZZCON_CATALOG[i].displayName + "\":",
                        0, true);
                else
                    AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label,
                        GOSSIP_SENDER_MAIN, i + 1,
                        "Receive \"" + BLIZZCON_CATALOG[i].displayName + "\"?",
                        0, false);
            }
        }

        SendGossipMenuFor(player, NPC_TEXT_BLIZZCON, creature->GetGUID());
        return true;
    }

    bool OnGossipSelectCode(Player* player, Creature* creature,
                            uint32 sender, uint32 action,
                            const char* code) override
    {
        std::string codeStr(code ? code : "");

        if (sender == SENDER_GM_CLEAR)
        {
            HandleGMClearFlags(player, creature, codeStr);
            OnGossipHello(player, creature);
            return true;
        }

        // GM: force re-delivery override confirmation.
        // For Blizzcon, action is just the 1-based item index (no expansion to encode).
        if (sender == SENDER_GM_FORCE)
        {
            uint32 idx = action - 1;
            if (idx < static_cast<uint32>(BLIZZCON_CATALOG.size()))
            {
                uint32 redeemKey  = BLIZZCON_CATALOG[idx].entries[0];
                bool   consumable = IsItemConsumable(redeemKey, BLIZZCON_CATALOG[idx].isConsumable);
                HandleGMDelivery(player, creature, codeStr,
                                 BLIZZCON_CATALOG[idx].entries,
                                 BLIZZCON_CATALOG[idx].factionMount,
                                 consumable,
                                 BLIZZCON_CATALOG[idx].displayName,
                                 true /* forceOverride */);
                OnGossipHello(player, creature);
                return true;
            }
            CloseGossipMenuFor(player);
            return false;
        }

        // GM send-code path for Blizzcon items.
        // action is 1-based item index directly (no expansion encoding needed).
        if (sender == SENDER_GM_SEND_CODE)
        {
            uint32 idx = action - 1;
            if (idx < static_cast<uint32>(BLIZZCON_CATALOG.size()))
            {
                HandleGMSendCode(player, creature, codeStr,
                    BLIZZCON_CATALOG[idx].displayName,
                    BLIZZCON_CATALOG[idx].rewardGroupKey,
                    BLIZZCON_CATALOG[idx].entries[0]);
                OnGossipHello(player, creature);
                return true;
            }
            CloseGossipMenuFor(player);
            return false;
        }

        if (player->IsGameMaster())
        {
            uint32 idx = action - 1;
            if (idx < static_cast<uint32>(BLIZZCON_CATALOG.size()))
            {
                uint32 redeemKey  = BLIZZCON_CATALOG[idx].entries[0];
                bool   consumable = IsItemConsumable(redeemKey, BLIZZCON_CATALOG[idx].isConsumable);
                bool delivered = HandleGMDelivery(player, creature, codeStr,
                                                  BLIZZCON_CATALOG[idx].entries,
                                                  BLIZZCON_CATALOG[idx].factionMount,
                                                  consumable,
                                                  BLIZZCON_CATALOG[idx].displayName,
                                                  false /* forceOverride */);
                if (!delivered)
                {
                    ClearGossipMenuFor(player);
                    AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                        "[GM] Force delivery to \"" + codeStr + "\" anyway",
                        SENDER_GM_FORCE, action,
                        "\"" + codeStr + "\" already has \"" + BLIZZCON_CATALOG[idx].displayName +
                        "\". Enter their name again to confirm forced re-delivery:",
                        0, true);
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                        "< Cancel", SENDER_MAIN, 0);
                    SendGossipMenuFor(player, NPC_TEXT_BLIZZCON, creature->GetGUID());
                }
                else
                {
                    OnGossipHello(player, creature);
                }
                return true;
            }
            return false;
        }

        if (GetVendorMode() == MODE_BLIZZLIKE)
        {
            HandleCodeRedemption(player, creature, codeStr);
            CloseGossipMenuFor(player);
            return true;
        }

        if (GetVendorMode() == MODE_ITEM_CODE)
        {
            uint32 idx = action - 1;
            if (idx < static_cast<uint32>(BLIZZCON_CATALOG.size()))
            {
                HandleItemSpecificCodeRedemption(
                    player, creature, codeStr,
                    BLIZZCON_CATALOG[idx].rewardGroupKey,
                    BLIZZCON_CATALOG[idx].displayName);
                OnGossipHello(player, creature);
                return true;
            }
        }

        return false;
    }

    bool OnGossipSelect(Player* player, Creature* creature,
                        uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);

        HandleBrowseSelect(player, creature, action, BLIZZCON_CATALOG);

        OnGossipHello(player, creature);
        return true;
    }
};

// ============================================================
//  Promo vendor browse helpers
// ============================================================

static void ShowPromoCategoryList(Player* player, Creature* creature)
{
    ClearGossipMenuFor(player);
    for (auto const& [senderVal, name] : PROMO_CATEGORY_NAMES)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, name, SENDER_MAIN, senderVal);
    SendGossipMenuFor(player, NPC_TEXT_PROMO, creature->GetGUID());
}

// Build the item list for one promo category.
// isGM true  => every item shown as player-name text input (GM delivery).
//         requiredSpell items are always visible to GMs.
// isGM false => items filtered by requiredSpell; mode-appropriate dialog.
static void ShowPromoItems(Player* player, Creature* creature, uint32 sender)
{
    auto catalogIt = PROMO_CATALOG.find(sender);
    if (catalogIt == PROMO_CATALOG.end())
    {
        ShowPromoCategoryList(player, creature);
        return;
    }

    bool isGM = player->IsGameMaster();
    int  mode = GetVendorMode();
    ClearGossipMenuFor(player);
    const std::vector<TCGItem>& items = catalogIt->second;
    uint32 playerGuid = player->GetGUID().GetCounter();

    for (uint32 i = 0; i < static_cast<uint32>(items.size()); ++i)
    {
        uint32 redeemKey  = items[i].entries[0];
        bool   consumable = items[i].isConsumable;

        // Conditional items: skip for non-GMs who haven't learned the required spell.
        if (!isGM && items[i].requiredSpell != 0 && !player->HasSpell(items[i].requiredSpell))
            continue;

        if (isGM)
        {
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] " + items[i].displayName,
                sender, i + 1,
                "Enter character name to deliver \"" + items[i].displayName + "\" to:",
                0, true);
        }
        else
        {
            bool        redeemed = !consumable && HasRedeemed(playerGuid, redeemKey);
            std::string label    = BuildItemLabel(items[i].displayName, redeemKey,
                                                  consumable, playerGuid);
            if (redeemed)
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, label, sender, i + 1);
            }
            else if (items[i].requiredSpell != 0)
            {
                // Spell-gated items (War Fuel) are always free regardless of mode —
                // no code is ever generated for them; possession of the required
                // companion is sufficient authorisation.
                AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label,
                    sender, i + 1,
                    "Receive \"" + items[i].displayName + "\"?",
                    0, false);
            }
            else if (mode == MODE_BLIZZLIKE || mode == MODE_ITEM_CODE)
            {
                AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label,
                    sender, i + 1,
                    "Enter your redemption code for \"" + items[i].displayName + "\":",
                    0, true);
            }
            else
            {
                AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label,
                    sender, i + 1,
                    "Receive \"" + items[i].displayName + "\"?",
                    0, false);
            }
        }
    }

    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "< Back", SENDER_MAIN, 0);
    SendGossipMenuFor(player, NPC_TEXT_PROMO, creature->GetGUID());
}

// ============================================================
//  n p c _ p r o m o _ v e n d o r
//  Handles Garel Redrock (Alliance, Ironforge) and
//  Tharl Stonebleeder (Horde, Orgrimmar).
// ============================================================
class npc_promo_vendor : public CreatureScript
{
public:
    npc_promo_vendor() : CreatureScript("npc_promo_vendor") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (player->IsGameMaster())
        {
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] Browse and deliver promotional items...",
                SENDER_MAIN, ACTION_OPEN_BROWSE);
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] Send a code to a player...",
                SENDER_MAIN, ACTION_OPEN_SEND_CODE);
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] Clear character redemption flags",
                SENDER_GM_CLEAR, 0,
                "Enter character name to clear all TCG redemption flags:",
                0, true);
            SendGossipMenuFor(player, NPC_TEXT_PROMO, creature->GetGUID());
            return true;
        }

        int mode = GetVendorMode();

        if (mode == MODE_DISABLED)
            return false;

        if (mode == MODE_FREE || mode == MODE_ITEM_CODE)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                "Browse promotional items by category...",
                SENDER_MAIN, ACTION_OPEN_BROWSE);
        }
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                "I have a Promotional redemption code.",
                SENDER_CODE_ENTRY, 0,
                "Please enter your redemption code:",
                0, true);
        }

        SendGossipMenuFor(player, NPC_TEXT_PROMO, creature->GetGUID());
        return true;
    }

    bool OnGossipSelectCode(Player* player, Creature* creature,
                            uint32 sender, uint32 action,
                            const char* code) override
    {
        std::string codeStr(code ? code : "");

        if (sender == SENDER_GM_CLEAR)
        {
            HandleGMClearFlags(player, creature, codeStr);
            OnGossipHello(player, creature);
            return true;
        }

        if (sender == SENDER_GM_FORCE)
        {
            uint32 origSender = (action >> 8) & 0xFF;
            uint32 origAction = action & 0xFF;
            auto catalogIt = PROMO_CATALOG.find(origSender);
            if (catalogIt != PROMO_CATALOG.end())
            {
                uint32 idx = origAction - 1;
                const std::vector<TCGItem>& items = catalogIt->second;
                if (idx < static_cast<uint32>(items.size()))
                {
                    bool consumable = items[idx].isConsumable;
                    HandleGMDelivery(player, creature, codeStr,
                                     items[idx].entries,
                                     items[idx].factionMount,
                                     consumable,
                                     items[idx].displayName,
                                     true /* forceOverride */);
                    ShowPromoItems(player, creature, origSender);
                    return true;
                }
            }
            CloseGossipMenuFor(player);
            return false;
        }

        // GM send-code path for promo items.
        // action encodes (categorySender << 8) | itemIndex.
        if (sender == SENDER_GM_SEND_CODE)
        {
            uint32 origSender = (action >> 8) & 0xFF;
            uint32 origAction = action & 0xFF;
            auto catalogIt = PROMO_CATALOG.find(origSender);
            if (catalogIt != PROMO_CATALOG.end())
            {
                uint32 idx = origAction - 1;
                const std::vector<TCGItem>& items = catalogIt->second;
                if (idx < static_cast<uint32>(items.size()))
                {
                    HandleGMSendCode(player, creature, codeStr,
                        items[idx].displayName,
                        items[idx].rewardGroupKey,
                        items[idx].entries[0]);
                    ShowPromoItemsForCode(player, creature, origSender);
                    return true;
                }
            }
            CloseGossipMenuFor(player);
            return false;
        }

        if (player->IsGameMaster())
        {
            auto catalogIt = PROMO_CATALOG.find(sender);
            if (catalogIt != PROMO_CATALOG.end())
            {
                uint32 idx = action - 1;
                const std::vector<TCGItem>& items = catalogIt->second;
                if (idx < static_cast<uint32>(items.size()))
                {
                    bool consumable = items[idx].isConsumable;
                    bool delivered = HandleGMDelivery(player, creature, codeStr,
                                                     items[idx].entries,
                                                     items[idx].factionMount,
                                                     consumable,
                                                     items[idx].displayName,
                                                     false /* forceOverride */);
                    if (!delivered)
                    {
                        ClearGossipMenuFor(player);
                        uint32 forceAction = (sender << 8) | action;
                        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                            "[GM] Force delivery to \"" + codeStr + "\" anyway",
                            SENDER_GM_FORCE, forceAction,
                            "\"" + codeStr + "\" already has \"" + items[idx].displayName +
                            "\". Enter their name again to confirm forced re-delivery:",
                            0, true);
                        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                            "< Cancel", SENDER_MAIN, 0);
                        SendGossipMenuFor(player, NPC_TEXT_PROMO, creature->GetGUID());
                    }
                    else
                    {
                        ShowPromoItems(player, creature, sender);
                    }
                    return true;
                }
            }
            return false;
        }

        if (GetVendorMode() == MODE_BLIZZLIKE)
        {
            HandleCodeRedemption(player, creature, codeStr);
            CloseGossipMenuFor(player);
            return true;
        }

        if (GetVendorMode() == MODE_ITEM_CODE)
        {
            auto catalogIt = PROMO_CATALOG.find(sender);
            if (catalogIt != PROMO_CATALOG.end())
            {
                uint32 idx = action - 1;
                const std::vector<TCGItem>& items = catalogIt->second;
                if (idx < static_cast<uint32>(items.size()))
                {
                    HandleItemSpecificCodeRedemption(
                        player, creature, codeStr,
                        items[idx].rewardGroupKey,
                        items[idx].displayName);
                    ShowPromoItems(player, creature, sender);
                    return true;
                }
            }
        }

        return false;
    }

    bool OnGossipSelect(Player* player, Creature* creature,
                        uint32 sender, uint32 action) override
    {
        ClearGossipMenuFor(player);

        if (sender == SENDER_MAIN)
        {
            if (action == ACTION_OPEN_BROWSE || action == 0)
            {
                ShowPromoCategoryList(player, creature);
                return true;
            }

            if (action == ACTION_OPEN_SEND_CODE)
            {
                ShowPromoCategoryListForCode(player, creature);
                return true;
            }

            ShowPromoItems(player, creature, action);
            return true;
        }

        // Send-code browse tree navigation.
        if (sender == SENDER_GM_SEND_CODE)
        {
            if (action == 0)
                ShowPromoCategoryListForCode(player, creature);
            else if (PROMO_CATALOG.find(action) != PROMO_CATALOG.end())
                ShowPromoItemsForCode(player, creature, action);
            else
                ShowPromoCategoryListForCode(player, creature);
            return true;
        }

        if (action == 0)
        {
            ShowPromoCategoryList(player, creature);
            return true;
        }

        auto catalogIt = PROMO_CATALOG.find(sender);
        if (catalogIt == PROMO_CATALOG.end())
        {
            CloseGossipMenuFor(player);
            return true;
        }

        uint32 idx = action - 1;
        const std::vector<TCGItem>& items = catalogIt->second;
        if (idx >= static_cast<uint32>(items.size()))
        {
            CloseGossipMenuFor(player);
            return true;
        }

        const TCGItem& item = items[idx];

        if (item.requiredSpell != 0 && !player->HasSpell(item.requiredSpell))
        {
            creature->Whisper(
                "You must have the Warbot companion learned before you can "
                "collect fuel for it.",
                LANG_UNIVERSAL, player);
            ShowPromoItems(player, creature, sender);
            return true;
        }

        DeliveryResult result = TryDeliverItem(player, creature,
                                               item.entries, item.entries[0],
                                               item.factionMount, item.isConsumable,
                                               item.displayName);
        if (result == DELIVERY_BAGS)
            creature->Whisper("Item granted. Enjoy!", LANG_UNIVERSAL, player);

        ShowPromoItems(player, creature, sender);
        return true;
    }
};

// ============================================================
//  BOSS DROP CONFIG HELPERS
// ============================================================
static bool GetBossDropEnabled()
{
    return sConfigMgr->GetOption<bool>("TCGVendors.BossDrop.Enabled", false);
}

static std::vector<uint32> GetBossDropCreatureIds()
{
    std::vector<uint32> ids;
    std::string str = sConfigMgr->GetOption<std::string>("TCGVendors.BossDrop.CreatureIds", "");
    size_t start = 0, end;
    while ((end = str.find(',', start)) != std::string::npos) {
        std::string token = str.substr(start, end - start);
        if (!token.empty()) ids.push_back(std::stoul(token));
        start = end + 1;
    }
    if (start < str.size()) ids.push_back(std::stoul(str.substr(start)));
    return ids;
}

static std::vector<uint32> GetBossDropItemIds()
{
    std::vector<uint32> ids;
    std::string str = sConfigMgr->GetOption<std::string>("TCGVendors.BossDrop.ItemIds", "");
    size_t start = 0, end;
    while ((end = str.find(',', start)) != std::string::npos) {
        std::string token = str.substr(start, end - start);
        if (!token.empty()) ids.push_back(std::stoul(token));
        start = end + 1;
    }
    if (start < str.size()) ids.push_back(std::stoul(str.substr(start)));
    return ids;
}

// TCGVendors.BossDrop.MailParticipants
//   0 — disabled (loot window only)
//   1 — mail only (no loot template rows; stationery mailed to every participant)
//   2 — mail AND loot (stationery on corpse + mail to every participant)
static int GetBossDropMailMode()
{
    int mode = sConfigMgr->GetOption<int>("TCGVendors.BossDrop.MailParticipants", 0);
    if (mode < 0 || mode > 2)
    {
        LOG_WARN("module",
            "mod-tcg-vendors: TCGVendors.BossDrop.MailParticipants has unrecognised value {} "
            "— falling back to 0 (disabled).", mode);
        return 0;
    }
    return mode;
}

// ============================================================
//  Boss Drop State
//
//  Keyed by creature entry ID.  Written in OnPlayerCreatureKill,
//  read in OnPlayerLootItem when each player picks up the stationery.
//
//  WHY THIS IS NECESSARY:
//  creature_loot_template stores item template entry IDs, not instances.
//  When a player loots, the engine calls Item::CreateItem() to produce a
//  brand-new blank instance — any pre-saved texted instance is orphaned.
//
//  The solution is to carry the pending text across the kill→loot boundary
//  in a C++ map, then inject it onto the fresh instance in OnPlayerLootItem
//  immediately after the loot system creates it.
// ============================================================
struct PendingBossDrop
{
    std::string rewardGroup;
    std::string bossName;
    std::string itemName;
    uint32      itemId = 0;
};

// Safe: AzerothCore map update loop is single-threaded per map.
static std::map<uint32, PendingBossDrop> s_pendingBossDrops;

// ============================================================
//  tcg_boss_drop_script  (PlayerScript)
//  Uses OnPlayerCreatureKill to detect configured boss kills.
// ============================================================
class tcg_boss_drop_script : public PlayerScript
{
public:
    tcg_boss_drop_script() : PlayerScript("tcg_boss_drop_script") {}

    void OnPlayerCreatureKill(Player* killer, Creature* killed) override
    {
        if (!GetBossDropEnabled() || !killer || !killed)
            return;

        uint32 creatureEntry = killed->GetEntry();
        auto bossIds = GetBossDropCreatureIds();
        if (std::find(bossIds.begin(), bossIds.end(), creatureEntry) == bossIds.end())
            return;

        auto itemIds = GetBossDropItemIds();
        if (itemIds.empty())
            return;

        uint32 itemId = itemIds[urand(0, static_cast<uint32>(itemIds.size()) - 1)];
        std::string rewardGroup = GetRewardGroupForItem(itemId);
        if (rewardGroup.empty())
        {
            LOG_ERROR("module",
                "mod-tcg-vendors: BossDrop item {} has no matching reward_group. "
                "Check TCGVendors.BossDrop.ItemIds and REWARD_GROUPS.", itemId);
            return;
        }

        std::string bossName = killed->GetName();
        std::string itemName = GetItemName(itemId);

        // Park metadata only — code generation happens in OnPlayerLootItem
        // so each looting player gets their own unique code from the corpse.
        // The mail-participants path generates its own codes independently.
        s_pendingBossDrops[creatureEntry] = { rewardGroup, bossName, itemName, itemId };

        // ---- Optional: mail a unique code to every group/raid member ----
        // Fires for MailParticipants = 1 (mail only) or 2 (mail + loot).
        if (GetBossDropMailMode() >= 1)
        {
            std::vector<Player*> participants;
            if (Group* group = killer->GetGroup())
            {
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                {
                    Player* member = ref->GetSource();
                    if (member && member->GetMap() == killed->GetMap())
                        participants.push_back(member);
                }
            }
            else
            {
                participants.push_back(killer);
            }

            for (Player* p : participants)
            {
                std::string pCode = GenerateRandomCode();
                InsertCodeToDatabase(pCode, rewardGroup);

                std::string pText = BuildStationeryText(bossName, itemName, pCode, itemId);
                Item* scroll = CreateStationeryWithText(p, pText);
                if (!scroll)
                    continue;

                uint32 scrollGuid = scroll->GetGUID().GetCounter();

                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                scroll->SaveToDB(trans);
                MailDraft("A reward for your valor!", "")
                    .AddItem(scroll)
                    .SendMailTo(trans,
                        MailReceiver(p, p->GetGUID().GetCounter()),
                        MailSender(killed));
                CharacterDatabase.CommitTransaction(trans);

                // Safety net: write text directly to item_instance in case
                // SaveToDB does not include m_text in this fork.
                DirectWriteItemText(scrollGuid, pText);
            }
        }
    }
};

// ============================================================
//  tcg_boss_drop_player_script  (PlayerScript)
//  Uses OnPlayerLootItem to inject the pending code text onto the
//  fresh stationery instance the loot system just created.
// ============================================================
class tcg_boss_drop_player_script : public PlayerScript
{
public:
    tcg_boss_drop_player_script() : PlayerScript("tcg_boss_drop_player_script") {}

    void OnPlayerLootItem(Player* player, Item* item, uint32 /*count*/, ObjectGuid lootGuid) override
    {
        if (!GetBossDropEnabled() || !player || !item)
            return;
 
        if (!lootGuid.IsCreature())
            return;
 
        Creature* source = ObjectAccessor::GetCreature(*player, lootGuid);
        if (!source)
            return;
 
        uint32 creatureEntry = source->GetEntry();
        auto bossIds = GetBossDropCreatureIds();
        if (std::find(bossIds.begin(), bossIds.end(), creatureEntry) == bossIds.end())
            return;

        if (item->GetEntry() != 9311)
            return;
 
        auto it = s_pendingBossDrops.find(creatureEntry);
        if (it == s_pendingBossDrops.end())
            return;
 
        const PendingBossDrop& drop = it->second;
 
        // Generate a unique code for this specific looting player.
        std::string code = GenerateRandomCode();
        InsertCodeToDatabase(code, drop.rewardGroup);
        std::string text = BuildStationeryText(drop.bossName, drop.itemName, code, drop.itemId);

        // Stamp the looted item in-place.
        //
        // StampItemText does three things:
        //   1. item->SetText(text)       — sets m_text for future SaveToDB calls
        //   2. SetFlag(ITEM_FIELD_FLAGS, ITEM_FIELD_FLAG_READABLE) — tells the
        //      client this item has readable text; client sends CMSG_ITEM_TEXT_QUERY
        //   3. SetState(ITEM_CHANGED)    — queues the flag update to be sent to
        //      the client on the next update cycle
        //
        // DirectWriteItemText immediately writes to item_instance.text so the
        // text is available the moment the client queries (CMSG_ITEM_TEXT_QUERY),
        // without waiting for the next autosave.
        StampItemText(item, player, text);
        DirectWriteItemText(item->GetGUID().GetCounter(), text);
    }
};

// ============================================================
//  tcg_boss_drop_world_script  (WorldScript)
//
//  On server startup, ensures every boss entry configured in
//  TCGVendors.BossDrop.CreatureIds has a creature_loot_template
//  row guaranteeing a 100% drop of item 9311 (Simple Stationery).
//
//  If any new rows are written, the creature loot tables are
//  reloaded in-process — no manual .reload or restart required.
//  If the rows already exist from a previous run or from manually
//  applying the SQL, nothing is touched and no reload happens.
// ============================================================
class tcg_boss_drop_world_script : public WorldScript
{
public:
    tcg_boss_drop_world_script() : WorldScript("tcg_boss_drop_world_script") {}

    void OnStartup() override
    {
        // Always purge ALL item 9311 rows on startup.
        // This keeps creature_loot_template in exact sync with the config
        // whether the feature is enabled, disabled, or CreatureIds is empty.
        WorldDatabase.Execute(
            "DELETE FROM creature_loot_template WHERE Item = 9311");

        if (!GetBossDropEnabled())
        {
            // Feature disabled — reload to reflect the clean state and stop.
            LoadLootTemplates_Creature();
            LootTemplates_Creature.CheckLootRefs();
            LOG_INFO("module",
                "mod-tcg-vendors: BossDrop disabled. "
                "All stationery loot rows purged.");
            return;
        }

        int mailMode = GetBossDropMailMode();
        auto bossIds = GetBossDropCreatureIds();

        // MailParticipants = 1 (mail only): no loot rows needed — stationery
        // is mailed directly to participants, never appears on the corpse.
        // Also skip loot rows if CreatureIds is empty regardless of mail mode.
        if (mailMode == 1 || bossIds.empty())
        {
            LoadLootTemplates_Creature();
            LootTemplates_Creature.CheckLootRefs();
            if (mailMode == 1)
                LOG_INFO("module",
                    "mod-tcg-vendors: BossDrop MailParticipants=1 (mail only). "
                    "No loot template rows inserted.");
            else
                LOG_INFO("module",
                    "mod-tcg-vendors: BossDrop enabled but CreatureIds is empty. "
                    "All stationery loot rows purged.");
            return;
        }

        // MailParticipants = 0 or 2: stationery appears on the corpse.
        // Insert one row per configured boss at 100% drop chance.
        for (uint32 bossEntry : bossIds)
        {
            WorldDatabase.Execute(
                "INSERT INTO creature_loot_template "
                "(Entry, Item, Reference, Chance, QuestRequired, LootMode, GroupId, MinCount, MaxCount, Comment) "
                "VALUES ({}, 9311, 0, 100, 0, 1, 0, 1, 1, 'TCG code scroll — mod-tcg-vendors')",
                bossEntry);
            LOG_INFO("module",
                "mod-tcg-vendors: Registered stationery drop for boss entry {}.", bossEntry);
        }

        LoadLootTemplates_Creature();
        LootTemplates_Creature.CheckLootRefs();
        LOG_INFO("module",
            "mod-tcg-vendors: Creature loot templates synced — {} boss drop row(s) active.",
            static_cast<uint32>(bossIds.size()));
    }
};

// ============================================================
// ============================================================
//
//  n p c _ t y r a e l s _ v e n d o r
//  Handles Edward Cairn (Horde, Undercity, entry 29095) and
//  Ian Drake (Alliance, Stormwind, entry 29093).
//
//  These vendors exist exclusively to distribute Tyrael's Hilt,
//  the reward granted at the 2008 Worldwide Invitational in Paris.
//
// ============================================================
// ============================================================
class npc_tyraels_vendor : public CreatureScript
{
public:
    npc_tyraels_vendor() : CreatureScript("npc_tyraels_vendor") {}

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (player->IsGameMaster())
        {
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] " + TYRAELS_CATALOG[0].displayName,
                GOSSIP_SENDER_MAIN, 1,
                "Enter character name to deliver \"" +
                TYRAELS_CATALOG[0].displayName + "\" to:",
                0, true);
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] Clear character redemption flags",
                SENDER_GM_CLEAR, 0,
                "Enter character name to clear all TCG redemption flags:",
                0, true);
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                "[GM] Send code: " + TYRAELS_CATALOG[0].displayName,
                SENDER_GM_SEND_CODE, 1,
                "Enter character name to send a code for \"" +
                TYRAELS_CATALOG[0].displayName + "\" to:",
                0, true);
            SendGossipMenuFor(player, NPC_TEXT_WWI, creature->GetGUID());
            return true;
        }

        int mode = GetVendorMode();

        if (mode == MODE_DISABLED)
            return false;

        uint32 playerGuid = player->GetGUID().GetCounter();
        uint32 redeemKey  = TYRAELS_CATALOG[0].entries[0];
        std::string label = BuildItemLabel(TYRAELS_CATALOG[0].displayName,
                                           redeemKey, false, playerGuid);

        if (HasRedeemed(playerGuid, redeemKey))
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, label,
                GOSSIP_SENDER_MAIN, 1);
        else if (mode == MODE_BLIZZLIKE || mode == MODE_ITEM_CODE)
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label,
                GOSSIP_SENDER_MAIN, 1,
                "Enter your redemption code for \"" +
                TYRAELS_CATALOG[0].displayName + "\":",
                0, true);
        else
            AddGossipItemFor(player, GOSSIP_ICON_VENDOR, label,
                GOSSIP_SENDER_MAIN, 1,
                "Receive \"" + TYRAELS_CATALOG[0].displayName + "\"?",
                0, false);

        // The WoW 3.3.5a client skips rendering the gossip window when there
        // is exactly one item with hasTextBox = true and jumps straight to the
        // text dialog.  A second item forces the menu to render so the NPC
        // greeting text is visible before the player commits to redeeming.
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Never mind.",
            GOSSIP_SENDER_MAIN, 0);

        SendGossipMenuFor(player, NPC_TEXT_WWI, creature->GetGUID());
        return true;
    }

    bool OnGossipSelectCode(Player* player, Creature* creature,
                            uint32 sender, uint32 action,
                            const char* code)
    {
        std::string codeStr(code ? code : "");

        if (sender == SENDER_GM_CLEAR)
        {
            HandleGMClearFlags(player, creature, codeStr);
            OnGossipHello(player, creature);
            return true;
        }

        // GM force-delivery override.
        if (sender == SENDER_GM_FORCE)
        {
            HandleGMDelivery(player, creature, codeStr,
                             TYRAELS_CATALOG[0].entries,
                             TYRAELS_CATALOG[0].factionMount,
                             TYRAELS_CATALOG[0].isConsumable,
                             TYRAELS_CATALOG[0].displayName,
                             true /* forceOverride */);
            OnGossipHello(player, creature);
            return true;
        }

        // GM send-code path.
        if (sender == SENDER_GM_SEND_CODE)
        {
            HandleGMSendCode(player, creature, codeStr,
                TYRAELS_CATALOG[0].displayName,
                TYRAELS_CATALOG[0].rewardGroupKey,
                TYRAELS_CATALOG[0].entries[0]);
            OnGossipHello(player, creature);
            return true;
        }

        // GM direct delivery.
        if (player->IsGameMaster())
        {
            bool delivered = HandleGMDelivery(player, creature, codeStr,
                                              TYRAELS_CATALOG[0].entries,
                                              TYRAELS_CATALOG[0].factionMount,
                                              TYRAELS_CATALOG[0].isConsumable,
                                              TYRAELS_CATALOG[0].displayName,
                                              false /* forceOverride */);
            if (!delivered)
            {
                ClearGossipMenuFor(player);
                AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1,
                    "[GM] Force delivery to \"" + codeStr + "\" anyway",
                    SENDER_GM_FORCE, 1,
                    "\"" + codeStr + "\" already has \"" +
                    TYRAELS_CATALOG[0].displayName +
                    "\". Enter their name again to confirm forced re-delivery:",
                    0, true);
                AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                    "< Cancel", SENDER_MAIN, 0);
                SendGossipMenuFor(player, NPC_TEXT_WWI, creature->GetGUID());
            }
            else
            {
                OnGossipHello(player, creature);
            }
            return true;
        }

        // Mode 2 (Blizzlike): any valid unused code accepted.
        if (GetVendorMode() == MODE_BLIZZLIKE)
        {
            HandleCodeRedemption(player, creature, codeStr);
            CloseGossipMenuFor(player);
            return true;
        }

        // Mode 3: code must match Tyrael's Hilt specifically.
        if (GetVendorMode() == MODE_ITEM_CODE)
        {
            HandleItemSpecificCodeRedemption(
                player, creature, codeStr,
                TYRAELS_CATALOG[0].rewardGroupKey,
                TYRAELS_CATALOG[0].displayName);
            OnGossipHello(player, creature);
            return true;
        }

        return false;
    }

    bool OnGossipSelect(Player* player, Creature* creature,
                        uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);

        if (action == 0)
        {
            // "Never mind." — just close.
            CloseGossipMenuFor(player);
            return true;
        }

        // action == 1: Mode 1 (free) confirmation click — deliver directly.
        HandleBrowseSelect(player, creature, 1, TYRAELS_CATALOG);
        OnGossipHello(player, creature);
        return true;
    }
};

// ============================================================
//  Script registration
// ============================================================
void Addmod_tcg_vendorsScripts()
{
    new npc_landro_longshot();
    new npc_blizzcon_vendor();
    new npc_promo_vendor();
    new npc_tyraels_vendor();
    new tcg_boss_drop_script();
    new tcg_boss_drop_player_script();
    new tcg_boss_drop_world_script();
}
