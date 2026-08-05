import './api';
import './super-potato';
import './load-hooks';
import './shared';
import { rcp } from './rcp';
import './loader';

// Pre-warm the RCP registry for the plugin views/ waits on.
//
// Push-style RCP requires the subscription to exist before LCUX announces.
// Everything in this file runs synchronously inside OnContextCreated, whereas
// views/ is now a lazily imported chunk that can evaluate after the announce —
// so the subscription lives here and views/ only awaits its fulfillment.
//
// Registered after './loader' so `rcp-fe-common-libs` still subscribes first,
// preserving the order the single-bundle build had.
rcp.preInit('rcp-fe-lol-shared-components', () => { });

// @ts-ignore
window.Pengu.version = __VERSION__;
Object.freeze(window.Pengu);