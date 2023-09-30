#if !__cplusplus
using System.Diagnostics;
using System;

namespace PenguLoader
{
    static partial class Program
    {
        public const string VERSION =
#endif
        "1.1.0"
#if !__cplusplus
        ;
        public static string CommitSha1;
    }
}
#endif