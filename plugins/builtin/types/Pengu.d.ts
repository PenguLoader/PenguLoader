/// <reference path="Primordials.d.ts" />

interface Pengu {
    version: string
    superPotato: boolean
    plugins: string[]
    __plugins: Record<string, string>;
    primordials: Primordials;
}