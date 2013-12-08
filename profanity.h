#ifndef PROFANITY_H
#define PROFANITY_H

#include <stdint.h>



struct prof_counter;

typedef struct prof_counter {
  const char* name;
  const char* unit;
  const char* type;

  void (*update)(struct prof_counter*, int64_t value);

  int64_t events;
  int64_t sum;
} prof_counter;


void prof_counter_initialise(prof_counter* cnt, int64_t value);



#define PROF_COUNTER_INIT(name, unit, type)     \
  {name, unit, type, &prof_counter_initialise, 0, 0}



#define PROF_TIMER_STACK_SIZE 256
typedef struct {
  prof_counter* timer_stack[PROF_TIMER_STACK_SIZE];
  int next_timer;

  uint64_t last_switch;
} prof_timer_context;

extern prof_counter overflow_timer, default_timer;
#define PROF_TIMER_CONTEXT_INIT \
  {{&overflow_timer, &default_timer}, 2, 0};


extern prof_timer_context prof_global_timer_context;


static inline uint64_t rdtsc (void)
               {
                 unsigned int tickl, tickh;
                 __asm__ __volatile__("rdtsc":"=a"(tickl),"=d"(tickh));
                 return ((unsigned long long)tickh << 32)|tickl;
               }

static inline void prof_timer_switch(prof_timer_context* ctx, prof_counter* from) {
  uint64_t now = rdtsc();
  uint64_t then = ctx->last_switch;
  from->update(from, (int64_t)(now - then));
  ctx->last_switch = now;
}

static inline void prof_timer_enter(prof_timer_context* ctx, prof_counter* t) {
  int next = ctx->next_timer++ & (PROF_TIMER_STACK_SIZE - 1);
  int prev = (next - 1) & (PROF_TIMER_STACK_SIZE - 1);
  ctx->timer_stack[next] = t;
  prof_timer_switch(ctx, ctx->timer_stack[prev]);
}

static inline void prof_timer_exit(prof_timer_context* ctx) {
  int curr = --ctx->next_timer & (PROF_TIMER_STACK_SIZE - 1);
  prof_timer_switch(ctx, ctx->timer_stack[curr]);
}


#define PROF_EVENT(name) do { \
  static prof_counter counter = PROF_COUNTER_INIT(name, "events", "counter"); \
  counter.update(&counter, 1); \
  } while(0)

#define PROF_COUNT(name, unit, value) do { \
  static prof_counter counter = PROF_COUNTER_INIT(name, unit, "counter"); \
  counter.update(&counter, value); \
  } while(0)

#define PROF_TIMER_BEGIN(name) \ 
static prof_counter TIMER = PROF_COUNTER_INIT(name, "cycles", "timer"); \
prof_timer_enter(&prof_global_timer_context, &TIMER)

#define PROF_TIMER_END prof_timer_exit(&prof_global_timer_context);

#endif
