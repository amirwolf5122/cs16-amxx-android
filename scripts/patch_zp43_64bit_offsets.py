#!/usr/bin/env python3
"""
Patch Zombie Plague 4.3 for the 64-bit (arm64)
ReGameDLL gamedll shipped on the device.

ZP 4.3 hardcodes classic x86 pdata offsets (OFFSET_CSTEAMS 114/119,
OFFSET_CSMENUCODE 205/210, ...). On the device the gamedll is the
cs16-client ReGameDLL fork built for arm64 where CBasePlayer offsets are
completely different (m_iTeam=576, m_iMenu=964, m_iDeaths=1992, ...), so
the stock plugin reads/writes unrelated memory:
  - team reads are garbage      -> nobody becomes a zombie
  - menu-code writes are garbage-> mod buy menu stays blocked
  - money/deaths/modelindex broken

Fix (self-calibrating): the offsets become runtime variables calibrated
on the first team-assigned player. The oracle is the cstrike module's
cs_get_user_team(), which is gamedata-driven and correct on every build.
If get_pdata_int(id, 114, 5) matches the oracle -> classic layout.
Otherwise if get_pdata_int(id, 576, 0) matches -> absolute LP64 offsets
(measured with offsetof() from the same ReGameDLL source the device ships;
identical layout on arm64 and x86-64, verified against
gamedata/offsets-cstrike-replugged.arm64.txt).
32-bit platforms keep the classic values, so the plugin stays portable.
"""
import sys, re, pathlib

SRC = pathlib.Path(sys.argv[1])
OUT = pathlib.Path(sys.argv[2])

text = SRC.read_text(encoding="latin-1")

# ---------------------------------------------------------------- offsets block
OLD_BLOCK = """// CS Player PData Offsets (win32)
const PDATA_SAFE = 2
const OFFSET_PAINSHOCK = 108 // ConnorMcLeod
const OFFSET_CSTEAMS = 114
const OFFSET_CSMONEY = 115
const OFFSET_CSMENUCODE = 205
const OFFSET_FLASHLIGHT_BATTERY = 244
const OFFSET_CSDEATHS = 444
const OFFSET_MODELINDEX = 491 // Orangutanz

// CS Player CBase Offsets (win32)
const OFFSET_ACTIVE_ITEM = 373

// CS Weapon CBase Offsets (win32)
const OFFSET_WEAPONOWNER = 41

// Linux diff's
const OFFSET_LINUX = 5 // offsets 5 higher in Linux builds
const OFFSET_LINUX_WEAPONS = 4 // weapon offsets are only 4 steps higher on Linux"""

NEW_BLOCK = """// CS Player PData Offsets (win32)
const PDATA_SAFE = 2
// runtime-calibrated pdata offsets. Classic win32
// (+linux diff) values by default; switched to the absolute LP64 offsets
// on the 64-bit ReGameDLL gamedll once cs_get_user_team() (gamedata-driven,
// always correct) confirms which layout the server really uses.
new OFFSET_PAINSHOCK = 108 // ConnorMcLeod
new OFFSET_CSTEAMS = 114
new OFFSET_CSMONEY = 115
new OFFSET_CSMENUCODE = 205
new OFFSET_FLASHLIGHT_BATTERY = 244
new OFFSET_CSDEATHS = 444
new OFFSET_MODELINDEX = 491 // Orangutanz

// CS Player CBase Offsets (win32)
new OFFSET_ACTIVE_ITEM = 373

// CS Weapon CBase Offsets (win32)
new OFFSET_WEAPONOWNER = 41

// Linux diff's
new OFFSET_LINUX = 5 // offsets 5 higher in Linux builds
new OFFSET_LINUX_WEAPONS = 4 // weapon offsets are only 4 steps higher on Linux

// 0 = not calibrated yet, 1 = classic, 2 = LP64
new g_pdata_mode
const OFFSET_PAINSHOCK_64 = -1 // field removed from the LP64 gamedll: skip
const OFFSET_CSTEAMS_64 = 576
const OFFSET_CSMONEY_64 = 580
const OFFSET_CSMENUCODE_64 = 964
const OFFSET_FLASHLIGHT_BATTERY_64 = 1132
const OFFSET_CSDEATHS_64 = 1992
const OFFSET_MODELINDEX_64 = 2180
const OFFSET_ACTIVE_ITEM_64 = 1696
const OFFSET_WEAPONOWNER_64 = 232

// Calibrate the pdata layout against the team the GAME just announced for
// this player (TeamInfo arg 2). No cs_get_user_team here -- TeamInfo
// fires before PutInServer during joins and the native raises "Invalid
// player" (runtime error 10) which aborted the whole hook. The announced
// team string IS the authoritative team, so use it directly as the oracle.
// Safe to call for every team change; it acts once.
stock pdata_calibrate(id, want)
{
        if (g_pdata_mode) return;

        // only a T/CT player gives an unambiguous answer
        if (want != 1 && want != 2) return;

        // classic layout: win offset + 5 (linux diff already applied)
        if (get_pdata_int(id, OFFSET_CSTEAMS, OFFSET_LINUX) == want)
        {
                g_pdata_mode = 1;
                return;
        }

        // LP64 (arm64/x86-64) gamedll: absolute offsets, no linux diff
        if (get_pdata_int(id, OFFSET_CSTEAMS_64, 0) == want)
        {
                g_pdata_mode = 2;
                OFFSET_PAINSHOCK = OFFSET_PAINSHOCK_64
                OFFSET_CSTEAMS = OFFSET_CSTEAMS_64
                OFFSET_CSMONEY = OFFSET_CSMONEY_64
                OFFSET_CSMENUCODE = OFFSET_CSMENUCODE_64
                OFFSET_FLASHLIGHT_BATTERY = OFFSET_FLASHLIGHT_BATTERY_64
                ORDER_CSDEATHS_PLACEHOLDER
                OFFSET_MODELINDEX = OFFSET_MODELINDEX_64
                OFFSET_ACTIVE_ITEM = OFFSET_ACTIVE_ITEM_64
                OFFSET_WEAPONOWNER = OFFSET_WEAPONOWNER_64
                OFFSET_LINUX = 0
                OFFSET_LINUX_WEAPONS = 0
        }
}"""

if OLD_BLOCK not in text:
    print("PATCH FAIL: offsets block marker not found"); sys.exit(1)
text = text.replace(OLD_BLOCK, NEW_BLOCK)
text = text.replace("ORDER_CSDEATHS_PLACEHOLDER", "OFFSET_CSDEATHS = OFFSET_CSDEATHS_64")

# ---------------------------------------------------------------- painshock guard
OLD_PAIN = "\t// Set pain shock free offset\n\tset_pdata_float(victim, OFFSET_PAINSHOCK, 1.0, OFFSET_LINUX)"
NEW_PAIN = "\t// Set pain shock free offset\n\t// the LP64 gamedll has no pain shock field\n\tif (OFFSET_PAINSHOCK >= 0)\n\t\tset_pdata_float(victim, OFFSET_PAINSHOCK, 1.0, OFFSET_LINUX)"
if OLD_PAIN not in text:
    print("PATCH FAIL: painshock marker not found"); sys.exit(1)
text = text.replace(OLD_PAIN, NEW_PAIN)

# ---------------------------------------------------------------- calibrate hooks
# 1) player spawn post (Ham_Spawn) — the standard entry ZP already uses
SPAWN_MARK = "register_forward(FM_PlayerPreThink,"
print("note: spawn hook search:", SPAWN_MARK in text)

# find fw_PlayerSpawn_Post (registered via RegisterHam(Ham_Spawn, "player", ...))
m = re.search(r"public fw_PlayerSpawn_Post\((\w+)\)\r?\n\{", text)
if not m:
    print("PATCH FAIL: fw_PlayerSpawnPost not found"); sys.exit(1)
ID = m.group(1)
INJ = (
    "public fw_PlayerSpawn_Post(" + ID + ")\n{\n"
    "\tserver_print(\"[ZPDBG] spawn_post id=%d alive=%d mode=%d\", " + ID + ", is_user_alive(" + ID + "), g_pdata_mode)\n"
    "\t// calibrate pdata offsets on the first spawn\n"
    "\t// (team passed explicitly, and only for ALIVE players -- the\n"
    "\t// putinserver spawn runs before AMXX marks the client ingame and\n"
    "\t// before a team is picked, where cs_get_user_team would raise\n"
    "\t// \"Invalid player\"; join-time calibration is covered by the\n"
    "\t// TeamInfo hook using the announced team string)\n"
    "\tif (is_user_alive(" + ID + ")) pdata_calibrate(" + ID + ", _:cs_get_user_team(" + ID + "))\n"
    "\tserver_print(\"[ZPDBG] spawn_post mode=%d\", g_pdata_mode)\n"
    "\t// team not assigned yet -> pdata layout still unknown; skip the\n"
    "\t// pdata-heavy spawn body (the round restart respawns everyone properly)\n"
    "\tif (!g_pdata_mode) return;"
)
text = re.sub(r"public fw_PlayerSpawn_Post\(\w+\)\r?\n\{", INJ.replace("\\", "\\\\"), text, count=1)

# 2) team info message — earliest reliable team signal (register_message handler:
#    params come from get_msg_arg_*; player id = get_msg_arg_int(1))
m2 = re.search(r"public message_teaminfo\(([^)]*)\)\r?\n\{", text)
if m2:
    INJ2 = (
        "public message_teaminfo(" + m2.group(1) + ")\n{\n"
        "\tserver_print(\"[ZPDBG] teaminfo\")\n"
        "\t// calibrate with the team announced by the\n"
        "\t// message itself (arg 2 = \"TERRORIST\"/\"CT\"/...), never with natives\n"
        "\t// that can raise \"Invalid player\" during the join race.\n"
        "\tnew szTeam[12], want\n"
        "\tget_msg_arg_string(2, szTeam, 11)\n"
        "\tif (equal(szTeam, \"TERRORIST\")) want = 1\n"
        "\telse if (equal(szTeam, \"CT\")) want = 2\n"
        "\tpdata_calibrate(get_msg_arg_int(1), want)\n"
        "\tserver_print(\"[ZPDBG] teaminfo mode=%d\", g_pdata_mode)"
    )
    text = re.sub(r"public message_teaminfo\(([^)]*)\)\r?\n\{", INJ2.replace("\\", "\\\\"), text, count=1)
else:
    print("note: no teaminfo hook found, spawn hook only")

# ---------------------------------------------------------------- version tag
text = re.sub(r'new const PLUGIN_VERSION\[\] = "([^"]+)"',
              lambda m: 'new const PLUGIN_VERSION[] = "%s-cs16amxx64"' % m.group(1),
              text, count=1)

OUT.write_text(text, encoding="latin-1")
print("patched:", OUT)
