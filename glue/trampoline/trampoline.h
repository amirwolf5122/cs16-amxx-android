/* libffcall-compatible trampoline API.
 *
 * Drop-in replacement for GNU libffcall's <trampoline.h> limited to the
 * subset hamsandwich needs. Semantics match libffcall exactly:
 *
 *   trampoline = alloc_trampoline(address, variable, data);
 *
 * Calling the trampoline stores `data` into `*variable` and then
 * tail-jumps to `address` with the original call arguments untouched
 * (registers and stack pass through verbatim). hamsandwich uses this to
 * set the global `Hook *hook` before dispatching into its HC_* callback,
 * which has the same signature as the hooked member function.
 */

#ifndef HAM_TRAMPOLINE_ANDROID_H
#define HAM_TRAMPOLINE_ANDROID_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
typedef int (*__TR_function) (...);
#else
typedef int (*__TR_function) ();
#endif

extern __TR_function alloc_trampoline (__TR_function address, void* variable, void* data);
extern void free_trampoline (__TR_function tramp);
extern int is_trampoline (void* tramp);

#ifdef __cplusplus
}
#endif

#endif /* HAM_TRAMPOLINE_ANDROID_H */
