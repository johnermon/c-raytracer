#include <unistd.h>

// engine includes
#include "engine.h"
#include "raytracer.h"

#define FRAMERATE 60
#define WIDTH 800
#define HEIGHT 800

int main() {
  rt_engine_inst_t engine = create_engine_inst(WIDTH, HEIGHT, FRAMERATE);

  // engine.window is null on creation failure
  if (engine.window == NULL)
    return 1;

  for (;;) {
    if (run_once(&engine))
      break;
    render(&engine);
    // temporary, will create a uh, delta time thingy once i know ray tracing is
    // actually working
    // usleep(1000000 / FRAMERATE);
  }

  close_engine_inst(&engine);
  return 0;
}
