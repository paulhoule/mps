/* 
TEST_HEADER
 id = $HopeName$
 summary = destroy space when messages are on the queue
 language = c
 link = testlib.o rankfmt.o
END_HEADER
*/

#include "testlib.h"
#include "mpscawl.h"
#include "mpscamc.h"
#include "mpsclo.h"
#include "rankfmt.h"

void *stackpointer;

mps_space_t space;

int final_count = 0;

enum {
 FINAL_DISCARD,
 FINAL_REREGISTER,
 FINAL_STORE,
 FINAL_QUEUE
};

mps_message_t mqueue[10000];

int qhd = 0;
int qtl = 0;

static void nq(mps_message_t mess) {
 mqueue[qhd] = mess;
 qhd = (qhd+1) % 10000;
 asserts(qhd != qtl, "No space in message queue.");
}

static int qmt(void) {
 if (qhd == qtl) {
  return 1;
 } else {
  return 0;
 }
}

static int dq(mps_message_t *mess) {
 if (qhd == qtl) {
  return 0;
 } else {
  *mess = mqueue[qtl];
  qtl = (qtl+1) % 10000;
  return 1;
 } 
}

static void process_mess(mps_message_t message, int faction, mps_addr_t *ref) {
 mps_addr_t ffref;

 switch (faction) {
  case FINAL_DISCARD:
   mps_message_discard(space, message);
   break;
  case FINAL_REREGISTER:
   mps_message_finalization_ref(&ffref, space, message);
   mps_finalize(space, &ffref);
   final_count +=1;
   mps_message_discard(space, message);
   break;
  case FINAL_STORE:
   mps_message_finalization_ref(ref, space, message);
   mps_message_discard(space, message);
   break;
  case FINAL_QUEUE:
   nq(message);
   break;
  default:
   asserts(0, "Unknown finalization action.");
 }
}

static void qpoll(mps_addr_t *ref, int faction) {
 mps_message_t message;

 if (dq(&message)) {
  process_mess(message, faction, ref);
 }
}

static void finalpoll(mps_addr_t *ref, int faction) {
 mps_message_t message;

 if (mps_message_get(&message, space, MPS_MESSAGE_TYPE_FINALIZATION)) {
  final_count -=1;
  process_mess(message, faction, ref);
 }
}

static void test(void) {
 mps_pool_t poolamc, poolawl, poollo;
 mps_thr_t thread;
 mps_root_t root0, root1;

 mps_fmt_t format;
 mps_ap_t apamc, apawl, aplo;

 mycell *a, *b, *c, *d;

 long int j;

 cdie(mps_space_create(&space), "create space");

 cdie(mps_thread_reg(&thread, space), "register thread");

 cdie(
  mps_root_create_reg(&root0, space, MPS_RANK_AMBIG, 0, thread,
   mps_stack_scan_ambig, stackpointer, 0),
  "create root");
 
 cdie(
  mps_root_create_table(&root1, space, MPS_RANK_AMBIG, 0, &exfmt_root, 1),
  "create table root");

 cdie(
  mps_fmt_create_A(&format, space, &fmtA),
  "create format");

 cdie(
  mps_pool_create(&poolamc, space, mps_class_amc(), format),
  "create pool");

 cdie(
  mps_pool_create(&poolawl, space, mps_class_awl(), format),
  "create pool");

 cdie(
  mps_pool_create(&poollo, space, mps_class_lo(), format),
  "create pool");

 cdie(
  mps_ap_create(&apawl, poolawl, MPS_RANK_WEAK),
  "create ap");

 cdie(
  mps_ap_create(&apamc, poolamc, MPS_RANK_EXACT),
  "create ap");
 
 cdie(
  mps_ap_create(&aplo, poollo, MPS_RANK_EXACT),
  "create ap");

 mps_message_type_enable(space, mps_message_type_finalization());

/* register loads of objects for finalization (1000*4) */

 a = allocone(apamc, 2, 1);
 b = a;

 for (j=0; j<1000; j++) {
  a = allocone(apamc, 2, MPS_RANK_EXACT);
  c = allocone(apawl, 2, MPS_RANK_WEAK);
  d = allocone(aplo, 2, MPS_RANK_EXACT); /* rank irrelevant here! */
  mps_finalize(space, &a);
  mps_finalize(space, &c);
  mps_finalize(space, &d);
  mps_finalize(space, &d);
  final_count += 4;
 }

/* throw them all away and collect everything */

 a = NULL;
 b = NULL;
 c = NULL;
 d = NULL;

 mps_root_destroy(root0);
 mps_root_destroy(root1);
 comment("Destroyed roots.");

 mps_arena_collect(space);

 while(mps_message_poll(space) == 0) {
  a = allocdumb(apawl, 1024, MPS_RANK_WEAK);
  a = allocdumb(apamc, 1024, MPS_RANK_EXACT);
  a = allocdumb(aplo,  1024, MPS_RANK_EXACT);
  mps_arena_collect(space);
 }

/* how many are left? (n.b. ideally this would be 0 but
   there's no guarantee)
*/

/* now to test leaving messages open for a long time! */

 mps_ap_destroy(apawl);
 mps_ap_destroy(apamc);
 mps_ap_destroy(aplo);
 comment("Destroyed aps.");

 mps_pool_destroy(poolamc);
 mps_pool_destroy(poolawl);
 mps_pool_destroy(poollo);
 comment("Destroyed pools.");

 mps_fmt_destroy(format);
 comment("Destroyed format.");

 mps_thread_dereg(thread);
 comment("Deregistered thread.");

 mps_space_destroy(space);
 comment("Destroyed space.");

}

int main(void) {
 void *m;
 stackpointer=&m; /* hack to get stack pointer */

 easy_tramp(test);
 pass();
 return 0;
}
