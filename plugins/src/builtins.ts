export const builtinNames = new Map<string, string>();
export const builtinUrls = new Map<string, string>();

function bind(name: string, content: string) {
    const blob = new Blob([content], { type: "text/javascript" });
    const url = URL.createObjectURL(blob);
    builtinNames.set(name, url);
    builtinUrls.set(url, name);
}

import fs from "@pengu/fs?raw";
bind("pengu:fs", fs);
import datastore from "@pengu/datastore?raw";
bind("pengu:datastore", datastore);