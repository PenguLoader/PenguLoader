#pragma once
#include "include/capi/cef_v8_capi.h"
#include "include/capi/cef_task_capi.h"
#include <thread>
#include <functional>
#include <optional>

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
    inline struct V8Promise *asPromise() { return reinterpret_cast<struct V8Promise *>(&_); }

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

class V8PromiseTask : CefRefCount<cef_task_t>
{
private:
    cef_v8context_t *context_;
    cef_v8value_t *promise_;
    std::optional<std::function<V8Value *()>> resolver_;

    static void CALLBACK _execute(cef_task_t *self)
    {
        auto *task = reinterpret_cast<V8PromiseTask *>(self);
        task->execute_in_renderer();
    }

    void execute_in_renderer()
    {
        context_->enter(context_);

        if (resolver_.has_value())
        {
            try
            {
                V8Value *val = resolver_.value()();
                promise_->resolve_promise(promise_, val->ptr());
            }
            catch (const std::string &err)
            {
                CefStr msg(err);
                promise_->reject_promise(promise_, &msg);
            }
        }
        else
        {
            promise_->resolve_promise(promise_, nullptr);
        }

        promise_->base.release(&promise_->base);
        context_->exit(context_);
    }

public:
    V8PromiseTask() : CefRefCount(this), resolver_(std::nullopt)
    {
        cef_task_t::execute = _execute;

        context_ = cef_v8context_get_current_context();
        context_->base.add_ref(&context_->base);

        context_->enter(context_);
        {
            promise_ = cef_v8value_create_promise();
            promise_->base.add_ref(&promise_->base);
        }
        context_->exit(context_);
    }

    ~V8PromiseTask()
    {
        context_->base.release(&context_->base);
    }

    void resolve()
    {
        resolver_ = std::nullopt;
        cef_post_task(TID_RENDERER, this);
    }

    void resolve(std::function<V8Value *()> &&resolver)
    {
        resolver_ = resolver;
        cef_post_task(TID_RENDERER, this);
    }

    void reject(const std::string &err)
    {
        resolve([err]() -> V8Value *
            {
                throw new std::string(err.c_str());
            }
        );
    }

    V8Value *execute(std::function<void()> &&runner)
    {
        std::thread(runner).detach();
        return (V8Value *)promise_;
    }
};

typedef V8Value *(*V8FunctionHandler)(V8Value *const argv[], int argc);

struct V8HandlerFunctionEntry
{
    const char *name;
    V8FunctionHandler func;
};
