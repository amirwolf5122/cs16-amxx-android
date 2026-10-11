#include <amxmodx>

public plugin_init()
{
	register_plugin("Hello Test", "1.0", "tester")
	register_clcmd("say hello", "cmdHello")
}

public cmdHello(id)
{
	client_print(id, print_chat, "Hello from compiled plugin!")
	return PLUGIN_HANDLED
}
