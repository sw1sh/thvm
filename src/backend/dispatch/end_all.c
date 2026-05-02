// backend/dispatch/end_all.c - close a backend dispatch batch on
// every installed backend that supports one.

fn void backend_dispatch_end_all(void) {
  for (u32 i = THVM_MAX_BACKENDS; i > 0; i--) {
    Backend *b = CURRENT_CTX->backends[i - 1];
    if (b != NULL && b->dispatch_end != NULL) {
      b->dispatch_end();
    }
  }
}
