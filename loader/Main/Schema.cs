using System;
using System.Collections.Generic;

namespace PenguLoader.Main
{
#pragma warning disable CS0649

    static class Schema
    {
        public class RiotClientInstalls
        {
            public IDictionary<string, string> associated_client;
            public string rc_default;
            public string rc_live;
        }
    }

#pragma warning restore CS0649
}