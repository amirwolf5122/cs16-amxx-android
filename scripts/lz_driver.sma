/**
 * lz_driver.sma — TEST-ONLY driver (host CI chain).
 * NOT shipped in the package. Its single job: after the real Light Zombie
 * boss plugin (zp_lightzombie.amxx) is loaded by the real server, invoke
 * its public Round_boss() through the standard AMXX callfunc API — exactly
 * what the in-game admin menu ("Spawn Light Zombie Boss") does — so the
 * boss entity, its models/sounds/sprites and its Ham think hooks all run
 * inside a real engine + metamod + AMXX + ReGameDLL + ZP 4.3 chain.
 */
#include <amxmodx>

public plugin_init()
{
	register_plugin( "LZ Test Driver", "1.0", "AmirWolf" )
	set_task( 45.0, "lzd_fire_boss" )
	set_task( 90.0, "lzd_report_alive" )
}

public lzd_fire_boss()
{
	server_print( "[LZTEST] dispatching Round_boss into zp_lightzombie.amxx" )

	if ( callfunc_begin( "Round_boss", "zp_lightzombie.amxx" ) != 1 )
	{
		server_print( "[LZTEST] FAIL: callfunc_begin failed (plugin not loaded?)" )
		return
	}

	callfunc_end()
	server_print( "[LZTEST] Round_boss dispatched OK" )
}

public lzd_report_alive()
{
	// if the boss spawn 45 seconds later crashed the server, we never
	// reach this line — it is the final liveness gate
	server_print( "[LZTEST] alive at t+90s — boss round ran without crash" )
}
