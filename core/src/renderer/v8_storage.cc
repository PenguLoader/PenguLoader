#include "pengu.h"
#include "v8_wrapper.h"

#include "sqlite3.h"

#include <cstring>

// =============================================================================
// `window.__native.Storage*` — the per-plugin async key/value store.
//
// See docs/plugin-storage.md. Only the version readout exists so far; it is
// here rather than in a throwaway harness because it is also the thing that
// forces sqlite3 to actually be linked into core.dll. Without a live reference
// the linker's /OPT:REF would discard the whole library and "does core still
// load with sqlite in it" would be a question we had not really asked.
//
// The store itself will be owned by one dedicated thread, not the shared
// V8AsyncPool — see docs/plugin-storage.md section 10 for why.
// =============================================================================

static V8Value *v8_storage_version(V8Value *const args[], int argc)
{
    // sqlite3_libversion() returns a pointer to a static string in the
    // library, so there is nothing to free and no lifetime to manage.
    const char *v = sqlite3_libversion();
    CefStr version(v, std::strlen(v));
    return V8Value::string(&version);
}

V8HandlerFunctionEntry v8_StorageEntries[]
{
    { "StorageVersion", v8_storage_version },
    { nullptr }
};
