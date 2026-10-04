// send a raw reserved svc id (29) exactly like a GoldSrc-brained mod
// would -- the server-side veto must reroute it so the client never sees it.
#include <amxmodx>

public plugin_init()
{
	register_plugin( "Svc29Test", "1.0", "AmirWolf" )
	set_task( 5.0, "send_raw_29" )
}

public send_raw_29()
{
	server_print( "[SVC29] sending raw svc id 29..." )
	message_begin( MSG_BROADCAST, 29 )
	write_byte( 1 )
	write_coord( 0 )
	write_coord( 0 )
	write_coord( 0 )
	message_end()
	server_print( "[SVC29] sent ok" )
}
