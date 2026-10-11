#include <amxmodx>
#include <amxmisc>
#include <fun>

// Sample plugin: +100 HP for admins on round start (test of the compiler)
public plugin_init()
{
	register_plugin("Admin HP Kit", "1.0", "amxx-android")
	register_concmd("amx_hp", "cmdHp", ADMIN_KICK, "<name> - set 100 hp")
}

public cmdHp(id, level, cid)
{
	if (!cmd_access(id, level, cid, 2))
		return PLUGIN_HANDLED

	new arg[32]
	read_argv(1, arg, charsmax(arg))
	new player = cmd_target(id, arg, CMDTARGET_OBEY_IMMUNITY | CMDTARGET_ALLOW_SELF)
	if (!player)
		return PLUGIN_HANDLED

	set_user_health(player, 100)
	client_print(player, print_chat, "* Your health has been restored.")
	return PLUGIN_HANDLED
}
