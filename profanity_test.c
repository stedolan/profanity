#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#include "profanity.h"

prof_counter E1 = PROF_COUNTER_INIT("events/e1", "things", "counter");
prof_counter E2 = PROF_COUNTER_INIT("events/e2", "things", "counter");
prof_counter E3 = PROF_COUNTER_INIT("events/e3", "things", "counter");
prof_counter C = PROF_COUNTER_INIT("events/C", "things", "state");

int main() {

  prof_timer_context ctx = PROF_TIMER_CONTEXT_INIT;
  prof_counter T1 = PROF_COUNTER_INIT("T1", "cycles", "timer");
  prof_counter T2 = PROF_COUNTER_INIT("T2", "cycles", "timer");
  prof_counter T3 = PROF_COUNTER_INIT("T3", "cycles", "timer");
  prof_timer_enter(&ctx, &T1);
  while (1) {
    prof_timer_enter(&ctx, &T2);
    // enter timer T2
    for (int i=0; i<20; i++){
      // event E2, i
      E2.update(&E2, i);
      usleep(50000);
    }
    prof_timer_exit(&ctx);
    // exit timer T2
    for (int i=0; i<10; i++){
      // event E1, i
      E1.update(&E1, i);
      usleep(20000);
    }
    prof_timer_enter(&ctx, &T3);
    static volatile int x;
    for (x=0; x<1000000; x++);
    prof_timer_exit(&ctx);

    for (int i=0; i<1000000000; i++){
      E3.update(&E3, 1);
      C.update(&C, 1000000000 - i);
    }
  }
  prof_timer_exit(&ctx);
  // exit timer T1
}
