// repro driver: create ONE fake client through the engine's full
// connect chain (XASH3D_FAKECLIENT_CONNECT=1) so the mod's real player-spawn
// burst runs exactly like on the device. No ham hooks, no battery probes.
#include <amxmodx>
#include <fakemeta>

public plugin_init()
{
	register_plugin( "FakePlayerDriver", "1.0", "AmirWolf" )
	set_task( 8.0, "drv_spawn_fake" )
}

public drv_spawn_fake()
{
	new id = engfunc( EngFunc_CreateFakeClient, "repro" )
	server_print( "[REPRO] CreateFakeClient -> edict %d", id )
	if( id > 0 )
		set_task( 6.0, "drv_report", id )
}

public drv_report( id )
{
	new flags = pev( id, pev_flags )
	server_print( "[REPRO] t+6s edict %d flags %d alive %d team %d", id, flags, is_user_alive( id ), get_user_team( id ) )
}
