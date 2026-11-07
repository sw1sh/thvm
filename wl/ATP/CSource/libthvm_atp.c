// libthvm_atp: thvm's equational prover as a LibraryLink library for the
// WolframInstitute`ATP` package (wl/ATP/Kernel/ATP.wl).  One translation unit
// over the runtime (src/thvm.c) and the prover's bridge (thvmlink_atp.c),
// without THVMLink's own entries, so the tensor and GPU code they reach is
// left out of the binary.  It registers its kept-system manager under its
// own name, ThvmAtpSystem, so it loads beside THVMLink in one kernel.

#include "WolframLibrary.h"
#include "WolframNumericArrayLibrary.h"
#include "../../../src/thvm.c"

EXTERN_C DLLEXPORT mint WolframLibrary_getVersion(void) {
  return WolframLibraryVersion;
}

static void atp_sys_manager(WolframLibraryData libData, mbool mode, mint id);

EXTERN_C DLLEXPORT int WolframLibrary_initialize(WolframLibraryData libData) {
  libData->registerLibraryExpressionManager("ThvmAtpSystem", atp_sys_manager);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT void WolframLibrary_uninitialize(WolframLibraryData libData) {
  (void)libData;
  if (HEAP != NULL) thvm_free();
}

// (Re)initialize the runtime: a fresh heap.
EXTERN_C DLLEXPORT int thvm_atp_lib_init(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  if (HEAP != NULL) thvm_free();
  thvm_init();
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

#include "../../THVMLink/CSource/thvmlink_atp.c"
