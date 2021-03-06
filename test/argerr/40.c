/* 
TEST_HEADER
 id = $Id$
 summary = unitSize > extendBy for pool_create (MFS)
 language = c
 link = testlib.o
END_HEADER
*/

#include "testlib.h"
#include "mpscmfs.h"
#include "arg.h"

static void test(void *stack_pointer)
{
 mps_arena_t arena;
 mps_pool_t pool;
 mps_thr_t thread;

 cdie(mps_arena_create(&arena, mps_arena_class_vm(), mmqaArenaSIZE), "create arena");

 cdie(mps_thread_reg(&thread, arena), "register thread");

 cdie(
  mps_pool_create(
     &pool, arena, mps_class_mfs(), (size_t) 32, (size_t) 33), 
  "create pool");

 mps_pool_destroy(pool);

}

int main(void)
{
 run_test(test);
 pass();
 return 0;
}
