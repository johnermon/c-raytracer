#ifndef utils
#define utils

#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>

typedef struct josiesync {
  atomic_bool spin;
  sem_t *sem;
} josiesync_t;

inline static void josie_spin_wait(josiesync_t *wait) {
  while (!wait->spin)
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#endif
#if defined(__arm__) || defined(__aarch64__)
  __yield();
#endif

  wait->spin = false;
}

inline static void josie_signal_spin(josiesync_t *signal) {
  signal->spin = true;
}

inline static void josie_sem_wait(josiesync_t *wait) { sem_wait(wait->sem); }

inline static void josie_sem_signal(josiesync_t *signal) {
  sem_post(signal->sem);
};

inline static void spin_wait(atomic_bool *wait) {
  while (!*wait)
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#endif
#if defined(__arm__) || defined(__aarch64__)
  __yield();
#endif

  *wait = false;
}

inline static void signal(atomic_bool *wait) { *wait = true; }

#endif
