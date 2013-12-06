#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>

#include "profanity.h"
#include "mongoose/mongoose.h"

// registration
#define MAX_COUNTERS 1000
struct {
  pthread_mutex_t lock;
  prof_counter* counters[MAX_COUNTERS];
  int ncounters;
} counter_table = {PTHREAD_MUTEX_INITIALIZER};

static void prof_counter_update(prof_counter* cnt, int64_t value) {
  cnt->events++;
  cnt->sum += value;
}

static void prof_counter_update_trace(prof_counter* cnt, int64_t value) {
  printf("%s: %lld %s\n", cnt->name, value, cnt->unit);
  prof_counter_update(cnt, value);
}

void prof_counter_initialise(prof_counter* cnt, int64_t value) {
  pthread_mutex_lock(&counter_table.lock);
  if (cnt->update == &prof_counter_initialise) {
    if (counter_table.ncounters < MAX_COUNTERS) {
      counter_table.counters[counter_table.ncounters++] = cnt;
    } else {
      fprintf(stderr, "too many counters!\n");
    }
    char* trace = getenv("PROFANITY_TRACE");
    if (trace && !strcmp(trace, "1")) {
      cnt->update = &prof_counter_update_trace;
    } else {
      cnt->update = &prof_counter_update;
    }
  }
  pthread_mutex_unlock(&counter_table.lock);

  cnt->update(cnt, value);
}

prof_counter overflow_timer = PROF_COUNTER_INIT("internal/timer_stack_overflow", "cycles");
prof_counter default_timer = PROF_COUNTER_INIT("internal/timer_default", "cycles");

prof_timer_context overhead_context = PROF_TIMER_CONTEXT_INIT;
prof_counter overhead_timer = PROF_COUNTER_INIT("internal/overhead", "cycles");


struct connection {
  struct mg_connection* conn;
  int num_counters_seen;
  int inited;
 
  struct connection* next;
};
pthread_mutex_t conn_lock = PTHREAD_MUTEX_INITIALIZER;
struct connection* connections = 0;


static void websocket_ready_handler(struct mg_connection* mg_conn) {
  struct connection* c = malloc(sizeof(struct connection));
  c->conn = mg_conn;
  c->num_counters_seen = 0;
  c->inited = 0;

  pthread_mutex_lock(&conn_lock);
  c->next = connections;
  connections = c;
  pthread_mutex_unlock(&conn_lock);
}

static void end_request_handler(const struct mg_connection* conn, int statuscode) {
  pthread_mutex_lock(&conn_lock);
  struct connection** p = &connections;
  for (struct connection* c = connections; c; c = c->next) {
    if (c->conn == conn) {
      *p = c->next;
      free(c);
      break;
    }
    p = &c->next;
  }
  pthread_mutex_unlock(&conn_lock);
}

static int websocket_data_handler(struct mg_connection *conn, int op,
                                  char *data, size_t data_len) {
  if (op == 0x8a && data_len == 0) {
    // pong frame, ignore
  } else {
    printf("got frame: %02x\n", op);
  }
  return 1;
}



#define MIN_BUFFER_SIZE 512
#define MAX_HEADER_SIZE 20
static char* send_buffer_base = 0;
static char* send_buffer = 0;
static int send_buffer_sz = 0;

static void ensure_buffer(int required_sz){
  required_sz += MAX_HEADER_SIZE;
  if (send_buffer_sz < required_sz) {
    free(send_buffer_base);
    send_buffer_base = malloc(required_sz);
    send_buffer_sz = required_sz;
    send_buffer = send_buffer_base + MAX_HEADER_SIZE; //FIXME
  }
}

static void write_buffer(struct connection* target, char* end) {
  // Fill in a WebSocket header before the send buffer, then write it
  assert(send_buffer <= end && end < send_buffer_base + send_buffer_sz);
  unsigned len = (unsigned)(end - send_buffer);
  char* op;
  if (len <= 125) {
    send_buffer[-1] = len;
    op = &send_buffer[-2];
  } else if (len <= 1 << 16) {
    send_buffer[-1] = (char)(len & 0xff);
    send_buffer[-2] = (char)((len >> 8) & 0xff);
    send_buffer[-3] = 126;
    op = &send_buffer[-4];
  } else {
    send_buffer[-1] = (char)(len & 0xff);
    send_buffer[-2] = (char)((len >> 8) & 0xff);
    send_buffer[-3] = (char)((len >> 16) & 0xff);
    send_buffer[-4] = (char)((len >> 24) & 0xff);
    send_buffer[-5] = send_buffer[-6] = send_buffer[-7] = send_buffer[-8] = 0;
    op = &send_buffer[-9];
  }
  *op = 0x81; // text frame opcode

  *--op = 0x00;
  *--op = 0x89;  // ping
  int r = mg_write(target->conn, op, end - op);
  if (r < 0) {
    perror("error writing to socket");
  }
}

static void sample() {
  int ncounters;
  pthread_mutex_lock(&counter_table.lock);
  ncounters = counter_table.ncounters;
  pthread_mutex_unlock(&counter_table.lock);
  
  ensure_buffer(ncounters * 50 + MIN_BUFFER_SIZE);

  pthread_mutex_lock(&conn_lock);

  // send any new counters out
  for (struct connection* c = connections; c; c = c->next) {
    if (!c->inited) {
      char* p = send_buffer;
      p += snprintf(p, MIN_BUFFER_SIZE, "S pid={{%d}}", getpid());
      write_buffer(c, p);
      c->inited = 1;
    }
    while (c->num_counters_seen < ncounters) {
      prof_counter* ctr = counter_table.counters[c->num_counters_seen++];
      char* p = send_buffer;
      p += snprintf(p, MIN_BUFFER_SIZE, "A name={{%s}} unit={{%s}}", ctr->name, ctr->unit);
      write_buffer(c, p);
    }
  }

  char* p = send_buffer;
  *p++ = 'D';
  for (int i=0; i<ncounters; i++) {
    prof_counter* ctr = counter_table.counters[i];
    p += sprintf(p, " %020lld %020lld", ctr->events, ctr->sum);
  }
  for (struct connection* c = connections; c; c = c->next) {
    write_buffer(c, p);
  }

  /*
  
  int i;
  //printf("sampling %d sockets\n", nconns);

  for (i = 0; i<nconns; i++) {
    unsigned char buf[40];
    struct timeval tv;
    gettimeofday(&tv, 0);
    unsigned long long now = 
      ((unsigned long long)tv.tv_sec) * 1000 + 
      ((unsigned long long)tv.tv_usec) / 1000;
    buf[0] = 0x89;
    buf[1] = 0x00;
    buf[2] = 0x81;
    buf[3] = snprintf((char *) buf + 4, sizeof(buf) - 4, "%llu %llu", now, events);
    mg_write(conns[i], buf, 4 + buf[3]);
  }
  */
  pthread_mutex_unlock(&conn_lock);
}

static void* server_thread(void* arg) {
  // fixme: proper timing loop needed
  while (1) {
    prof_timer_enter(&overhead_context, &overhead_timer);
    sample();
    prof_timer_exit(&overhead_context);
    usleep(50000);
  }
  return 0;
}


__attribute__((constructor))
void prof_run_server(void) {
  struct mg_context *ctx;
  struct mg_callbacks callbacks = {0};
  const char *options[] = {
    "listening_ports", "8080",
    "document_root", "/home/stephen/projects/prof/root",
    NULL
  };

  callbacks.websocket_ready = websocket_ready_handler;
  callbacks.end_request = end_request_handler;
  callbacks.websocket_data = websocket_data_handler;
  ctx = mg_start(&callbacks, NULL, options);

  pthread_t th;
  pthread_create(&th, 0, server_thread, 0);
  pthread_detach(th);
}
