#ifndef RAYTRACER
#define RAYTRACER

#include "engine.h"

typedef struct vec3 {
  float x, y, z;
} vec3_t;

void render(rt_engine_inst_t *engine_inst);

#endif
