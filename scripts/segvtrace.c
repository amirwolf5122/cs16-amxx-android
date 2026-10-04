// LD_PRELOAD SIGSEGV/SIGBUS/SIGABRT tracer: prints a raw backtrace with
// module(+offset) lines to stderr and /tmp/segvtrace.txt so the crashing
// library can be identified without gdb.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>

static char g_altstack[ 1024 * 512 ];

static void handler( int sig, siginfo_t *si, void *uc )
{
        void  *bt[128];
        int    n = backtrace( bt, 128 );
        char  **syms = backtrace_symbols( bt, n );
        FILE  *f = fopen( "/tmp/segvtrace.txt", "w" );

        Dl_info info;
        fprintf( f, "=== segvtrace sig=%d addr=%p ===\n", sig, si->si_addr );
        fprintf( stderr, "\n=== segvtrace sig=%d addr=%p ===\n", sig, si->si_addr );
        for( int i = 0; i < n; i++ )
        {
                const char *mod = "?";
                void       *base = NULL;
                if( dladdr( bt[i], &info ) && info.dli_fname )
                {
                        mod  = info.dli_fname;
                        base = info.dli_fbase;
                }
                fprintf( f, "#%02d %p %s(+0x%tx) %s\n", i, bt[i], mod,
                        ( char * )bt[i] - ( char * )base, syms ? syms[i] : "" );
                fprintf( stderr, "#%02d %p %s(+0x%tx)\n", i, bt[i], mod, ( char * )bt[i] - ( char * )base );
        }
        fclose( f );
        // dump full maps for post-mortem resolution
        FILE *m = fopen( "/proc/self/maps", "r" );
        FILE *o = fopen( "/tmp/segvmaps.txt", "w" );
        if( m && o )
        {
                char buf[ 8192 ];
                size_t n;
                while(( n = fread( buf, 1, sizeof( buf ), m )) > 0 )
                        fwrite( buf, 1, n, o );
        }
        if( m ) fclose( m );
        if( o ) fclose( o );
        _exit( 142 );
}

static void install( void )
{
        struct sigaction sa;
        memset( &sa, 0, sizeof( sa ));
        sa.sa_sigaction = handler;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigaction( SIGSEGV, &sa, NULL );
        sigaction( SIGBUS, &sa, NULL );
        sigaction( SIGABRT, &sa, NULL );
        sigaction( SIGILL, &sa, NULL );
        sigaction( SIGFPE, &sa, NULL );
}

// the engine installs its own SIGSEGV handler at init -- keep re-arming ours
// from a background thread so the tracer wins the race
static void *rearm_thread( void *arg )
{
        ( void )arg;
        for(;;)
        {
                install();
                sleep( 1 );
        }
        return NULL;
}

__attribute__((constructor)) static void setup( void )
{
        pthread_t tid;
        stack_t ss;
        memset( &ss, 0, sizeof( ss ));
        ss.ss_sp = g_altstack;
        ss.ss_size = sizeof( g_altstack );
        ss.ss_flags = 0;
        sigaltstack( &ss, NULL );
        install();
        pthread_create( &tid, NULL, rearm_thread, NULL );
}

