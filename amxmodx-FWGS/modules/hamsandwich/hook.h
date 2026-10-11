// vim: set ts=4 sw=4 tw=99 noet:
//
// AMX Mod X, based on AMX Mod by Aleksander Naszko ("OLO").
// Copyright (C) The AMX Mod X Development Team.
//
// This software is licensed under the GNU General Public License, version 3 or higher.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://alliedmods.net/amxmodx-license

//
// Ham Sandwich Module
//

#ifndef HOOK_H
#define HOOK_H

#include "forward.h"
#ifdef USE_LIBFFCALL
extern "C"
{
#include <trampoline.h>
#include <sys/mman.h>
}
#else
#include "Trampolines.h"
#endif

#include <amtl/am-vector.h>
#include <amtl/am-string.h>

#define ALIGN(ar) ((intptr_t)ar & ~(sysconf(_SC_PAGESIZE)-1))

#ifdef USE_LIBFFCALL
class Hook;
extern Hook *hook;
#endif

// This is just a simple container for data so I only have to add 1 extra 
// parameter to calls that get trampolined
static void TestTrampoline();

class Hook
{
private:
        int              trampSize;
        
public:
        ke::Vector<Forward *> pre;     // pre forwards
        ke::Vector<Forward *> post;    // post forwards
        void                    *func;    // original function
        void           **vtable;  // vtable of the original location
        int              entry;   // vtable entry of the function
        void            *target;  // target function being called (the hook)
        int              exec;    // 1 when this hook is in execution
        int              del;     // 1 if this hook should be destroyed after exec
        char                    *ent;     // ent name that's being hooked
        void            *tramp;   // trampoline for this hook
        const char              *methodname;

        Hook(void **vtable_, int entry_, void *target_, bool voidcall, bool retbuf, int paramcount, char *name, const char *methodname) :
                func(NULL), vtable(vtable_), entry(entry_), target(target_), exec(0), del(0), tramp(NULL), trampSize(0), methodname(methodname)
                {
                        // original function is vtable[entry]
                        // to not make the compiler whine, cast vtable to intptr_t **
                        // was int** — on LP64 (arm64) that truncated the
                        // 64-bit vtable/trampoline pointers to 32 bits
                        intptr_t **ivtable=(intptr_t **)vtable;
                        func=(void *)ivtable[entry];

                        // now install a trampoline
                        // (int thiscall, int voidcall, int paramcount, void *extraptr)

#ifdef USE_LIBFFCALL
                        tramp = (void*)alloc_trampoline((__TR_function)target, &hook, this );
                        if (!tramp)
                        {
                                // no executable memory available (W^X denied both
                                // fallbacks) — leave the vtable alone instead of
                                // writing NULL into it and crashing the server
                                MF_Log("[HAM] alloc_trampoline failed for %s, hook disabled", methodname ? methodname : "?");
                        }
#else
                        tramp = CreateGenericTrampoline(true, voidcall, retbuf, paramcount, (void*)this, target, &trampSize);
#endif

                        // Insert into vtable
#if defined(_WIN32)
                        DWORD OldFlags;
                        VirtualProtect(&ivtable[entry],sizeof(int*),PAGE_READWRITE,&OldFlags);
#elif defined(__linux__) || defined(__APPLE__)
                        void *addr = (void *)ALIGN(&ivtable[entry]);
                        // the vtable page only needs write access to swap
                        // the function pointer. PROT_EXEC made mprotect fail on
                        // Android 10+ (W^X), which then left the page read-only
                        // and crashed the server on the actual pointer write.
                        mprotect(addr,sysconf(_SC_PAGESIZE),PROT_READ|PROT_WRITE);
#endif
                        if (tramp)
                                ivtable[entry]=(intptr_t *)tramp;

#if defined(__linux__) || defined(__APPLE__)
                        // verify the swap actually landed. If
                        // mprotect or the store failed (RELRO/SELinux edge
                        // cases) the slot keeps its original value and every
                        // hook silently stays dead -- log it either way.
                        if ((void *)ivtable[entry] == tramp && tramp)
                                MF_Log("[HAM] hooked %s::%s vtable=%p entry=%d orig=%p tramp=%p (patch verified)",
                                        name, methodname, (void *)vtable, entry, func, tramp);
                        else
                                MF_Log("[HAM] PATCH FAILED for %s::%s vtable=%p entry=%d orig=%p tramp=%p slot-now=%p",
                                        name, methodname, (void *)vtable, entry, func, tramp, (void *)ivtable[entry]);
#endif

                        size_t len=strlen(name);
                        ent=new char[len+1];

                        ke::SafeSprintf(ent, len + 1, "%s", name);
                };

        ~Hook()
        {
                // Insert the original function back into the vtable
                intptr_t **ivtable=(intptr_t **)vtable;

#if defined(_WIN32)
                DWORD OldFlags;
                VirtualProtect(&ivtable[entry],sizeof(int*),PAGE_READWRITE,&OldFlags);
#elif defined(__linux__) || defined(__APPLE__)
                void *addr = (void *)ALIGN(&ivtable[entry]);
                // RW is enough to restore the pointer (see ctor)
                mprotect(addr,sysconf(_SC_PAGESIZE),PROT_READ|PROT_WRITE);
#endif

                ivtable[entry]=(intptr_t *)func;
#if defined(USE_LIBFFCALL)
                if (tramp)
                        free_trampoline( (__TR_function)tramp );
#elif defined(_WIN32)
                VirtualFree(tramp, 0, MEM_RELEASE);
#elif defined(__linux__)
                munmap(tramp, trampSize);
#elif defined(__APPLE__)
                free(tramp);
#endif

                delete[] ent;

                for (size_t i = 0; i < pre.length(); ++i)
                {
                        pre.at(i)->Release();
                }

                for (size_t i = 0; i < post.length(); ++i)
                {
                        post.at(i)->Release();
                }

                pre.clear();
                post.clear();
        }
};

#ifdef USE_LIBFFCALL
static void TestTrampoline()
{
        MF_Log( "Called trampoline %p:%s", hook, hook->methodname);
}
#endif

#endif
