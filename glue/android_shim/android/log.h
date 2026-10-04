#pragma once
// host-build shim for android/log.h (used when compiling the AMXX chain
// for the CI host with -D__ANDROID__ to exercise the Android code paths)
#include <stdio.h>
#define ANDROID_LOG_INFO  4
#define ANDROID_LOG_WARN  5
#define ANDROID_LOG_ERROR 6
static inline int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
	(void)prio; (void)tag;
	va_list ap; va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	return 0;
}
