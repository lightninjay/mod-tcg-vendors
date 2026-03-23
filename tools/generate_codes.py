#!/usr/bin/env python3
"""
mod-tcg-vendors  —  Redemption Code Generator
==============================================

Run with no arguments to launch the interactive dashboard.
Run with flags for non-interactive / scripted use.

CODE FORMAT
-----------
    XXXX-XXXX-XXXX-XXXX

Each segment uses an unambiguous charset:
    Letters : A B C D E F G H J K M N P Q R S T U V W X Y Z
              (excludes I, L, O — visually similar to 1, 1, 0)
    Digits  : 2 3 4 5 6 7 8 9
              (excludes 0, 1 — visually similar to O, I/L)

31 chars × 16 positions → 31^16 ≈ 7.3 × 10^23 possible codes.

NON-INTERACTIVE USAGE
---------------------
    python generate_codes.py --list-groups
    python generate_codes.py --reward-group TCG_RIDING_TURTLE --count 10
    python generate_codes.py --reward-group TCG_RIDING_TURTLE TCG_SPECTRAL_TIGER --count 5
    python generate_codes.py --all --count 10 --output full_batch.sql
    python generate_codes.py --reward-group BLIZZCON_MURKY --count 3 --dry-run
    python generate_codes.py --interactive    # force dashboard even with other flags

APPLYING CODES
--------------
    mysql -u root -p acore_characters < generated_codes.sql
"""

import argparse
import curses
import secrets
import sys
from datetime import datetime, timezone

# ============================================================
#  Unambiguous charset — must match IsValidCodeFormat() in
#  mod_tcg_vendors.cpp exactly.
# ============================================================
CHARSET = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"   # 31 characters
VERSION = "1.0.0"

# ============================================================
#  Reward groups — keep in sync with REWARD_GROUPS in
#  mod_tcg_vendors.cpp.  Ordered by in-game Landro sub-menus.
# ============================================================
EXPANSIONS = [
    ("Heroes of Azeroth", [
        ("TCG_TABARD_OF_FLAME",           "Tabard of Flame"),
        ("TCG_HIPPOGRYPH_HATCHLING",      "Hippogryph Hatchling"),
        ("TCG_RIDING_TURTLE",             "Riding Turtle"),
    ]),
    ("Through the Dark Portal", [
        ("TCG_PICNIC_BASKET",             "Picnic Basket"),
        ("TCG_BANANA_CHARM",              "Banana Charm"),
        ("TCG_IMP_IN_A_BALL",             "Imp in a Ball"),
    ]),
    ("Fires of Outland", [
        ("TCG_GOBLIN_GUMBO_KETTLE",       "Goblin Gumbo Kettle"),
        ("TCG_FISHING_CHAIR",             "Fishing Chair"),
        ("TCG_SPECTRAL_TIGER",            "Reins of the Spectral Tiger (both variants)"),
    ]),
    ("March of the Legion", [
        ("TCG_PAPER_FLYING_MACHINE",      "Paper Flying Machine Kit"),
        ("TCG_ROCKET_CHICKEN",            "Rocket Chicken"),
        ("TCG_DRAGON_KITE",               "Dragon Kite"),
    ]),
    ("Servants of the Betrayer", [
        ("TCG_X51_NETHER_ROCKET",         "X-51 Nether-Rocket (both variants)"),
        ("TCG_PET_BISCUIT",               "Papa Hummel's Old-Fashioned Pet Biscuit"),
        ("TCG_GOBLIN_WEATHER_MACHINE",    "Goblin Weather Machine - Prototype 01-B"),
    ]),
    ("Hunt for Illidan", [
        ("TCG_PATH_OF_ILLIDAN",           "Path of Illidan"),
        ("TCG_DISCO",                     "D.I.S.C.O."),
        ("TCG_SOUL_TRADER_BEACON",        "Soul-Trader Beacon"),
    ]),
    ("Drums of War", [
        ("TCG_PARTY_GRENADE",             "Party G.R.E.N.A.D.E."),
        ("TCG_FLAG_OF_OWNERSHIP",         "The Flag of Ownership"),
        ("TCG_BIG_BATTLE_BEAR",           "Big Battle Bear"),
    ]),
    ("Blood of Gladiators", [
        ("TCG_SANDBOX_TIGER",             "Sandbox Tiger"),
        ("TCG_EPIC_PURPLE_SHIRT",         "Epic Purple Shirt"),
        ("TCG_FOAM_SWORD_RACK",           "Foam Sword Rack"),
    ]),
    ("Fields of Honor", [
        ("TCG_PATH_OF_CENARIUS",          "Path of Cenarius"),
        ("TCG_OGRE_PINATA",               "Ogre Pinata"),
        ("TCG_MAGIC_ROOSTER_EGG",         "Magic Rooster Egg"),
    ]),
    ("Scourgewar", [
        ("TCG_SCOURGEWAR_MINIMOUNT",      "Scourgewar Mini-Mount (faction-aware)"),
        ("TCG_TUSKARR_KITE",              "Tuskarr Kite"),
        ("TCG_SPECTRAL_TIGER_CUB",        "Spectral Tiger Cub"),
    ]),
    ("Wrathgate", [
        ("TCG_LANDROS_GIFT_BOX",          "Landro's Gift Box"),
        ("TCG_INSTANT_STATUE_PEDESTAL",   "Instant Statue Pedestal"),
        ("TCG_BLAZING_HIPPOGRYPH",        "Blazing Hippogryph"),
    ]),
    ("Icecrown", [
        ("TCG_PAINT_BOMB",                "Paint Bomb"),
        ("TCG_ETHEREAL_PORTAL",           "Ethereal Portal"),
        ("TCG_WOOLY_WHITE_RHINO",         "Wooly White Rhino"),
    ]),
    ("Points Redemption", [
        ("TCG_TABARD_OF_FROST",           "Tabard of Frost"),
        ("TCG_PERPETUAL_PURPLE_FIREWORK", "Perpetual Purple Firework"),
        ("TCG_CARVED_OGRE_IDOL",          "Carved Ogre Idol"),
        ("TCG_TABARD_OF_THE_ARCANE",      "Tabard of the Arcane"),
        ("TCG_TABARD_OF_BRILLIANCE",      "Tabard of Brilliance"),
        ("TCG_TABARD_OF_THE_DEFENDER",    "Tabard of the Defender"),
        ("TCG_TABARD_OF_FURY",            "Tabard of Fury"),
        ("TCG_TABARD_OF_NATURE",          "Tabard of Nature"),
        ("TCG_TABARD_OF_THE_VOID",        "Tabard of the Void"),
        ("TCG_LANDROS_PET_BOX",           "Landro's Pet Box"),
    ]),
    ("Blizzcon Promotional", [
        ("BLIZZCON_MURKY",                "Murky (Blue Murloc Egg)"),
        ("BLIZZCON_MURLOC_COSTUME",       "Murloc Costume"),
        ("BLIZZCON_BIG_BLIZZARD_BEAR",    "Big Blizzard Bear"),
    ]),
    ("Murloc Companions", [
        ("PROMO_GURKY",              "Gurky (Pink Murloc Egg)"),
        ("PROMO_ORANGE_MURLOC_EGG", "Orange Murloc Egg"),
        ("PROMO_WHITE_MURLOC_EGG",  "White Murloc Egg"),
        ("PROMO_HEAVY_MURLOC_EGG",  "Heavy Murloc Egg"),
        ("PROMO_MURKIMUS_SPEAR",    "Murkimus' Little Spear"),
    ]),
    ("Classic & Special Promotions", [
        ("PROMO_ZERGLING_LEASH",    "Zergling Leash"),
        ("PROMO_PANDA_COLLAR",      "Panda Collar"),
        ("PROMO_DIABLO_STONE",      "Diablo Stone"),
        ("PROMO_NETHERWHELP",       "Netherwhelp's Collar"),
        ("PROMO_FROSTYS_COLLAR",    "Frosty's Collar"),
        ("PROMO_WARBOT_KEY",        "Warbot Ignition Key"),
        # Red/Blue War Fuel are freely redeemable in-game with no code required.
        # Do not generate codes for them.
    ]),
    ("Blizzard Store", [
        ("PROMO_ENCHANTED_ONYX",    "Enchanted Onyx"),
        ("PROMO_CORE_HOUND_PUP",    "Core Hound Pup"),
        ("PROMO_GRYPHON_HATCHLING", "Gryphon Hatchling"),
        ("PROMO_WIND_RIDER_CUB",    "Wind Rider Cub"),
        ("PROMO_PANDAREN_MONK",     "Pandaren Monk"),
    ]),
    ("Worldwide Invitational", [
        ("PROMO_TYRAELS_HILT",      "Tyrael's Hilt"),
    ]),
    ("Special Events & Tournaments", [
        ("PROMO_LIL_PHYLACTERY",    "Lil' Phylactery"),
        ("PROMO_LIL_XT",            "Lil' XT"),
        ("PROMO_MINI_THOR",         "Mini Thor"),
        ("PROMO_ONYXIAN_WHELPLING", "Onyxian Whelpling"),
    ]),
]

REWARD_GROUPS = {k: v for _, items in EXPANSIONS for k, v in items}
ALL_KEYS      = [k   for _, items in EXPANSIONS for k, _ in items]


# ============================================================
#  Core generation logic  (shared by TUI and CLI)
# ============================================================

def generate_code():
    return "-".join("".join(secrets.choice(CHARSET) for _ in range(4)) for _ in range(4))


def generate_unique_codes(count, existing):
    codes, seen = [], set(existing)
    attempts, limit = 0, count * 20
    while len(codes) < count and attempts < limit:
        c = generate_code()
        if c not in seen:
            codes.append(c)
            seen.add(c)
        attempts += 1
    return codes


def esc_sql(v):
    return v.replace("\\", "\\\\").replace("'", "\\'")


def build_sql_block(codes, reward_group):
    name = REWARD_GROUPS[reward_group]
    ts   = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
    rows = ",\n".join(
        "    ('{}', '{}')".format(esc_sql(c), esc_sql(reward_group)) for c in codes
    )
    return (
        "-- {}  ({})\n"
        "-- Generated: {} UTC  |  Count: {}\n"
        "INSERT IGNORE INTO `account_tcg_codes` (`code`, `reward_group`) VALUES\n"
        "{};\n"
    ).format(reward_group, name, ts, len(codes), rows)


def build_full_sql(batches):
    ts    = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
    total = sum(len(c) for _, c in batches)
    hdr = (
        "-- ============================================================\n"
        "-- mod-tcg-vendors  --  Redemption Code Batch\n"
        "-- Generated : {} UTC\n"
        "-- Groups    : {}\n"
        "-- Total     : {} codes\n"
        "-- Apply to  : characters database\n"
        "-- ============================================================\n\n"
    ).format(ts, len(batches), total)
    return hdr + "\n".join(build_sql_block(c, g) for g, c in batches)


# ============================================================
#  Colour pair IDs
# ============================================================
CP_HEADER    = 1   # black on gold    — title bar
CP_STATUSBAR = 2   # black on gold    — bottom hint bar
CP_SELECTED  = 3   # black on cyan    — highlighted row
CP_SECTION   = 4   # yellow on black  — expansion headings
CP_NORMAL    = 5   # white on black   — regular text
CP_DIM       = 6   # grey on black    — secondary / hints
CP_CHECKED   = 7   # green on black   — selected / success
CP_ERROR     = 8   # red on black     — errors
CP_INPUTBG   = 9   # white on blue    — text-input field
CP_BORDER    = 10  # cyan on black    — box borders


# ============================================================
#  Flat row model for the item browser
# ============================================================
class Row:
    __slots__ = ("kind", "label", "key")
    def __init__(self, kind, label, key=None):
        self.kind  = kind    # "header" | "item"
        self.label = label
        self.key   = key     # reward group key (items only)


def _build_flat_rows():
    rows = []
    for exp_name, items in EXPANSIONS:
        rows.append(Row("header", "  " + exp_name))
        for key, name in items:
            rows.append(Row("item", "    " + name, key=key))
    return rows


FLAT_ROWS = _build_flat_rows()


# ============================================================
#  Dashboard
# ============================================================
class Dashboard:

    SCR_MAIN       = "MAIN"
    SCR_SINGLE     = "SINGLE"
    SCR_MULTI      = "MULTI"
    SCR_FULL_BATCH = "FULL_BATCH"
    SCR_CONFIRM    = "CONFIRM"
    SCR_RESULT     = "RESULT"

    MAIN_OPTIONS = [
        ("Single Item",  "Browse expansions and generate codes for one reward."),
        ("Multi-Select", "Pick several items; generate codes for each."),
        ("Full Batch",   "Generate codes for every available item."),
        ("Quick Batch",  "Full batch with a single count prompt — fastest option."),
        ("Exit",         "Quit the dashboard."),
    ]

    def __init__(self, stdscr):
        self.scr            = stdscr
        self.screen         = self.SCR_MAIN
        self.running        = True
        self.stdout_sql     = None      # set when output == stdout

        self.main_cur       = 0
        self.browse_cur     = 1         # start on first item (skip first header)
        self.browse_scroll  = 0
        self.selected_keys  = set()

        self.pending_keys   = []
        self.pending_count  = 10
        self.pending_output = ""        # "" = stdout

        self.result_msg     = ""
        self.result_ok      = True

        self._setup_colors()

    # ── colours ──────────────────────────────────────────────

    def _setup_colors(self):
        curses.start_color()
        curses.use_default_colors()
        curses.init_pair(CP_HEADER,    curses.COLOR_BLACK,  curses.COLOR_YELLOW)
        curses.init_pair(CP_STATUSBAR, curses.COLOR_BLACK,  curses.COLOR_YELLOW)
        curses.init_pair(CP_SELECTED,  curses.COLOR_BLACK,  curses.COLOR_CYAN)
        curses.init_pair(CP_SECTION,   curses.COLOR_YELLOW, -1)
        curses.init_pair(CP_NORMAL,    curses.COLOR_WHITE,  -1)
        curses.init_pair(CP_DIM,       curses.COLOR_WHITE,  -1)   # may brighten on 256-color
        curses.init_pair(CP_CHECKED,   curses.COLOR_GREEN,  -1)
        curses.init_pair(CP_ERROR,     curses.COLOR_RED,    -1)
        curses.init_pair(CP_INPUTBG,   curses.COLOR_WHITE,  curses.COLOR_BLUE)
        curses.init_pair(CP_BORDER,    curses.COLOR_CYAN,   -1)
        # Use dark-grey for DIM on 256-colour terminals
        if curses.COLORS >= 256:
            curses.init_color(240, 500, 500, 500)
            curses.init_pair(CP_DIM, 240, -1)

    # ── drawing primitives ───────────────────────────────────

    def _hw(self):
        return self.scr.getmaxyx()

    def _put(self, y, x, text, attr=0):
        h, w = self._hw()
        if not (0 <= y < h and 0 <= x < w):
            return
        text = text[:max(0, w - x)]
        try:
            self.scr.addstr(y, x, text, attr)
        except curses.error:
            pass

    def _fillrow(self, y, attr):
        h, w = self._hw()
        if 0 <= y < h:
            try:
                self.scr.addstr(y, 0, " " * w, attr)
            except curses.error:
                pass

    def _hline(self, y, ch="─"):
        h, w = self._hw()
        if 0 <= y < h:
            try:
                self.scr.addstr(y, 0, ch * w, curses.color_pair(CP_BORDER))
            except curses.error:
                pass

    def _box(self, y, x, rows, cols, title=""):
        attr = curses.color_pair(CP_BORDER)
        inner = cols - 2
        top = "┌─ {} {}┐".format(title, "─" * max(0, inner - len(title) - 3)) if title \
              else "┌" + "─" * inner + "┐"
        self._put(y, x, top[:cols], attr)
        for i in range(1, rows - 1):
            self._put(y + i, x, "│", attr)
            self._put(y + i, x + cols - 1, "│", attr)
        self._put(y + rows - 1, x, ("└" + "─" * inner + "┘")[:cols], attr)

    def _header(self):
        h, w = self._hw()
        title = "  mod-tcg-vendors  |  Code Generator Dashboard  v{}  ".format(VERSION)
        self._fillrow(0, curses.color_pair(CP_HEADER) | curses.A_BOLD)
        self._put(0, 0, title, curses.color_pair(CP_HEADER) | curses.A_BOLD)

    def _statusbar(self, hint):
        h, w = self._hw()
        bar = ("  " + hint).ljust(w)[:w]
        try:
            self.scr.addstr(h - 1, 0, bar, curses.color_pair(CP_STATUSBAR))
        except curses.error:
            pass

    # ── input dialog ─────────────────────────────────────────

    def _input(self, prompt, default="", validator=None):
        """Modal text-input dialog. Returns string or None (cancelled)."""
        h, w = self._hw()
        bw = min(62, w - 4)
        bh = 7
        by = max(1, (h - bh) // 2)
        bx = max(0, (w - bw) // 2)
        value = default
        errmsg = ""

        curses.curs_set(1)
        while True:
            self.scr.clear()
            self._header()
            self._box(by, bx, bh, bw, title=" Input ")
            self._put(by + 1, bx + 2, prompt[:bw - 4],
                      curses.color_pair(CP_NORMAL) | curses.A_BOLD)
            fy, fx, fw = by + 3, bx + 2, bw - 4
            self._put(fy, fx, " " * fw, curses.color_pair(CP_INPUTBG))
            disp = value[-fw:] if len(value) > fw else value
            self._put(fy, fx, disp, curses.color_pair(CP_INPUTBG))
            if errmsg:
                self._put(by + 5, bx + 2, errmsg[:bw - 4], curses.color_pair(CP_ERROR))
            self._statusbar("Enter: confirm    Esc: cancel")
            try:
                self.scr.move(fy, fx + min(len(disp), fw - 1))
            except curses.error:
                pass
            self.scr.refresh()

            try:
                key = self.scr.get_wch()
            except curses.error:
                continue

            if isinstance(key, str):
                ch = ord(key)
                if ch == 27:                   # Esc
                    curses.curs_set(0)
                    return None
                elif ch in (10, 13):           # Enter
                    if validator:
                        r = validator(value)
                        if r is not True:
                            errmsg = r if isinstance(r, str) else "Invalid input."
                            continue
                    curses.curs_set(0)
                    return value
                elif ch in (8, 127):           # Backspace
                    value = value[:-1]
                    errmsg = ""
                elif 32 <= ch < 127:
                    value += key
                    errmsg = ""
            else:
                if key == curses.KEY_BACKSPACE:
                    value = value[:-1]
                    errmsg = ""
                elif key in (curses.KEY_ENTER, 10, 13):
                    if validator:
                        r = validator(value)
                        if r is not True:
                            errmsg = r if isinstance(r, str) else "Invalid input."
                            continue
                    curses.curs_set(0)
                    return value

    # ── screens ──────────────────────────────────────────────

    def _draw_main(self):
        h, w = self._hw()
        self.scr.clear()
        self._header()
        self._put(2, 4, "WoW TCG & Blizzcon Promotional Code Generator",
                  curses.color_pair(CP_DIM))
        self._hline(3)

        label_x, desc_x = 6, 28
        self._put(4, label_x, "MAIN MENU",
                  curses.color_pair(CP_SECTION) | curses.A_BOLD)

        for i, (label, desc) in enumerate(self.MAIN_OPTIONS):
            y   = 5 + i * 2
            sel = (i == self.main_cur)
            pfx = " >> " if sel else "    "
            if sel:
                self._fillrow(y, curses.color_pair(CP_SELECTED))
                self._put(y, 1, pfx + label,
                          curses.color_pair(CP_SELECTED) | curses.A_BOLD)
                self._put(y, desc_x, desc[:w - desc_x - 1],
                          curses.color_pair(CP_SELECTED))
            else:
                col = curses.color_pair(CP_ERROR) if label == "Exit" else curses.color_pair(CP_NORMAL)
                self._put(y, 1, pfx + label, col | curses.A_BOLD)
                self._put(y, desc_x, desc[:w - desc_x - 1], curses.color_pair(CP_DIM))

        info_y = 5 + len(self.MAIN_OPTIONS) * 2 + 1
        self._hline(info_y)
        self._put(info_y + 1, label_x,
                  "Items: {}    Expansions: {}".format(len(ALL_KEYS), len(EXPANSIONS)),
                  curses.color_pair(CP_DIM))
        self._statusbar("Up/Down: navigate    Enter/Space: select    Q: quit")

    def _draw_browser(self, multi):
        h, w = self._hw()
        self.scr.clear()
        self._header()

        if multi:
            n_sel = len(self.selected_keys)
            self._put(1, 0, "  SELECT ITEMS  --  Space: toggle    Enter: confirm",
                      curses.color_pair(CP_SECTION) | curses.A_BOLD)
            badge = "  [ {} selected ] ".format(n_sel)
            self._put(1, w - len(badge) - 1, badge,
                      curses.color_pair(CP_CHECKED) | curses.A_BOLD)
        else:
            self._put(1, 0, "  SELECT ITEM  --  Enter: choose",
                      curses.color_pair(CP_SECTION) | curses.A_BOLD)
        self._hline(2)

        list_y = 3
        list_h = h - list_y - 2
        n      = len(FLAT_ROWS)

        if self.browse_cur < self.browse_scroll:
            self.browse_scroll = self.browse_cur
        if self.browse_cur >= self.browse_scroll + list_h:
            self.browse_scroll = self.browse_cur - list_h + 1

        for vi in range(list_h):
            ri  = self.browse_scroll + vi
            if ri >= n:
                break
            row = FLAT_ROWS[ri]
            y   = list_y + vi
            if row.kind == "header":
                self._put(y, 0, row.label,
                          curses.color_pair(CP_SECTION) | curses.A_BOLD)
            else:
                is_cur    = (ri == self.browse_cur)
                is_picked = multi and row.key in self.selected_keys
                tick      = "OK " if is_picked else "   "
                text      = tick + row.label.lstrip()
                if is_cur:
                    self._fillrow(y, curses.color_pair(CP_SELECTED))
                    self._put(y, 4, text[:w - 5],
                              curses.color_pair(CP_SELECTED) | curses.A_BOLD)
                elif is_picked:
                    self._put(y, 4, text[:w - 5],
                              curses.color_pair(CP_CHECKED) | curses.A_BOLD)
                else:
                    self._put(y, 4, text[:w - 5], curses.color_pair(CP_NORMAL))

        if n > list_h:
            pct = int(self.browse_scroll / max(1, n - list_h) * 100)
            self._put(list_y + list_h - 1, w - 8,
                      " {:>3d}% ".format(pct), curses.color_pair(CP_DIM))

        if multi:
            self._statusbar("Up/Down: navigate    Space: toggle    A: all    N: none    Enter: confirm    Esc: back")
        else:
            self._statusbar("Up/Down: navigate    Enter: select    Esc: back")

    def _draw_full_batch_screen(self):
        h, w = self._hw()
        self.scr.clear()
        self._header()
        self._put(2, 4, "FULL BATCH GENERATION",
                  curses.color_pair(CP_SECTION) | curses.A_BOLD)
        self._hline(3)
        lines = [
            "",
            "  Generates codes for all {} available rewards across all".format(len(ALL_KEYS)),
            "  TCG expansions and Blizzcon promotional items.",
            "",
            "  You will be asked for:",
            "    - Codes to generate per item",
            "    - Output file  (blank = print to stdout)",
            "",
            "  Apply the resulting file with:",
            "    mysql -u root -p acore_characters < your_batch.sql",
            "",
            "  Press Enter to continue, or Esc to go back.",
        ]
        for i, line in enumerate(lines):
            self._put(4 + i, 2, line, curses.color_pair(CP_NORMAL))
        self._statusbar("Enter: continue    Esc: back")

    def _draw_confirm(self):
        h, w = self._hw()
        self.scr.clear()
        self._header()
        self._put(2, 4, "CONFIRM & GENERATE",
                  curses.color_pair(CP_SECTION) | curses.A_BOLD)
        self._hline(3)
        self._put(4, 4, "Items:", curses.color_pair(CP_NORMAL) | curses.A_BOLD)

        max_rows = max(1, h - 14)
        for i, key in enumerate(self.pending_keys):
            if i >= max_rows:
                self._put(5 + i, 6,
                          "  ... and {} more".format(len(self.pending_keys) - max_rows),
                          curses.color_pair(CP_DIM))
                break
            self._put(5 + i, 6,
                      "[+]  {}".format(REWARD_GROUPS.get(key, key)),
                      curses.color_pair(CP_CHECKED))

        sy = 5 + min(len(self.pending_keys), max_rows) + 1
        self._hline(sy)
        out = self.pending_output if self.pending_output else "(stdout)"
        for i, line in enumerate([
            "  Codes per item : {}".format(self.pending_count),
            "  Total codes    : {}".format(len(self.pending_keys) * self.pending_count),
            "  Output         : {}".format(out),
        ]):
            self._put(sy + 1 + i, 4, line, curses.color_pair(CP_NORMAL))

        self._put(sy + 5, 4,
                  "Press Enter to generate, or Esc to go back.",
                  curses.color_pair(CP_DIM))
        self._statusbar("Enter: generate    Esc: back")

    def _draw_result(self):
        h, w = self._hw()
        self.scr.clear()
        self._header()
        self._put(2, 4, "GENERATION COMPLETE",
                  curses.color_pair(CP_SECTION) | curses.A_BOLD)
        self._hline(3)

        ry   = h // 2 - 2
        icon = "[OK]" if self.result_ok else "[!!]"
        col  = curses.color_pair(CP_CHECKED) if self.result_ok else curses.color_pair(CP_ERROR)
        self._put(ry, 4, "{}  {}".format(icon, self.result_msg), col | curses.A_BOLD)

        if self.result_ok:
            self._put(ry + 2, 4, "Apply with:", curses.color_pair(CP_NORMAL))
            if self.pending_output:
                cmd = "    mysql -u root -p acore_characters < {}".format(self.pending_output)
            else:
                cmd = "    (SQL printed to stdout after the dashboard exits)"
            self._put(ry + 3, 4, cmd[:w - 5], curses.color_pair(CP_DIM))

        self._put(ry + 6, 4,
                  "Press any key to return to the main menu.",
                  curses.color_pair(CP_DIM))
        self._statusbar("Any key: return to main menu")

    # ── helpers ──────────────────────────────────────────────

    def _validate_count(self, v):
        try:
            n = int(v)
            return True if 1 <= n <= 100000 else "Enter a number between 1 and 100000."
        except ValueError:
            return "Please enter a whole number."

    def _ask_count(self, default=10):
        r = self._input("How many codes per item?",
                        default=str(default), validator=self._validate_count)
        return int(r) if r is not None else None

    def _ask_output(self, default=""):
        return self._input("Output file path  (leave blank for stdout):",
                           default=default)

    def _generate(self):
        try:
            batches, seen = [], set()
            for key in self.pending_keys:
                codes = generate_unique_codes(self.pending_count, seen)
                seen.update(codes)
                batches.append((key, codes))
            sql   = build_full_sql(batches)
            total = len(self.pending_keys) * self.pending_count
            if self.pending_output:
                with open(self.pending_output, "w", encoding="utf-8") as f:
                    f.write(sql)
                self.result_msg = "{} codes for {} item(s) written to '{}'".format(
                    total, len(self.pending_keys), self.pending_output)
            else:
                self.stdout_sql = sql
                self.result_msg = "{} codes for {} item(s) generated (SQL prints after exit)".format(
                    total, len(self.pending_keys))
            self.result_ok = True
        except Exception as e:
            self.result_msg = "Error: {}".format(e)
            self.result_ok  = False

    # ── browser navigation helper ────────────────────────────

    def _browse_advance(self, delta):
        pos = self.browse_cur + delta
        n   = len(FLAT_ROWS)
        while 0 <= pos < n and FLAT_ROWS[pos].kind == "header":
            pos += delta
        if 0 <= pos < n:
            self.browse_cur = pos

    # ── input handlers ───────────────────────────────────────

    def _on_main(self, key):
        n = len(self.MAIN_OPTIONS)
        if key in (curses.KEY_UP, ord('k')):
            self.main_cur = (self.main_cur - 1) % n
        elif key in (curses.KEY_DOWN, ord('j')):
            self.main_cur = (self.main_cur + 1) % n
        elif key in (curses.KEY_ENTER, 10, 13, ord(' ')):
            label = self.MAIN_OPTIONS[self.main_cur][0]
            if label == "Exit":
                self.running = False
            elif label == "Single Item":
                self.browse_cur = 1; self.browse_scroll = 0
                self.screen = self.SCR_SINGLE
            elif label == "Multi-Select":
                self.browse_cur = 1; self.browse_scroll = 0
                self.selected_keys = set()
                self.screen = self.SCR_MULTI
            elif label == "Full Batch":
                self.screen = self.SCR_FULL_BATCH
            elif label == "Quick Batch":
                count = self._ask_count(1)
                if count is None: return
                out = self._ask_output("full_batch.sql")
                if out is None: return
                self.pending_keys   = list(ALL_KEYS)
                self.pending_count  = count
                self.pending_output = out
                self._generate()
                self.screen = self.SCR_RESULT
        elif key in (ord('q'), ord('Q')):
            self.running = False

    def _on_browser(self, key, multi):
        if key in (curses.KEY_UP, ord('k')):
            self._browse_advance(-1)
        elif key in (curses.KEY_DOWN, ord('j')):
            self._browse_advance(1)
        elif key == curses.KEY_PPAGE:
            for _ in range(10): self._browse_advance(-1)
        elif key == curses.KEY_NPAGE:
            for _ in range(10): self._browse_advance(1)
        elif key == 27:
            self.screen = self.SCR_MAIN
        elif not multi and key in (curses.KEY_ENTER, 10, 13):
            row = FLAT_ROWS[self.browse_cur]
            if row.kind == "item":
                count = self._ask_count()
                if count is None: return
                out = self._ask_output()
                if out is None: return
                self.pending_keys   = [row.key]
                self.pending_count  = count
                self.pending_output = out
                self.screen = self.SCR_CONFIRM
        elif multi:
            if key == ord(' '):
                row = FLAT_ROWS[self.browse_cur]
                if row.kind == "item":
                    if row.key in self.selected_keys:
                        self.selected_keys.discard(row.key)
                    else:
                        self.selected_keys.add(row.key)
                    self._browse_advance(1)
            elif key in (ord('a'), ord('A')):
                self.selected_keys = set(ALL_KEYS)
            elif key in (ord('n'), ord('N')):
                self.selected_keys = set()
            elif key in (curses.KEY_ENTER, 10, 13):
                if not self.selected_keys: return
                ordered = [k for k in ALL_KEYS if k in self.selected_keys]
                count = self._ask_count()
                if count is None: return
                out = self._ask_output()
                if out is None: return
                self.pending_keys   = ordered
                self.pending_count  = count
                self.pending_output = out
                self.screen = self.SCR_CONFIRM

    def _on_full_batch(self, key):
        if key == 27:
            self.screen = self.SCR_MAIN
        elif key in (curses.KEY_ENTER, 10, 13):
            count = self._ask_count(10)
            if count is None: return
            out = self._ask_output("full_batch.sql")
            if out is None: return
            self.pending_keys   = list(ALL_KEYS)
            self.pending_count  = count
            self.pending_output = out
            self.screen = self.SCR_CONFIRM

    def _on_confirm(self, key):
        if key == 27:
            self.screen = self.SCR_MAIN
        elif key in (curses.KEY_ENTER, 10, 13):
            self._generate()
            self.screen = self.SCR_RESULT

    def _on_result(self, _key):
        self.screen = self.SCR_MAIN

    # ── main loop ────────────────────────────────────────────

    def run(self):
        curses.curs_set(0)
        self.scr.keypad(True)
        self.scr.timeout(-1)

        while self.running:
            if   self.screen == self.SCR_MAIN:       self._draw_main()
            elif self.screen == self.SCR_SINGLE:      self._draw_browser(multi=False)
            elif self.screen == self.SCR_MULTI:       self._draw_browser(multi=True)
            elif self.screen == self.SCR_FULL_BATCH:  self._draw_full_batch_screen()
            elif self.screen == self.SCR_CONFIRM:     self._draw_confirm()
            elif self.screen == self.SCR_RESULT:      self._draw_result()

            self.scr.refresh()

            try:
                key = self.scr.getch()
            except curses.error:
                continue
            if key == -1:
                continue

            if   self.screen == self.SCR_MAIN:       self._on_main(key)
            elif self.screen == self.SCR_SINGLE:      self._on_browser(key, multi=False)
            elif self.screen == self.SCR_MULTI:       self._on_browser(key, multi=True)
            elif self.screen == self.SCR_FULL_BATCH:  self._on_full_batch(key)
            elif self.screen == self.SCR_CONFIRM:     self._on_confirm(key)
            elif self.screen == self.SCR_RESULT:      self._on_result(key)

        return self.stdout_sql


def launch_dashboard():
    sql = None
    def _run(stdscr):
        nonlocal sql
        sql = Dashboard(stdscr).run()
    try:
        curses.wrapper(_run)
    except KeyboardInterrupt:
        pass
    if sql:
        sys.stdout.write(sql)


# ============================================================
#  CLI (non-interactive)
# ============================================================

def cli_list_groups():
    col = max(len(k) for k in REWARD_GROUPS) + 2
    print()
    for exp_name, items in EXPANSIONS:
        print("  [ {} ]".format(exp_name))
        for key, name in items:
            print("    {:<{}}  {}".format(key, col, name))
        print()


def cli_generate(args):
    keys = list(ALL_KEYS) if args.all else (args.reward_group or [])
    bad  = [k for k in keys if k not in REWARD_GROUPS]
    if bad:
        sys.exit("ERROR: Unknown reward group(s): {}".format(", ".join(bad)))
    if not keys:
        sys.exit("ERROR: specify --reward-group KEY [KEY...] or --all")
    if args.count < 1:
        sys.exit("ERROR: --count must be at least 1")

    batches, seen = [], set()
    for key in keys:
        codes = generate_unique_codes(args.count, seen)
        seen.update(codes)
        if args.dry_run:
            print("\n--- {}  ({}) ---".format(key, REWARD_GROUPS[key]))
            for c in codes: print("  " + c)
        else:
            batches.append((key, codes))

    if args.dry_run:
        print("\nDry run: {} code(s). No SQL written.".format(len(keys) * args.count))
        return

    sql = build_full_sql(batches)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(sql)
        print("Written to: {}".format(args.output))
    else:
        sys.stdout.write(sql)


def main():
    if len(sys.argv) == 1:
        launch_dashboard()
        return

    p = argparse.ArgumentParser(
        description="mod-tcg-vendors Redemption Code Generator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    p.add_argument("--interactive", action="store_true",
                   help="Force the interactive dashboard.")
    p.add_argument("--list-groups", action="store_true",
                   help="List all reward group keys and exit.")
    p.add_argument("--reward-group", nargs="+", metavar="KEY",
                   help="One or more reward group keys.")
    p.add_argument("--all", action="store_true",
                   help="Generate codes for every available item.")
    p.add_argument("--count", type=int, default=10, metavar="N",
                   help="Codes per reward group (default: 10).")
    p.add_argument("--output", metavar="FILE",
                   help="Write SQL to FILE instead of stdout.")
    p.add_argument("--dry-run", action="store_true",
                   help="Print code list only; no SQL output.")

    args = p.parse_args()

    if args.interactive:
        launch_dashboard()
    elif args.list_groups:
        cli_list_groups()
    else:
        cli_generate(args)


if __name__ == "__main__":
    main()
