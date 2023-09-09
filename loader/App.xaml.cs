using System;
using System.Collections.Generic;
using System.Windows;
using PenguLoader.Main;

namespace PenguLoader
{
    public partial class App
    {
        private static readonly Dictionary<string, string> LanguageMap = new Dictionary<string, string>
        {
            { "English", "en-US.xaml" },
            { "Tiếng Việt", "vi-VN.xaml" },
            { "日本語", "ja-JP.xaml" },
            { "中文", "zh-CN.xaml" }
        };

        public static string Language
        {
            get => Config.Language;
            set
            {
                if (value == Config.Language) return;
                Config.Language = value;
                SetLanguage(value);
                LanguageChanged?.Invoke(null, EventArgs.Empty);
            }
        }

        public static string[] Languages
        {
            get
            {
                var keys = LanguageMap.Keys;
                var languages = new string[keys.Count];
                keys.CopyTo(languages, 0);
                return languages;
            }
        }

        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);
            SetLanguage(Config.Language);
        }

        public static event EventHandler LanguageChanged;

        private static void SetLanguage(string lang)
        {
            if (!LanguageMap.ContainsKey(lang))
                lang = "English";

            var dict = new ResourceDictionary();
            var file = LanguageMap[lang];
            dict.Source = new Uri($"Languages/{file}", UriKind.Relative);

            Current.Resources.MergedDictionaries.Add(dict);
        }

        public static string GetTranslation(string key)
        {
            try
            {
                var trans = Current.FindResource(key);
                if (trans is string s)
                    return s;
            }
            catch
            {
                // ignored
            }

            return $"%{key}";
        }
    }
}