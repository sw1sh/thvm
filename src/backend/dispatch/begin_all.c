// backend/dispatch/begin_all.c - open a backend dispatch batch on
// every installed backend that supports one.

fn void backend_dispatch_begin_all(void) {
  for (u32 i = 0; i < THVM_MAX_BACKENDS; i++) {
    Backend *b = CURRENT_CTX->backends[i];
    if (b != NULL && b->dispatch_begin != NULL) {
      b->dispatch_begin();
    }
  }
}
