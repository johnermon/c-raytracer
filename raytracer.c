#include <math.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "MiniFB.h"
#include "engine.h"
#include "utils.h"

#define RECV_KILL queue->i == -1

typedef struct vec3 {
  float x, y, z;
} vec3_t;

void render(rt_engine_inst_t *engine_inst) {
  size_t height = engine_inst->height, width = engine_inst->width;
  size_t total_pixels = height * width;

  // same formula used to create the thread pool
  size_t jobs_per_thread =
      ceil((double)total_pixels / (double)engine_inst->thread_cnt);

  rt_job_t *job_queue = engine_inst->render_pool->jobs;

  for (size_t t = 0; t < engine_inst->thread_cnt; t++) {
    spin_wait(&(engine_inst->render_pool + t)->done);

    rt_job_t *curr_job_queue = job_queue + jobs_per_thread * t;

    size_t running_total = jobs_per_thread * t;

    for (size_t i = 0; i < jobs_per_thread && running_total + i != total_pixels;
         i++) {
      rt_job_t *curr_job = (curr_job_queue + i);
      // naive scheduling here rn. nice thing about this approach is i
      // can swap this out for smth like a space filling curve if i want
      // immediately better cache locality
      curr_job->i = (running_total + i) % height;
      curr_job->j = (running_total + i) / height;
    }

    signal(&(engine_inst->render_pool + t)->start);
  }

  mfb_update(engine_inst->window, engine_inst->buf);
}

inline void trace_ray(unsigned char *canvas, signed int i, signed int j) {
  *(canvas + (j * 800 + i) * 4) = i % 255;
  *(canvas + ((j * 800 + i) * 4) + 1) = 255 - i % 255;
  *(canvas + ((j * 800 + i) * 4) + 2) = 122 + i % 123;
  *(canvas + ((j * 800 + i) * 4) + 3) = 0;
}

void *dispatch_ray_pthread(void *input) {
  rt_job_t *queue;
  atomic_bool *start, *done;
  unsigned char *canvas;
  size_t job_cnt;

  {
    struct setup {
      rt_job_t *queue;
      atomic_bool *start, *done;
      unsigned char *canvas;
      size_t job_cnt;
    };

    struct setup *job_queue = (struct setup *)input;
    queue = job_queue->queue;
    start = job_queue->start;
    done = job_queue->done;
    canvas = job_queue->canvas;
    job_cnt = job_queue->job_cnt;
  }

  // signals that all variables were passed to the thread allowing the
  // tmp setup var to go out of scope
  signal(done);

  for (;;) {
    spin_wait(start);

    if (RECV_KILL)
      break;

    for (size_t i = 0; i < job_cnt; i++)
      trace_ray(canvas, (queue + i)->i, (queue + i)->j);
    signal(done);
  }
  return NULL;
}
