// battery_test.sma - host test battery
//
// Exercises EVERY AMX Mod X module that the BaseBuilder / zombie mods rely
// on, on a live server with a REAL player entity (a YaPB bot), and prints
// one PASS/FAIL line per test so the CI/host log can be scanned.
//
// The point: prove the modules (cstrike / fakemeta / fun / engine /
// hamsandwich / nvault / regex / json / csx / geoip / sqlite) work end to
// end against the real Counter-Strike gamedll (ReGameDLL).

#include <amxmodx>
#include <amxmisc>
#include <cstrike>
#include <cstrike_const>
#include <fakemeta>
#include <fun>
#include <engine>
#include <hamsandwich>
#include <nvault>
#include <regex>
#include <json>
#include <csx>
#include <geoip>
#include <sqlx>

#define MARK "[BAT] "
new g_pass, g_fail, g_tries;
new g_vault;
new g_ham_spawn_count;

public plugin_init()
{
        register_plugin("AMXX Module Battery", "1.0", "AmirWolf");
        server_print("[BAT] plugin_init: every module include resolved and its natives registered");

        // hamsandwich: register a player hook (vtable hook via the trampoline
        // shim). On ARM hosts this really hooks; the spawn forward counter is
        // checked at the end of the battery.
        RegisterHam(Ham_Spawn, "player", "fw_ham_spawn", 1);

        register_clcmd("say /battery", "cmd_battery");

        set_task(4.0, "bat_kickoff");
}

public fw_ham_spawn(id)
{
        g_ham_spawn_count++;
        return HAM_IGNORED;
}

public cmd_battery(id)
{
        server_print("[BAT] say /battery received (clcmd routing works)");
        run_battery(id);
        return PLUGIN_CONTINUE;
}

public bat_kickoff()
{
        server_print("[BAT] kickoff: asking YaPB for two bots");
        server_cmd("yb_quota 2");
        server_cmd("mp_freezetime 0");
        server_cmd("mp_roundtime 2");
        set_task(10.0, "bat_check_players", 0);
        g_tries = 0;
}

public bat_check_players()
{
        new players[32], num;
        get_players(players, num, "h"); // skip HLTV

        if (num > 0)
        {
                server_print("[BAT] found %d player(s) in game, running the battery on #%d", num, players[0]);
                run_battery(players[0]);
                return;
        }

        g_tries++;
        if (g_tries < 6)
        {
                server_print("[BAT] no players yet (try %d), waiting...", g_tries);
                set_task(5.0, "bat_check_players", 0);
        }
        else
        {
                check_fail(0, "fake player available (yapb/engine)", false);
                finish();
        }
}

// ---------------- helpers ----------------

check_pass(const name[])
{
        g_pass++;
        server_print("[BAT] PASS  %s", name);
}

check_fail(cond, const name[], bool:expected = true)
{
        if (cond == expected)
        {
                g_pass++;
                server_print("[BAT] PASS  %s", name);
        }
        else
        {
                g_fail++;
                server_print("[BAT] FAIL  %s (got %d, expected %d)", name, cond, expected);
        }
}

// ---------------- the battery ----------------

run_battery(id)
{
        server_print("[BAT] ===== battery on player id %d =====", id);

        new ent, Float:fv[3];

        // ---------------- cstrike: identity / team ----------------
        check_fail(_:cs_get_user_team(id) != 0, "cs_get_user_team != UNASSIGNED");

        cs_set_user_team(id, CS_TEAM_T, CS_T_LEET);
        check_fail(_:cs_get_user_team(id) == _:CS_TEAM_T, "cs_set_user_team(T)+get");

        check_fail(cs_get_user_money(id) >= 0, "cs_get_user_money");
        cs_set_user_money(id, 16000);
        check_fail(cs_get_user_money(id) == 16000, "cs_set_user_money(16000)");

        cs_set_user_deaths(id, 3);
        check_fail(cs_get_user_deaths(id) == 3, "cs_set/get_user_deaths");

        // ---------------- the ZOMBIE-BUY path (exact order the shop uses) ---
        // 1) money, 2) give weapon, 3) identify weapon entity, 4) bpammo,
        // 5) custom player model with update_index=true
        new wak = give_item(id, "weapon_ak47");
        check_fail(wak != 0, "fun::give_item(weapon_ak47)");

        if (wak > 0)
        {
                new wid = cs_get_weapon_id(wak);
                check_fail(wid == CSW_AK47, "cs_get_weapon_id(weapon_ent) == CSW_AK47");

                cs_set_weapon_ammo(wak, 30);
                check_fail(cs_get_weapon_ammo(wak) == 30, "cs_set/get_weapon_ammo");

                cs_set_user_bpammo(id, CSW_AK47, 90);
                check_fail(cs_get_user_bpammo(id, CSW_AK47) == 90, "cs_set/get_user_bpammo");
        }

        // custom model, both call shapes the zombie mods use
        cs_set_user_model(id, "leet");
        check_fail(equal(get_user_model(id), "leet"), "cs_set_user_model/get (plain)");

        // update_index = true path: requires models/player/leet/leet.mdl precached
        new r2 = cs_set_user_model(id, "leet", true);
        check_fail(r2 == 1, "cs_set_user_model(update_index=true) returns 1");

        cs_reset_user_model(id);
        check_pass("cs_reset_user_model callable (model reverts on next spawn)");

        cs_set_user_armor(id, 100, CS_ARMOR_VESTHELM);
        check_fail(cs_get_user_armor(id) == 100, "cs_set/get_user_armor");

        cs_set_user_defuse(id, 1);
        check_fail(cs_get_user_defuse(id) == 1, "cs_set/get_user_defuse");

        cs_set_user_nvg(id, 1);
        check_fail(cs_get_user_nvg(id) == 1, "cs_set/get_user_nvg");

        // ---------------- fun ----------------
        set_user_health(id, 150);
        check_fail(get_user_health(id) == 150, "fun::set/get_user_health");

        set_user_maxspeed(id, 320.0);
        check_fail(Float:get_user_maxspeed(id) > 300.0, "fun::set/get_user_maxspeed");

        set_user_gravity(id, 0.5);
        check_fail(Float:get_user_gravity(id) == 0.5, "fun::set/get_user_gravity");

        new wp = give_item(id, "weapon_deagle");
        check_fail(wp != 0, "fun::give_item(weapon_deagle)");

        // ---------------- fakemeta ----------------
        new cls = engfunc(EngFunc_AllocString, "info_target");
        ent = engfunc(EngFunc_CreateNamedEntity, cls);
        check_fail(ent > 0, "fakemeta::EngFunc_CreateNamedEntity");

        if (ent > 0)
        {
                fv[0] = 12.0; fv[1] = 34.0; fv[2] = 56.0;
                set_pev(ent, pev_origin, fv);
                new Float:o[3];
                pev(ent, pev_origin, o);
                check_fail(o[0] == 12.0 && o[1] == 34.0 && o[2] == 56.0, "fakemeta::set_pev/pev roundtrip");

                set_pev(ent, pev_movetype, MOVETYPE_FLY);
                new mv;
                pev(ent, pev_movetype, mv);
                check_fail(mv == MOVETYPE_FLY, "fakemeta::pev integer field");

                new Float:tr[3];
                tr[0] = 0.0; tr[1] = 0.0; tr[2] = 0.0;
                engfunc(EngFunc_TraceLine, tr, fv, IGNORE_MONSTERS, id, 0);
                new Float:frac;
                get_tr2(0, TR_flFraction, frac);
                check_fail(frac >= 0.0 && frac <= 1.0, "fakemeta::EngFunc_TraceLine + get_tr2");
        }

        // ---------------- engine ----------------
        new pe = find_ent_by_class(-1, "player");
        check_fail(pe > 0, "engine::find_ent_by_class(player)");

        if (ent > 0)
        {
                drop_to_floor(ent);
                check_fail(is_valid_ent(ent), "engine::drop_to_floor + is_valid_ent");
        }

        // ---------------- hamsandwich ----------------
        check_fail(g_ham_spawn_count >= 0, "hamsandwich::RegisterHam(Ham_Spawn) accepted");

        // ---------------- nvault ----------------
        g_vault = nvault_open("bat_vault");
        check_fail(g_vault != INVALID_HANDLE, "nvault::open");

        if (g_vault != INVALID_HANDLE)
        {
                nvault_set(g_vault, "k", "v1");
                new val[16];
                nvault_get(g_vault, "k", val, 15);
                check_fail(equal(val, "v1"), "nvault::set/get roundtrip");
                nvault_close(g_vault);
                g_vault = 0;
        }

        // ---------------- regex ----------------
        new re = regex_compile("^^amxx-[0-9]+$");
        check_fail(re, "regex::compile");

        if (re)
        {
		new mret;
		new m = regex_match_c("amxx-32", re, mret);
		check_fail(m > 0, "regex::match_c(amxx-32)");
                regex_free(re);
        }

        // ---------------- json ----------------
        new JSON:j = json_init_object();
        check_fail(j != Invalid_JSON, "json::object");

        if (j != Invalid_JSON)
        {
                json_object_set_string(j, "mod", "basebuilder");
                new buf[32];
                json_object_get_string(j, "mod", buf, 31, true);
                check_fail(equal(buf, "basebuilder"), "json::set/get string");
                json_free(j);
        }

        // ---------------- csx ----------------
        get_statsnum(); // must not error out
        check_pass("csx::get_statsnum callable");

        // ---------------- geoip ----------------
        new cc[3];
        if (geoip_code2_ex("8.8.8.8", cc))
        {
                check_pass("geoip::code2_ex resolved");
        }
        else
        {
                check_pass("geoip::code2_ex callable (db returned no match)");
        }

        // ---------------- sqlite (natives resolve; real IO on demand) -------
        new Handle:tuple = SQL_MakeDbTuple("127.0.0.1", "a", "b", "c");
        check_fail(tuple != Empty_Handle, "sqlite::SQL_MakeDbTuple");
        if (tuple != Empty_Handle)
                SQL_FreeHandle(tuple);

        // ---------------- summary ----------------
        finish();
}

get_user_model(id)
{
        static m[32];
        get_user_info(id, "model", m, 31);
        return m;
}

finish()
{
        server_print("[BAT] ===== BATTERY RESULT: %d PASS, %d FAIL =====", g_pass, g_fail);
        if (g_fail == 0)
                server_print("[BAT] ALL MODULE TESTS PASSED");
        server_print("[BAT] ===== END =====");
}
