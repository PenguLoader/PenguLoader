using System;
using System.Text.RegularExpressions;

namespace Pengu.Loader.Utils
{
    class HtmlPatcher
    {
        private string _html;

        public HtmlPatcher(string html)
        {
            _html = html;
        }

        public string Html => _html;

        public override string ToString() => _html;

        // Regex to match the <meta http-equiv="Content-Security-Policy" ... content="..."> tag
        // Works with minified HTML, mixed attribute order, single/double/no whitespace.
        private static readonly Regex MetaCspRegex = new Regex(
            @"<meta\b(?=[^>]*\bhttp-equiv\b)(?=[^>]*Content-Security-Policy)[^>]*>",
            RegexOptions.IgnoreCase | RegexOptions.Compiled
        );

        // Regex to extract the content="...csp..." attribute
        private static readonly Regex ContentAttrRegex = new Regex(
            @"content\s*=\s*(?<q>""|')(?<csp>.*?)(\k<q>)",
            RegexOptions.IgnoreCase | RegexOptions.Compiled
        );

        // Adds a new source to the CSP meta tag in the given HTML string.
        public HtmlPatcher AddCspSource(string newSource)
        {
            _html = MetaCspRegex.Replace(_html, match =>
            {
                string metaTag = match.Value;

                // Extract content attribute
                var m = ContentAttrRegex.Match(metaTag);
                if (!m.Success)
                    return metaTag; // no content attribute → return unchanged

                string quote = m.Groups["q"].Value;
                string csp = m.Groups["csp"].Value;

                // Already contains target?
                if (csp.Contains(newSource, StringComparison.OrdinalIgnoreCase))
                    return metaTag; // nothing to add

                // Append new source
                string newCsp = csp + " " + newSource;

                // Replace content attribute inside this meta tag
                string patchedTag = ContentAttrRegex.Replace(metaTag,
                    $"content={quote}{newCsp}{quote}", 1);

                return patchedTag;
            });

            return this;
        }

        // Adds a <script> tag with the given src before </head>.
        public HtmlPatcher AddScriptTag(string src, bool module)
        {
            var scriptTag = module
                ? $"<script type=\"module\" src=\"{src}\"></script>"
                : $"<script src=\"{src}\"></script>";

            // Insert before </head>
            _html = Regex.Replace(_html, @"</head>",
                scriptTag + "</head>", RegexOptions.IgnoreCase);

            return this;
        }
    }
}