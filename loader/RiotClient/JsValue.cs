using System;
using System.Collections.Generic;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Pengu.Loader.RiotClient
{
    partial class JsValue
    {
        private enum ValueType
        {
            Null,
            Boolean,
            Number,
            String,
            Object,
            Array,
        }

        private ValueType Type { get; }
        private object? Value { get; }

        public static JsValue Null { get; } = new JsValue(ValueType.Null, null);
        public static JsValue True { get; } = new JsValue(ValueType.Boolean, null);
        public static JsValue False { get; } = new JsValue(ValueType.Boolean, null);

        public static JsValue Boolean(bool value)
        {
            return value ? True : False;
        }

        public static JsValue Number(double value)
        {
            return new JsValue(ValueType.Number, value);
        }

        public static JsValue String(string value)
        {
            return new JsValue(ValueType.String, value);
        }

        public static JsValue Object(Dictionary<string, JsValue> value)
        {
            return new JsValue(ValueType.Object, value);
        }

        public static JsValue Array(List<JsValue> value)
        {
            return new JsValue(ValueType.Array, value);
        }

        private JsValue(ValueType type, object? value)
        {
            Type = type;
            Value = value;
        }

        private string SerializeObject(Dictionary<string, JsValue>? obj)
        {
            if (obj is null)
                return "{}";

            var entries = new List<string>();
            foreach (var kvp in obj)
            {
                entries.Add($"{JsonSerializer.Serialize(kvp.Key, JsValueJson.Default.String)}:{kvp.Value.ToString()}");
            }

            return "{" + string.Join(",", entries) + "}";
        }

        private string SerializeArray(List<JsValue>? arr)
        {
            if (arr is null)
                return "[]";

            var entries = new List<string>();
            foreach (var item in arr)
            {
                entries.Add(item.ToString());
            }

            return "[" + string.Join(",", entries) + "]";
        }

        public override string ToString()
        {
            return Type switch
            {
                ValueType.Null => "null",
                ValueType.Boolean => (Value is true) ? "true" : "false",
                ValueType.Number => Convert.ToString(Value, System.Globalization.CultureInfo.InvariantCulture) ?? "0",
                ValueType.String => JsonSerializer.Serialize((Value as string)!, JsValueJson.Default.String),
                ValueType.Object => SerializeObject(Value as Dictionary<string, JsValue>),
                ValueType.Array => SerializeArray(Value as List<JsValue>),
                _ => "null",
            };
        }

        [JsonSerializable(typeof(string))]
        partial class JsValueJson : JsonSerializerContext
        {
        }
    }
}