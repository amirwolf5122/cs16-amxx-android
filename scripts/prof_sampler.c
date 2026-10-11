// prof_sampler.c — LD_PRELOAD SIGPROF sampler for spin-loop hunting.
// Samples the interrupted RIP every 10ms of CPU time into /tmp/prof_samples.txt
#define _GNU_SOURCE
#include <signal.h>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static void prof_handler( int sig, siginfo_t *si, void *uc )
{
	ucontext_t *ctx = (ucontext_t *)uc;
	unsigned long rip = (unsigned long)ctx->uc_mcontext.gregs[REG_RIP];
	char buf[64];
	int len = snprintf( buf, sizeof( buf ), "%lx\n", rip );
	int fd = open( "/tmp/prof_samples.txt", O_WRONLY|O_CREAT|O_APPEND, 0644 );
	if( fd >= 0 ) { write( fd, buf, len ); close( fd ); }
}

__attribute__((constructor)) static void prof_init( void )
{
	struct sigaction sa;
	memset( &sa, 0, sizeof( sa ));
	sa.sa_sigaction = prof_handler;
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sigaction( SIGPROF, &sa, NULL );

	struct itimerval it;
	it.it_interval.tv_sec = 0; it.it_interval.tv_usec = 10000;
	it.it_value = it.it_interval;
	setitimer( ITIMER_PROF, &it, NULL );
}
