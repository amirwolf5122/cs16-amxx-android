// zp_test_host.sma - host test for the Zombie Plague mod
//
// The real-world test: on a live server with the real module chain,
// two engine fake clients join (T and CT), and the Zombie Plague mod
// must be able to turn a human into a zombie (direct native path AND
// the natural round-start path). Prints [ZPTEST] PASS/FAIL lines that
// the runner greps from the console log.
#include <amxmodx>
#include <cstrike>
#include <fakemeta>
#include <zombieplague>

#define MARK "[ZPTEST] "

public plugin_init()
{
        register_plugin("ZP Infection Test", "1.0", "AmirWolf");
        server_print(MARK "plugin_init: zombieplague natives resolved");
        server_cmd("mp_freezetime 0");
        server_cmd("mp_roundtime 9");
        server_cmd("mp_autoteambalance 0");
        server_cmd("mp_limitteams 0");
        set_task(5.0, "zp_stage_spawn");
}

static make_bot(const name[], const ip[], CsTeams:team, id)
{
        new ent = engfunc(EngFunc_CreateFakeClient, name);
        if (ent <= 0)
        {
                server_print(MARK "FAIL EngFunc_CreateFakeClient(%s)", name);
                return 0;
        }
        new reject[64];
        dllfunc(DLLFunc_ClientConnect, ent, name, ip, reject);
        dllfunc(DLLFunc_ClientPutInServer, ent);
        cs_set_user_team(id, team, team == CS_TEAM_T ? CS_T_LEET : CS_URBAN);
        cs_user_spawn(id);
        server_print(MARK "bot %s -> edict %d id %d team %d alive %d", name, ent, id, _:cs_get_user_team(id), is_user_alive(id));
        return 1;
}

public zp_stage_spawn()
{
        new okA = make_bot("ZPBotA", "127.0.0.111", CS_TEAM_T, 1);
        new okB = make_bot("ZPBotB", "127.0.0.112", CS_TEAM_CT, 2);
        if (!okA || !okB)
        {
                server_print(MARK "RESULT FAIL (bot creation)");
                return;
        }
        set_task(8.0, "zp_stage_status");
}

public zp_stage_status()
{
        server_print(MARK "status: alive1=%d alive2=%d zombie1=%d zombie2=%d packs1=%d",
                is_user_alive(1), is_user_alive(2),
                zp_get_user_zombie(1), zp_get_user_zombie(2),
                zp_get_user_ammo_packs(1));
        set_task(5.0, "zp_stage_infect");
}

public zp_stage_infect()
{
        // THE test: turn the CT human (id 2) into a zombie through the mod's
        // own native, exactly like sub-plugins (admin menus, antidote, ...)
        // do. With the stock x86 pdata offsets this path reads garbage team
        // data and silently fails on the arm64 device.
        new ok = zp_infect_user(2, 1, false, false);
        server_print(MARK "zp_infect_user(2,1) returned %d", ok);
        set_task(2.0, "zp_stage_verify");
}

public zp_stage_verify()
{
        new z = zp_get_user_zombie(2);
        new h = zp_get_user_zombie(1);
        server_print(MARK "verify: zombie2=%d zombie1=%d class2=%d maxhp2=%d",
                z, h, zp_get_user_zombie_class(2), zp_get_zombie_maxhealth(2));

        // zombie model must have been applied (pdata model index path)
        new model[32];
        pev(2, pev_model, model, charsmax(model));
        server_print(MARK "model2=%s", model);

        if (z)
                server_print(MARK "RESULT PASS (direct infection works)");
        else
                server_print(MARK "RESULT FAIL (zp_infect_user did not stick)");

        // natural path: restart the round and let ZP pick its own first
        // zombie after zp_delay (default 10s in zombieplague.cfg)
        server_cmd("sv_restart 1");
        set_task(18.0, "zp_stage_natural");
}

public zp_stage_natural()
{
        new z1 = zp_get_user_zombie(1), z2 = zp_get_user_zombie(2);
        server_print(MARK "natural round-start zombies: z1=%d z2=%d", z1, z2);
        if (z1 || z2)
                server_print(MARK "RESULT PASS (natural infection works)");
        else
                server_print(MARK "RESULT FAIL (no round-start zombie picked)");
}
