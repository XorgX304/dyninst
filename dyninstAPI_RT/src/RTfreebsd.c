/*
 * Copyright (c) 1996-2009 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as "Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/************************************************************************
 * $Id: RTlinux.c,v 1.54 2008/04/11 23:30:44 legendre Exp $
 * RTlinux.c: mutatee-side library function specific to Linux
 ************************************************************************/

#include "dyninstAPI_RT/h/dyninstAPI_RT.h"
#include "dyninstAPI_RT/src/RTthread.h"
#include "dyninstAPI_RT/src/RTcommon.h"
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <dlfcn.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <link.h>

#define NOT_ON_FREEBSD "This function is unimplemented on FreeBSD"

/* FreeBSD libc has stubs so a static version shouldn't need libpthreads */
#include <pthread.h>

/* TODO threading support, mutatee traps */

extern double DYNINSTstaticHeap_512K_lowmemHeap_1[];
extern double DYNINSTstaticHeap_16M_anyHeap_1[];
extern unsigned long sizeOfLowMemHeap1;
extern unsigned long sizeOfAnyHeap1;

static struct trap_mapping_header *getStaticTrapMap(unsigned long addr);

/** RT lib initialization **/

void mark_heaps_exec() {
    RTprintf( "*** Initializing dyninstAPI runtime.\n" );

    /* Grab the page size, to align the heap pointer. */
    long int pageSize = sysconf( _SC_PAGESIZE );
    if( pageSize == 0 || pageSize == - 1 ) {
        fprintf( stderr, "*** Failed to obtain page size, guessing 16K.\n" );
        perror( "mark_heaps_exec" );
        pageSize = 1024 * 16;
    } /* end pageSize initialization */

    /* Align the heap pointer. */
    unsigned long int alignedHeapPointer = (unsigned long int) DYNINSTstaticHeap_16M_anyHeap_1;
    alignedHeapPointer = (alignedHeapPointer) & ~(pageSize - 1);
    unsigned long int adjustedSize = (unsigned long int) DYNINSTstaticHeap_16M_anyHeap_1 - alignedHeapPointer + sizeOfAnyHeap1;

    /* Make the heap's page executable. */
    int result = mprotect( (void *) alignedHeapPointer, (size_t) adjustedSize, PROT_READ | PROT_WRITE | PROT_EXEC );
    if( result != 0 ) {
        fprintf( stderr, "%s[%d]: Couldn't make DYNINSTstaticHeap_16M_anyHeap_1 executable!\n", __FILE__, __LINE__);
        perror( "mark_heaps_exec" );
    }
    RTprintf( "*** Marked memory from 0x%lx to 0x%lx executable.\n", alignedHeapPointer, alignedHeapPointer + adjustedSize );

    /* Mark _both_ heaps executable. */
    alignedHeapPointer = (unsigned long int) DYNINSTstaticHeap_512K_lowmemHeap_1;
    alignedHeapPointer = (alignedHeapPointer) & ~(pageSize - 1);
    adjustedSize = (unsigned long int) DYNINSTstaticHeap_512K_lowmemHeap_1 - alignedHeapPointer + sizeOfLowMemHeap1;

    /* Make the heap's page executable. */
    result = mprotect( (void *) alignedHeapPointer, (size_t) adjustedSize, PROT_READ | PROT_WRITE | PROT_EXEC );
    if( result != 0 ) {
        fprintf( stderr, "%s[%d]: Couldn't make DYNINSTstaticHeap_512K_lowmemHeap_1 executable!\n", __FILE__, __LINE__ );
        perror( "mark_heaps_exec" );
    }
    RTprintf( "*** Marked memory from 0x%lx to 0x%lx executable.\n", alignedHeapPointer, alignedHeapPointer + adjustedSize );
} /* end mark_heaps_exec() */

int DYNINST_sysEntry;
void DYNINSTos_init(int calledByFork, int calledByAttach)
{
    assert(!NOT_ON_FREEBSD);
}

#if defined(cap_binary_rewriter) && !defined(DYNINST_RT_STATIC_LIB)
/* For a static binary, all global constructors are combined in an undefined
 * order. Also, DYNINSTBaseInit must be run after all global constructors have
 * been run. Since the order of global constructors is undefined, DYNINSTBaseInit
 * cannot be run as a constructor in static binaries. Instead, it is run from a
 * special constructor handler that processes all the global constructors in
 * the binary. Leaving this code in would create a global constructor for the
 * function runDYNINSTBaseInit(). See DYNINSTglobal_ctors_handler.
 */ 
extern void DYNINSTBaseInit();
void runDYNINSTBaseInit() __attribute__((constructor));
void runDYNINSTBaseInit()
{
   DYNINSTBaseInit();
}
#endif

/** Dynamic instrumentation support **/

void DYNINSTbreakPoint()
{
    assert(!NOT_ON_FREEBSD);
}

static int failed_breakpoint = 0;
void uncaught_breakpoint(int sig)
{
   failed_breakpoint = 1;
}

void DYNINSTsafeBreakPoint()
{
    assert(!NOT_ON_FREEBSD);
}

/* FreeBSD libc includes dl* functions typically in libdl */
typedef struct dlopen_args {
  const char *libname;
  int mode;
  void *result;
  void *caller;
} dlopen_args_t;

void *(*DYNINST_do_dlopen)(dlopen_args_t *) = NULL;

/*
static int get_dlopen_error() {
    assert(!NOT_ON_FREEBSD);
    return 1;
}
*/

int DYNINSTloadLibrary(char *libname)
{
    assert(!NOT_ON_FREEBSD);
    return 1;
}

/** threading support **/

int dyn_lwp_self()
{
    static int gettid_not_valid = 0;
    int result;
    
    if( gettid_not_valid )
        return getpid();

    long lwp_id;
    result = syscall(SYS_thr_self, &lwp_id);
    if( result && errno == ENOSYS ) {
        gettid_not_valid = 1;
        return getpid();
    }

    return lwp_id;
}

int dyn_pid_self()
{
   return getpid();
}

dyntid_t (*DYNINST_pthread_self)(void);

dyntid_t dyn_pthread_self()
{
   dyntid_t me;
   if (DYNINSTstaticMode) {
      return (dyntid_t) pthread_self();
   }
   if (!DYNINST_pthread_self) {
      return (dyntid_t) DYNINST_SINGLETHREADED;
   }
   me = (*DYNINST_pthread_self)();
   return (dyntid_t) me;
}

/* 
   We reserve index 0 for the initial thread. This value varies by
   platform but is always constant for that platform. Wrap that
   platform-ness here. 
*/
int DYNINST_am_initial_thread( dyntid_t tid ) {
    if( dyn_lwp_self() == getpid() ) {
        return 1;
    }
    return 0;
} /* end DYNINST_am_initial_thread() */

/*
 * This code extracts the lwp, the address of the thread entry function,
 * and the top of the thread's stack. It uses predefined offset to 
 * extract this information from pthread_t, which is usually opaque. 
 * 
 * Hopefully, only one set of offsets should be needed per architecture
 * because there should exist only one version of FreeBSD libc per 
 * FreeBSD version.
 *
 * If different versions are encountered, see the Linux version of this
 * for ideas on how to handle them.
 *
 * Finally, there is a problem that can be used to determine these offsets
 * at the end of this file.
 */
#define READ_OPAQUE(buffer, pos, type) *((type *)(buffer + pos))

typedef struct pthread_offset_t {
    unsigned long lwp_pos;
    unsigned long start_func_pos;
    unsigned long stack_start_pos;
    unsigned long stack_size_pos;
} pthread_offset_t;

#if defined(os_freebsd) && defined(arch_x86_64)
static pthread_offset_t offsets = { 0, 112, 152, 160 };
#elif defined(os_freebsd) && defined(arch_x86)
static pthread_offset_t offsets = { 0, 80, 108, 112};
#else
#error pthread_t offsets undefined for this architecture
#endif

int DYNINSTthreadInfo(BPatch_newThreadEventRecord *ev) {
    static int err_printed = 0;
    unsigned char *buffer = (unsigned char *)ev->tid;

    unsigned long lwp = READ_OPAQUE(buffer, offsets.lwp_pos, unsigned long);
    ev->stack_addr = (void *)(READ_OPAQUE(buffer, offsets.stack_start_pos, unsigned long) + 
        READ_OPAQUE(buffer, offsets.stack_size_pos, unsigned long));
    ev->start_pc = (void *)(READ_OPAQUE(buffer, offsets.start_func_pos, unsigned long));

    if( lwp != ev->lwp && !err_printed ) {
        RTprintf("%s[%d]: Failed to parse pthread_t information. Making a best effort guess.\n",
                __FILE__, __LINE__);
        err_printed = 1;
    }

    return 1;
}

/** trap based instrumentation **/

#if defined(cap_mutatee_traps)

#include <ucontext.h>

/* XXX This will compile, but it does not work yet */

/* XXX This current only works for amd64 FreeBSD -- needs some ifdefs for i386 */

extern void dyninstSetupContext(ucontext_t *context, unsigned long flags, void *retPoint);
extern unsigned long dyninstTrapTableUsed;
extern unsigned long dyninstTrapTableVersion;
extern trapMapping_t *dyninstTrapTable;
extern unsigned long dyninstTrapTableIsSorted;

/**
 * Called by the SIGTRAP handler, dyninstTrapHandler.  This function is 
 * closly intwined with dyninstTrapHandler, don't modify one without 
 * understanding the other.
 *
 * This function sets up the calling context that was passed to the
 * SIGTRAP handler so that control will be redirected to our instrumentation
 * when we do the setcontext call.
 * 
 * There are a couple things that make this more difficult than it should be:
 *   1. The OS provided calling context is similar to the GLIBC calling context,
 *      but not compatible.  We'll create a new GLIBC compatible context and
 *      copy the possibly stomped registers from the OS context into it.  The
 *      incompatiblities seem to deal with FP and other special purpose registers.
 *   2. setcontext doesn't restore the flags register.  Thus dyninstTrapHandler
 *      will save the flags register first thing and pass us its value in the
 *      flags parameter.  We'll then push the instrumentation entry and flags
 *      onto the context's stack.  Instead of transfering control straight to the
 *      instrumentation, we'll actually go back to dyninstTrapHandler, which will
 *      do a popf/ret to restore flags and go to instrumentation.  The 'retPoint'
 *      parameter is the address in dyninstTrapHandler the popf/ret can be found.
 **/
void dyninstSetupContext(ucontext_t *context, unsigned long flags, void *retPoint)
{
   ucontext_t newcontext;
   unsigned long *orig_sp;
   void *orig_ip;
   void *trap_to;

   getcontext(&newcontext);
   
   //Set up the 'context' parameter so that when we restore 'context' control
   // will get transfered to our instrumentation.
   newcontext.uc_mcontext = context->uc_mcontext;

   orig_sp = (unsigned long *) context->uc_mcontext.mc_rsp;
   orig_ip = (void *) context->uc_mcontext.mc_rip;

   assert(orig_ip);

   //Set up the PC to go to the 'ret_point' in RTsignal-x86.s
   newcontext.uc_mcontext.mc_rip = (unsigned long) retPoint;

   //simulate a "push" of the flags and instrumentation entry points onto
   // the stack.
   if (DYNINSTstaticMode) {
      unsigned long zero = 0;
      unsigned long one = 1;
      struct trap_mapping_header *hdr = getStaticTrapMap((unsigned long) orig_ip);
      assert(hdr);
      trapMapping_t *mapping = &(hdr->traps[0]);
      trap_to = dyninstTrapTranslate(orig_ip, 
                                     (unsigned long *) &hdr->num_entries, 
                                     &zero, 
                                     (volatile trapMapping_t **) &mapping,
                                     &one);
   }
   else {
      trap_to = dyninstTrapTranslate(orig_ip, 
                                     &dyninstTrapTableUsed,
                                     &dyninstTrapTableVersion,
                                     (volatile trapMapping_t **) &dyninstTrapTable,
                                     &dyninstTrapTableIsSorted);
                                     
   }
   *(orig_sp - 1) = (unsigned long) trap_to;
   *(orig_sp - 2) = flags;
   unsigned shift = 2;
#if defined(arch_x86_64) && !defined(MUTATEE_32)
   *(orig_sp - 3) = context->uc_mcontext.mc_r11;
   *(orig_sp - 4) = context->uc_mcontext.mc_r10;
   *(orig_sp - 5) = context->uc_mcontext.mc_rax;
   shift = 5;
#else
#error mutatee traps unavailable on this architecture
#endif
   newcontext.uc_mcontext.mc_rsp = (unsigned long) (orig_sp - shift);

   //Restore the context.  This will move all the register values of 
   // context into the actual registers and transfer control away from
   // this function.  This function shouldn't actually return.
   setcontext(&newcontext);
   assert(0);
}

#if defined(cap_binary_rewriter)

extern struct r_debug _r_debug;
struct r_debug _r_debug __attribute__ ((weak));

#define NUM_LIBRARIES 512 //Important, max number of rewritten libraries

#define WORD_SIZE (8 * sizeof(unsigned))
#define NUM_LIBRARIES_BITMASK_SIZE (1 + NUM_LIBRARIES / WORD_SIZE)
struct trap_mapping_header *all_headers[NUM_LIBRARIES];

static unsigned all_headers_current[NUM_LIBRARIES_BITMASK_SIZE];
static unsigned all_headers_last[NUM_LIBRARIES_BITMASK_SIZE];

#if !defined(arch_x86_64) || defined(MUTATEE_32)
typedef Elf32_Dyn ElfX_Dyn;
#else
typedef Elf64_Dyn ElfX_Dyn;
#endif

static int parse_libs();
static int parse_link_map(struct link_map *l);
static void clear_unloaded_libs();

static void set_bit(unsigned *bit_mask, int bit, char value);
static void clear_bitmask(unsigned *bit_mask);
static unsigned get_next_free_bitmask(unsigned *bit_mask, int last_pos);
static unsigned get_next_set_bitmask(unsigned *bit_mask, int last_pos);

static tc_lock_t trap_mapping_lock;

static struct trap_mapping_header *getStaticTrapMap(unsigned long addr)
{
   struct trap_mapping_header *header;
   int i;
   
   tc_lock_lock(&trap_mapping_lock);
   parse_libs();

   i = -1;
   for (;;) {
      i = get_next_set_bitmask(all_headers_current, i);
      assert(i >= 0 && i <= NUM_LIBRARIES);
      if (i == NUM_LIBRARIES) {
         header = NULL;
         goto done;
      }
      header = all_headers[i];
      if (addr >= header->low_entry && addr <= header->high_entry) {
         goto done;
      }
   }  
 done:
   tc_lock_unlock(&trap_mapping_lock);
   return header;
}

static int parse_libs()
{
   struct link_map *l_current;

   l_current = _r_debug.r_map;
   if (!l_current)
      return -1;

   clear_bitmask(all_headers_current);
   while (l_current) {
      parse_link_map(l_current);
      l_current = l_current->l_next;
   }
   clear_unloaded_libs();

   return 0;
}

//parse_link_map return values
#define PARSED 0
#define NOT_REWRITTEN 1
#define ALREADY_PARSED 2
#define ERROR_INTERNAL -1
#define ERROR_FULL -2
static int parse_link_map(struct link_map *l) 
{
   ElfX_Dyn *dynamic_ptr;
   struct trap_mapping_header *header;
   unsigned int i, new_pos;

   dynamic_ptr = (ElfX_Dyn *) l->l_ld;
   if (!dynamic_ptr)
      return -1;

   assert(sizeof(dynamic_ptr->d_un.d_ptr) == sizeof(void *));
   for (; dynamic_ptr->d_tag != DT_NULL && dynamic_ptr->d_tag != DT_DYNINST; dynamic_ptr++);
   if (dynamic_ptr->d_tag == DT_NULL) {
      return NOT_REWRITTEN;
   }

   header = (struct trap_mapping_header *) (dynamic_ptr->d_un.d_val + l->l_addr);
   
   if (header->signature != TRAP_HEADER_SIG)
      return ERROR_INTERNAL;
   if (header->pos != -1) {
      set_bit(all_headers_current, header->pos, 1);
      assert(all_headers[header->pos] == header);
      return ALREADY_PARSED;
   }
 
   for (i = 0; i < header->num_entries; i++)
   {
      header->traps[i].source = (void *) (((unsigned long) header->traps[i].source) + l->l_addr);
      header->traps[i].target = (void *) (((unsigned long) header->traps[i].target) + l->l_addr);
      if (!header->low_entry || header->low_entry > (unsigned long) header->traps[i].source)
         header->low_entry = (unsigned long) header->traps[i].source;
      if (!header->high_entry || header->high_entry < (unsigned long) header->traps[i].source)
         header->high_entry = (unsigned long) header->traps[i].source;
   }

   new_pos = get_next_free_bitmask(all_headers_last, -1);
   assert(new_pos >= 0 && new_pos < NUM_LIBRARIES);
   if (new_pos == NUM_LIBRARIES)
      return ERROR_FULL;

   header->pos = new_pos;
   all_headers[new_pos] = header;
   set_bit(all_headers_current, new_pos, 1);
   set_bit(all_headers_last, new_pos, 1);

   return PARSED;
}

static void clear_unloaded_libs()
{
   unsigned i;
   for (i = 0; i<NUM_LIBRARIES_BITMASK_SIZE; i++)
   {
      all_headers_last[i] = all_headers_current[i];
   }
}

static void set_bit(unsigned *bit_mask, int bit, char value) {
   assert(bit < NUM_LIBRARIES);
   unsigned *word = bit_mask + bit / WORD_SIZE;
   unsigned shift = bit % WORD_SIZE;
   if (value) {
      *word |= (1 << shift);
   }
   else {
      *word &= ~(1 << shift);
   }
}

static void clear_bitmask(unsigned *bit_mask) {
   unsigned i;
   for (i = 0; i < NUM_LIBRARIES_BITMASK_SIZE; i++) {
      bit_mask[i] = 0;
   }
}

static unsigned get_next_free_bitmask(unsigned *bit_mask, int last_pos) {
   unsigned i, j;
   j = last_pos+1;
   i = j / WORD_SIZE;
   for (; j < NUM_LIBRARIES; i++) {
      if (bit_mask[i] == (unsigned) -1) {
         j += WORD_SIZE;
         continue;
      }
      for (;;) {
         if (!((1 << (j % WORD_SIZE) & bit_mask[i]))) {
            return j;
         }
         j++;
         if (j % WORD_SIZE == 0) {
            break;
         }
      }
   }
   return NUM_LIBRARIES;
}

static unsigned get_next_set_bitmask(unsigned *bit_mask, int last_pos) {
   unsigned i, j;
   j = last_pos+1;
   i = j / WORD_SIZE;
   for (; j < NUM_LIBRARIES; i++) {
      if (bit_mask[i] == (unsigned) 0) {
         j += WORD_SIZE;
         continue;
      }
      for (;;) {
         if ((1 << (j % WORD_SIZE) & bit_mask[i])) {
            return j;
         }
         j++;
         if (j % WORD_SIZE == 0) {
            break;
         }
      }
   }
   return NUM_LIBRARIES;
}

#endif

#endif /* cap_mutatee_traps */

/*
 * A program to determine the offsets of certain thread structures on FreeBSD
 *
 * This program should be compiled with the headers from the libthr library from
 * /usr/src. This can be installed using sysinstall. The following arguments 
 * should be added to the compile once these headers are installed.
 *
 * -I/usr/src/lib/libthr/arch/amd64/include -I/usr/src/lib/libthr/thread
 *
 * Change amd64 to what ever is appropriate.

#include <pthread.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "thr_private.h"

pthread_attr_t attr;

void *foo(void *f) {
    unsigned long stack_addr;
    void *(*start_func)(void *);
    unsigned long tid;

    // Get all the values
    syscall(SYS_thr_self, &tid);

    start_func = foo;

    asm("mov %%rbp,%0" : "=r" (stack_addr));

    pthread_t threadSelf = pthread_self();

    printf("TID: %u == %u\n", tid, threadSelf->tid);
    printf("STACK: 0x%lx == 0x%lx\n", stack_addr, threadSelf->attr.stackaddr_attr + threadSelf->attr.stacksize_attr);
    printf("START: 0x%lx == 0x%lx\n", (unsigned long)start_func, (unsigned long)threadSelf->start_routine);

    unsigned char *ptr = (unsigned char *)threadSelf;
    unsigned long tidVal = *((unsigned long *)(ptr + offsetof(struct pthread, tid)));
    unsigned long stackAddrVal = *((unsigned long *)(ptr + offsetof(struct pthread, attr) + offsetof(struct pthread_attr, stackaddr_attr)));
    unsigned long stackSizeVal = *((unsigned long *)(ptr + offsetof(struct pthread, attr) + offsetof(struct pthread_attr, stacksize_attr)));
    unsigned long startFuncVal = *((unsigned long *)(ptr + offsetof(struct pthread, start_routine)));

    printf("TID = %u, offset = %u\n", tidVal, offsetof(struct pthread, tid));
    printf("STACK = 0x%lx, offset = %u\n", stackAddrVal, offsetof(struct pthread, attr) + offsetof(struct pthread_attr, stackaddr_attr));
    printf("SIZE = 0x%lx, offset = %u\n", stackSizeVal, offsetof(struct pthread, attr) + offsetof(struct pthread_attr, stacksize_attr));
    printf("START = 0x%lx, offset = %u\n", startFuncVal, offsetof(struct pthread, start_routine));

    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t t;
    void *result;
    pthread_attr_init(&attr);
    pthread_create(&t, &attr, foo, NULL);
    pthread_join(t, &result);

    return 0;
}
*/