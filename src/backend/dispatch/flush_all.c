// backend/dispatch/flush_all.c - wait for pending backend dispatch
// batches.  Used before host-side buffer reads and by autotune timing.

fn void backend_dispatch_flush_all(void) {
  for (u32 i = 0; i < THVM_MAX_BACKENDS; i++) {
    Backend *b = CURRENT_CTX->backends[i];
    if (b != NULL && b->dispatch_flush != NULL) {
      b->dispatch_flush();
    }
  }
}
