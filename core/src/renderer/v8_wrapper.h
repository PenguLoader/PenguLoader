#pragma once
#include "include/capi/cef_v8_capi.h"

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

struct V8Promise : V8ValueBase
{
    inline void addRef()
    {
        _.base.add_ref(&_.base);
    }

    inline bool release()
    {
        return _.base.release(&_.base);
    }

    inline bool resolve(V8Value *arg)
    {
        return _.resolve_promise(&_, (cef_v8value_t *)arg);
    }

    inline bool reject(const cef_string_t *err)
    {
        return _.reject_promise(&_, err);
    }

    static inline V8Promise *create()
    {
        return (V8Promise *)cef_v8value_create_promise();
    }
};

typedef V8Value *(*V8FunctionHandler)(V8Value *const argv[], int argc);

struct V8HandlerFunctionEntry
{
    const char *name;
    V8FunctionHandler func;
};
