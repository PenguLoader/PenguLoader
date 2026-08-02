#pragma once

#include "include/capi/cef_v8_capi.h"
#include "include/capi/cef_task_capi.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

struct V8ValueBase
{
    inline cef_v8value_t *ptr()
    {
        return &_;
    }

protected:
    cef_v8value_t _;
};

struct V8Value : V8ValueBase
{
    inline bool isUndefined() { return _.is_undefined(&_); }
    inline bool isNull() { return _.is_null(&_); }

    inline bool isBool() { return _.is_bool(&_); }
    inline bool isInt() { return _.is_int(&_); }
    inline bool isUint() { return _.is_uint(&_); }
    inline bool isDouble() { return _.is_double(&_); }
    inline bool isString() { return _.is_string(&_); }
    inline bool isObject() { return _.is_object(&_); }
    inline bool isArray() { return _.is_array(&_); }
    inline bool isFunction() { return _.is_function(&_); }
    inline bool isPromise() { return _.is_promise(&_); }

    inline bool asBool() { return _.get_bool_value(&_); }
    inline int asInt() { return _.get_int_value(&_); }
    inline uint32_t asUint() { return _.get_uint_value(&_); }
    inline double asDouble() { return _.get_double_value(&_); }
    inline cef_string_userfree_t asString() { return _.get_string_value(&_); }

    inline struct V8Array *asArray() { return reinterpret_cast<struct V8Array *>(&_); }
    inline struct V8Object *asObject() { return reinterpret_cast<struct V8Object *>(&_); }

    static inline V8Value *undefined()
    {
        return (V8Value *)cef_v8value_create_undefined();
    }

    static inline V8Value *null()
    {
        return (V8Value *)cef_v8value_create_null();
    }

    static inline V8Value *boolean(bool value)
    {
        return (V8Value *)cef_v8value_create_bool(value);
    }

    static inline V8Value *number(double value)
    {
        return (V8Value *)cef_v8value_create_double(value);
    }

    static inline V8Value *number(int value)
    {
        return (V8Value *)cef_v8value_create_int(value);
    }

    static inline V8Value *string(const cef_string_t *value)
    {
        return (V8Value *)cef_v8value_create_string(value);
    }

    static inline V8Value *function(const cef_string_t *name, cef_v8handler_t *handler)
    {
        return (V8Value *)cef_v8value_create_function(name, handler);
    }
};

struct V8Array : V8ValueBase
{
    inline int length()
    {
        return _.get_array_length(&_);
    }

    inline V8Value *get(int index)
    {
        return (V8Value *)_.get_value_byindex(&_, index);
    }

    inline void set(int index, V8ValueBase *value)
    {
        _.set_value_byindex(&_, index, (cef_v8value_t *)value);
    }

    static inline V8Array *create(int length)
    {
        return (V8Array *)cef_v8value_create_array(length);
    }
};

struct V8Object : V8ValueBase
{
    inline bool has(const cef_string_t *key)
    {
        return _.has_value_bykey(&_, key);
    }

    inline V8Value *get(const cef_string_t *key)
    {
        return (V8Value *)_.get_value_bykey(&_, key);
    }

    inline void set(const cef_string_t *key, V8ValueBase *value, cef_v8_propertyattribute_t attr)
    {
        _.set_value_bykey(&_, key, (cef_v8value_t *)value, attr);
    }

    static inline V8Object *create()
    {
        return (V8Object *)cef_v8value_create_object(nullptr, nullptr);
    }
};

// =============================================================================
// V8AsyncPool — tiny renderer-side thread pool for off-thread I/O / CPU work.
//
// Workers are detached and never join — process exit kills them. CEF's
// process-exit path is abrupt enough that graceful shutdown isn't worth the
// machinery (matches how the rest of the renderer's helper threads behave).
// =============================================================================

namespace V8AsyncPool
{
    namespace _detail
    {
        inline std::mutex                          mu;
        inline std::condition_variable             cv;
        inline std::queue<std::function<void()>>   tasks;
        inline bool                                started = false;

        inline void worker_loop()
        {
            for (;;)
            {
                std::function<void()> job;
                {
                    std::unique_lock<std::mutex> lock(mu);
                    cv.wait(lock, [] { return !tasks.empty(); });
                    job = std::move(tasks.front());
                    tasks.pop();
                }
                if (job) job();
            }
        }

        inline void ensure_started()
        {
            std::lock_guard<std::mutex> lock(mu);
            if (started) return;
            started = true;
            // Three workers: enough for the rare burst of concurrent async
            // calls (Load + Flush + a Settings save) without spinning a thread
            // per request.
            for (int i = 0; i < 3; ++i)
                std::thread(worker_loop).detach();
        }
    }

    inline void submit(std::function<void()> &&job)
    {
        _detail::ensure_started();
        {
            std::lock_guard<std::mutex> lock(_detail::mu);
            _detail::tasks.push(std::move(job));
        }
        _detail::cv.notify_one();
    }
}

// =============================================================================
// V8PromiseTask — pair a JS Promise with off-thread work.
//
// Lifecycle:
//   1. Construct on the renderer thread inside a V8 callback (the context is
//      already entered). Constructor captures the context + creates a fresh
//      Promise.
//   2. Caller hands `promise()` to JS and calls `execute(runner)` to run the
//      off-thread work on the pool.
//   3. Inside `runner` (worker thread), do the I/O / CPU work, then call
//      `resolve(...)` or `reject(...)`. Both post `this` back to TID_RENDERER.
//   4. On TID_RENDERER, `_execute` enters the context, settles the Promise,
//      and drops its own reference.
//
// Refcounting:
//   - Constructor: ref = 1.
//   - `execute()` adds one for the worker closure, then releases the
//     constructor's ref. Worker holds it during the runner; releases on exit.
//   - `resolve/reject()` -> cef_post_task addrefs; CEF releases after running.
//   - Net: the task self-destructs once both the worker closure exits and the
//     renderer-side _execute has run.
// =============================================================================

class V8PromiseTask : public CefRefCount<cef_task_t>
{
private:
    cef_v8context_t *context_;
    cef_v8value_t *promise_;
    std::function<V8Value *()> resolver_;
    std::string rejection_;
    bool has_rejection_ = false;

    static void CALLBACK _execute(cef_task_t *self)
    {
        reinterpret_cast<V8PromiseTask *>(self)->run_on_renderer();
    }

    /// `cef_v8value_create_promise` is not exported by the macOS CEF
    /// framework, so build the promise by evaluating one instead and drive it
    /// through the same resolve_promise/reject_promise slots. Caller must
    /// already be inside the context.
    cef_v8value_t *create_promise()
    {
#if OS_MAC
        CefStr code("new Promise(() => {})");
        CefStr script_url("pengu://native-promise");
        cef_v8value_t *value = nullptr;
        cef_v8exception_t *exception = nullptr;

        if (!context_->eval(context_, &code, &script_url, 1, &value, &exception) || value == nullptr)
        {
            if (exception != nullptr)
                exception->base.release(&exception->base);

            return cef_v8value_create_undefined();
        }

        return value;
#else
        return cef_v8value_create_promise();
#endif
    }

    void run_on_renderer()
    {
        context_->enter(context_);

        if (has_rejection_)
        {
            CefStr msg(rejection_.c_str(), rejection_.size());
            promise_->reject_promise(promise_, &msg);
        }
        else if (resolver_)
        {
            V8Value *val = resolver_();
            promise_->resolve_promise(promise_, val ? val->ptr() : nullptr);
        }
        else
        {
            promise_->resolve_promise(promise_, nullptr);
        }

        promise_->base.release(&promise_->base);
        context_->exit(context_);
    }

public:
    V8PromiseTask() : CefRefCount(this)
    {
        cef_task_t::execute = _execute;

        context_ = cef_v8context_get_current_context();
        context_->base.add_ref(&context_->base);

        // Per cef_v8_capi.h: callers with a *stored* context reference must
        // wrap V8 allocation in enter()/exit(). We hold a context ref for
        // the deferred TID_RENDERER hop, so honor that contract even on the
        // originating thread — skipping it crashed early renderer init.
        context_->enter(context_);
        promise_ = create_promise();
        promise_->base.add_ref(&promise_->base);
        context_->exit(context_);
    }

    ~V8PromiseTask()
    {
        context_->base.release(&context_->base);
    }

    /// JS Promise to hand back to the renderer.
    inline V8Value *promise() { return reinterpret_cast<V8Value *>(promise_); }

    /// Submit `runner` to the worker pool. `runner` must call `resolve(...)`
    /// or `reject(...)` exactly once before returning.
    ///
    /// Refcount model: the constructor's ref=1 transfers through this call
    /// → worker closure (captured pointer) → `cef_post_task` (takes
    /// ownership per CEF capi convention) → `_execute` on TID_RENDERER →
    /// CEF releases after dispatch. No explicit add_ref/release here — the
    /// transfer chain keeps the ref ≥1 until `_execute` runs, then frees.
    void execute(std::function<void()> &&runner)
    {
        V8AsyncPool::submit(std::move(runner));
    }

    /// Resolve with `undefined`.
    void resolve()
    {
        resolver_ = nullptr;
        has_rejection_ = false;
        cef_post_task(TID_RENDERER, this);
    }

    /// Resolve with whatever `fn` returns. `fn` runs on the renderer thread
    /// inside the captured V8 context — safe to allocate V8 values there.
    void resolve(std::function<V8Value *()> &&fn)
    {
        resolver_ = std::move(fn);
        has_rejection_ = false;
        cef_post_task(TID_RENDERER, this);
    }

    /// Reject with an error message.
    void reject(const std::string &msg)
    {
        resolver_ = nullptr;
        rejection_ = msg;
        has_rejection_ = true;
        cef_post_task(TID_RENDERER, this);
    }
};

typedef V8Value *(*V8FunctionHandler)(V8Value *const argv[], int argc);

struct V8HandlerFunctionEntry
{
    const char *name;
    V8FunctionHandler func;
};
