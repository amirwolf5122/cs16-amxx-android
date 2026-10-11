#!/usr/bin/env python3
"""Generate a working delta.lst for the xash3d dedicated host test.

Parses the engine's own delta field tables out of net_encode.c and emits a
delta.lst with all struct/field names the engine expects. Flag/bit values are
sane defaults (the CI spawn test never encodes real deltas -- those only
matter when a remote client connects -- but struct/field names must be valid
or Delta_InitFields bails out).
"""
import re, sys

def parse(src):
    def table(name):
        m = re.search(r'delta_field_t\s+%s\[\]\s*=\s*\{(.*?)\n\};' % name, src, re.S)
        body = m.group(1)
        entries = []
        # entries look like: { MACRO( fieldname ) } or { MACRO_( name, realmember ) }
        for e in re.finditer(r'\{\s*(\w+)\(\s*([^,)]+?)\s*(?:,\s*([^)]+?))?\s*\)\s*\}', body):
            macro, a, b = e.group(1), e.group(2), e.group(3)
            fieldname = (b if (b and macro.endswith('_')) else a).strip()
            entries.append(fieldname)
        return entries
    return {
        'cmd_fields': table('cmd_fields'),
        'pm_fields': table('pm_fields'),
        'ev_fields': table('ev_fields'),
        'wd_fields': table('wd_fields'),
        'cd_fields': table('cd_fields'),
        'ent_fields': table('ent_fields'),
    }

def flags_for(name):
    n = name
    if 'viewangles' in n or n.startswith('angles') or 'v_angle' in n:
        return 'DT_ANGLE | DT_SIGNED', 16, 4096.0
    if 'origin' in n or 'startpos' in n or 'endpos' in n or 'velocity' in n or 'mins' in n or 'maxs' in n:
        return 'DT_FLOAT | DT_SIGNED', 19, 8192.0
    if 'time' in n:
        return 'DT_TIMEWINDOW_8', 8, 100.0
    if 'color' in n and 'r' in n or 'color' in n and 'g' in n or 'color' in n and 'b' in n:
        return 'DT_FLOAT | DT_SIGNED', 16, 1.0
    if 'scale' in n or 'friction' in n or 'gravity' in n or 'speed' in n or 'accelerate' in n:
        return 'DT_FLOAT | DT_SIGNED', 16, 8.0
    if 'modelindex' in n or 'weaponmodel' in n or 'animtime' in n or 'frame' in n:
        return 'DT_FLOAT', 16, 1.0
    if 'weaponanim' in n or 'aiment' in n or 'owner' in n or 'groundent' in n or 'impact_index' in n:
        return 'DT_INTEGER', 16, 1.0
    if 'impulse' in n or 'buttons' in n or 'weaponselect' in n or 'weaponbits' in n:
        return 'DT_INTEGER', 24, 1.0
    if 'health' in n or 'armorval' in n or 'ammo' in n or 'clip' in n or 'count' in n or 'playerclass' in n:
        return 'DT_SIGNED | DT_SHORT', 16, 1.0
    if 'viewmodel' in n or 'punchangle' in n or 'view_ofs' in n or 'basevelocity' in n or 'punch' in n:
        return 'DT_FLOAT | DT_SIGNED', 16, 8.0
    if 'msec' in n or 'lightlevel' in n or 'lerp_msec' in n:
        return 'DT_BYTE', 8, 1.0
    if 'skyName' in n or 'name' in n or 'classname' in n or 'globalname' in n:
        return 'DT_STRING', 1, 1.0
    if 'flags' in n or 'effects' in n or 'renderfx' in n or 'rendermode' in n or 'movetype' in n or 'solid' in n or 'body' in n or 'skin' in n or 'sequence' in n or 'gaitsequence' in n or 'weaponmodel' in n:
        return 'DT_INTEGER', 32, 1.0
    return 'DT_FLOAT | DT_SIGNED', 16, 8.0

def emit(tables, out):
    lines = []
    def section(name, fields):
        lines.append('%s none' % name)
        lines.append('{')
        for f in fields:
            fl, bits, mult = flags_for(f)
            fm = ('%g' % mult)
            lines.append('DEFINE_DELTA( %s, %s, %d, %s ),' % (f, fl, bits, fm))
        lines.append('}')
    section('usercmd_t', tables['cmd_fields'])
    section('movevars_t', tables['pm_fields'])
    section('event_t', tables['ev_fields'])
    section('weapon_data_t', tables['wd_fields'])
    section('clientdata_t', tables['cd_fields'])
    section('entity_state_t', tables['ent_fields'])
    section('entity_state_player_t', tables['ent_fields'])
    section('custom_entity_state_t', tables['ent_fields'])
    open(out, 'w').write('\n'.join(lines) + '\n')
    print('wrote %s (%d sections)' % (out, 8))

if __name__ == '__main__':
    src = open(sys.argv[1]).read()
    out = sys.argv[2]
    emit(parse(src), out)
