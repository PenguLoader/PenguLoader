/// <reference path="Primordials.d.ts" />
/// <reference path="Pengu.d.ts" />

export {}; // Make this a module

declare global {
    var Pengu: Pengu;

    interface Window {
        Pengu: Pengu;
    }
}