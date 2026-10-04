// segv_probe.c — LD_PRELOAD SIGSEGV diagnostic for the AMXX Android project.
// Installs a SIGSEGV handler that records the faulting RIP, the return address
// at [RSP] (null function-pointer calls leave the caller's address on the
// stack), a frame-pointer walk, /proc/self/maps and then exits, so the crash
// site can be resolved with addr2line even without gdb.
#define _GNU_SOURCE
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

static void segv_handler( int sig, siginfo_t *si, void *uc )
{
        char buf[256];
        int fd = open( "/tmp/segv_report.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644 );
        ucontext_t *ctx = (ucontext_t *)uc;
        greg_t *g = ctx->uc_mcontext.gregs;
        unsigned long rip = (unsigned long)g[REG_RIP];
        unsigned long rsp = (unsigned long)g[REG_RSP];
        unsigned long rbp = (unsigned long)g[REG_RBP];

        if( fd < 0 ) _exit( 139 );

#define W(s) write( fd, s, strlen( s ))
        snprintf( buf, sizeof( buf ),
                "signal=%d addr=%p code=%d\nRIP=%#lx RSP=%#lx RBP=%#lx\n",
                sig, si->si_addr, si->si_code, rip, rsp, rbp );
        W( buf );
        snprintf( buf, sizeof( buf ),
                "RAX=%#lx RBX=%#lx RCX=%#lx RDX=%#lx RSI=%#lx RDI=%#lx\n",
                (unsigned long)g[REG_RAX], (unsigned long)g[REG_RBX],
                (unsigned long)g[REG_RCX], (unsigned long)g[REG_RDX],
                (unsigned long)g[REG_RSI], (unsigned long)g[REG_RDI] );
        W( buf );

        // return address pushed by the CALL instruction (call [reg] through NULL)
        unsigned long retaddr = 0, retaddr2 = 0;
        if( rsp )
        {
                retaddr = *(unsigned long *)rsp;
                snprintf( buf, sizeof( buf ), "RET@RSP=%#lx\n", retaddr );
                W( buf );
                if( rbp && rbp != rsp )
                {
                        unsigned long *rb = (unsigned long *)rbp;
                        // rbp chain: [rbp]=saved rbp, [rbp+8]=return address
                        unsigned long chain[8];
                        int n = 0;
                        unsigned long cur = rbp;
                        for( int i = 0; i < 8; i++ )
                        {
                                if( cur < 0x1000 || (cur & 7) ) break;
                                unsigned long *p = (unsigned long *)cur;
                                unsigned long next = p[0];
                                unsigned long ra = p[1];
                                if( !ra ) break;
                                chain[n++] = ra;
                                if( next <= cur || next - cur > 0x100000 ) break;
                                cur = next;
                        }
                        for( int i = 0; i < n; i++ )
                        {
                                snprintf( buf, sizeof( buf ), "CHAIN%d=%#lx\n", i, chain[i] );
                                W( buf );
                        }
                        (void)retaddr2;
                }
        }

        // dump g_engfuncs table of a library given via env SEGV_PROBE_SYM=libname:symoff
        {
                const char *spec = getenv( "SEGV_PROBE_SYM" );
                if( spec )
                {
                        char libname[256];
                        unsigned long symoff = 0;
                        const char *colon = strchr( spec, ':' );
                        if( colon )
                        {
                                size_t ln = colon - spec;
                                if( ln >= sizeof( libname )) ln = sizeof( libname ) - 1;
                                memcpy( libname, spec, ln );
                                libname[ln] = 0;
                                symoff = strtoul( colon + 1, NULL, 0 );

                                int mfd = open( "/proc/self/maps", O_RDONLY );
                                if( mfd >= 0 )
                                {
                                        static char mbuf[262144];
                                        ssize_t tr = 0, r;
                                        while( tr < (ssize_t)sizeof( mbuf ) - 1 && ( r = read( mfd, mbuf + tr, sizeof( mbuf ) - 1 - tr )) > 0 )
                                                tr += r;
                                        close( mfd );
                                        mbuf[tr > 0 ? tr : 0] = 0;

                                        unsigned long lo = 0, hi = 0;
                                        char *save = NULL, *linep = mbuf, *l;
                                        while(( l = strtok_r( linep, "\n", &save )) != NULL )
                                        {
                                                linep = NULL;
                                                unsigned long a, b;
                                                char path[400];
                                                path[0] = 0;
                                                if( sscanf( l, "%lx-%lx %*s %*s %*s %*s %399s", &a, &b, path ) == 3 )
                                                {
                                                        if( strstr( path, libname ) && lo == 0 )
                                                        {
                                                                lo = a;
                                                        }
                                                        if( strstr( path, libname ) && b > hi )
                                                        {
                                                                hi = b;
                                                        }
                                                }
                                        }
                                        if( lo )
                                        {
                                                unsigned long *tab = (unsigned long *)( lo + symoff );
                                                snprintf( buf, sizeof( buf ), "== TABLE %s+0x%lx (%p) entries:\n", libname, symoff, (void*)tab );
                                                W( buf );
                                                for( int i = 0; i < 159; i++ )
                                                {
                                                        snprintf( buf, sizeof( buf ), "  [%3d] 0x%03lx = %#lx\n", i, (unsigned long)i * 8, tab[i] );
                                                        W( buf );
                                                }
                                        }
                                        else
                                        {
                                                snprintf( buf, sizeof( buf ), "== TABLE: library %s not found in maps (lo=%lx)\n", libname, lo );
                                                W( buf );
                                        }
                                }
                        }
                }
        }

        // map range containing RIP and RET@RSP so we can compute library offsets
        int mfd = open( "/proc/self/maps", O_RDONLY );
        if( mfd >= 0 )
        {
                char line[512];
                ssize_t r;
                while(( r = read( mfd, line, sizeof( line ) - 1 )) > 0 )
                {
                        line[r] = 0;
                        write( fd, line, r );
                }
                close( mfd );
        }
        close( fd );
        _exit( 139 );
}

__attribute__((constructor))
static void segv_probe_init( void )
{
        struct sigaction sa;
        memset( &sa, 0, sizeof( sa ));
        sa.sa_sigaction = segv_handler;
        sa.sa_flags = SA_SIGINFO;
        sigaction( SIGSEGV, &sa, NULL );
        sigaction( SIGBUS, &sa, NULL );
        sigaction( SIGILL, &sa, NULL );
}
