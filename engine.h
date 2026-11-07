#ifndef ENGINE_HEADER
#define ENGINE_HEADER

#include <MiniFB_types.h>
#include <bits/pthreadtypes.h>
#include <stdatomic.h>
#include <stddef.h>

typedef struct mfb_window mfb_window_t;

typedef struct rt_job {
  signed int i, j;
} rt_job_t;

typedef struct rt_ctx {
  rt_job_t *jobs;
  pthread_t thread;
  atomic_bool start, done;
} rt_ctx_t;

typedef struct rt_engine_inst {
  mfb_window_t *window;
  unsigned char *buf;
  rt_ctx_t *render_pool;
  size_t thread_cnt;
  unsigned int width, height, framerate;
} rt_engine_inst_t;

rt_engine_inst_t create_engine_inst(int width, int height, int framerate);

void close_engine_inst(rt_engine_inst_t *engine);

bool is_pressed(int key);

bool was_just_pressed(int key);

bool run_once(rt_engine_inst_t *engine);
#endif
