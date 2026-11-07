#include <MiniFB.h>
#include <bits/pthreadtypes.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"

#define RT_KILL_THREAD (rt_job_t){-1, 0}

typedef struct mfb_window mfb_window_t;
// INPUT HANDLING SECTION
unsigned char keystate[64] = {}, prev_keystate[64] = {};

bool is_pressed(int key) {
  int shift = key % 8;
  return (keystate[key / 8] & (1 << shift)) >> shift;
}

bool was_just_pressed_raw(int key) {
  int shift = key % 8;
  return (prev_keystate[key / 8] & (1 << shift)) >> shift;
}

// i could genericize the update behavior but unneeded, increases call sight
// complexity, and this is fine
bool was_just_pressed(int key) {
  int presskey = is_pressed(key) + (was_just_pressed_raw(key) << 1);
  char key_mask = 1 << (key % 8);

  switch (presskey) {
  case 1:
    prev_keystate[key / 8] &= ~key_mask;
    prev_keystate[key / 8] |= key_mask;
    return true;
    break;
  case 2:
    prev_keystate[key / 8] &= ~key_mask;
    break;
  }
  return false;
}

void update_keystate(mfb_window_t *window, mfb_key key, mfb_key_mod bits,
                     bool pressed) {
  if (key >= 0 && key < 512) {
    char key_mask = 1 << (key % 8);
    keystate[key / 8] &= ~key_mask;
    if (pressed)
      keystate[key / 8] |= key_mask;
  }
}

// engine stuff section
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

rt_engine_inst_t create_engine_inst(unsigned int width, unsigned int height,
                                    unsigned int framerate) {
  unsigned int total_pixels = width * height;
  mfb_window_t *window = mfb_open("C_ATOMS", width, height);
  size_t thread_cnt = (size_t)sysconf(_SC_NPROCESSORS_ONLN);

  if (window == NULL)
    goto close_window;

  mfb_set_target_fps(framerate);
  mfb_set_keyboard_callback(window, update_keystate);

  // setting buffers
  unsigned char *buf = (unsigned char *)malloc(4 * total_pixels);
  if (buf == NULL)
    goto close_backbuf;

  rt_ctx_t *thread_pool = (rt_ctx_t *)calloc(thread_cnt, sizeof(rt_ctx_t));
  if (thread_pool == NULL)
    goto close_thread_pool;

  size_t jobs_per_thread =
      (size_t)ceil((double)total_pixels / (double)thread_cnt);

  size_t jobs_total = jobs_per_thread * thread_cnt;

  rt_job_t *rt_jobs = (rt_job_t *)calloc(jobs_total, sizeof(rt_job_t));
  if (rt_jobs == NULL)
    goto close_job_buf;

  struct setup {
    rt_job_t *queue;
    atomic_bool *start, *done;
    unsigned char *canvas;
    size_t job_cnt;
  };

  // prototype for dispatch_ray_pthread function in raytracer
  void *dispatch_ray_pthread(void *input);

  for (size_t i = 0; i < thread_cnt; i++) {
    rt_ctx_t *curr_thread = thread_pool + i;

    curr_thread->jobs = rt_jobs + i * jobs_per_thread;

    struct setup tmp = (struct setup){curr_thread->jobs, &curr_thread->start,
                                      &curr_thread->done, buf, jobs_per_thread};

    if (pthread_create(&curr_thread->thread, NULL, dispatch_ray_pthread,
                       (void *)&tmp) != 0) {
      for (int t = 0; t < i; t++) {
        rt_ctx_t *to_del = thread_pool + t;

        *to_del->jobs = RT_KILL_THREAD;
        signal(&to_del->start);
        pthread_join(to_del->thread, NULL);
      }

      goto close_job_buf;
    }

    // on thread create tmp needs to be unpacked and saved to local thread
    // stack,and as such, this stack frame needs to be kept alive long enough
    // for the tmp to not go out of scope. otherwise could malloc it but nah, no
    // thanks m8, i hate the heap
    spin_wait(&curr_thread->done);

    // sets the tmp done to true, otherwise deadlock: deadman danny, valve
    // software
    signal(&curr_thread->done);
  }
  return (rt_engine_inst_t){window, buf,    thread_pool, thread_cnt,
                            width,  height, framerate};

close_job_buf:
  free((void *)rt_jobs);
close_thread_pool:
  free((void *)thread_pool);
close_backbuf:
  free((void *)buf);
close_window:
  mfb_close(window);
  return (rt_engine_inst_t){NULL};
}

void close_engine_inst(rt_engine_inst_t *engine) {

  for (size_t i = 0; i < engine->thread_cnt; i++) {
    rt_ctx_t *curr_thread = engine->render_pool + i;
    *curr_thread->jobs = RT_KILL_THREAD;
    signal(&curr_thread->start);
    pthread_join(curr_thread->thread, NULL);
  }

  free((void *)(engine->render_pool)->jobs);

  free((void *)engine->render_pool);

  free((void *)engine->buf);

  mfb_close(engine->window);
}

bool run_once(rt_engine_inst_t *engine) {
  if (was_just_pressed(81))
    return true;

  return false;
}
