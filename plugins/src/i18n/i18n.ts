import { flatten, translator } from '@solid-primitives/i18n';
import { createMemo, createSignal } from 'solid-js';

import { en_US } from './en';
import { zh_CN, zh_TW } from './zh';

type Locale = 'en_US' | 'zh_CN' | 'zh_TW';

const dictionaries = {
  en_US: en_US,
  zh_CN: zh_CN,
  zh_TW: zh_TW,
};

const RiotClientLocale = (function getRiotClientLocale() {
  // @ts-ignore
  // for testing only
  if (!window.riotInvoke) return 'en_US';

  // This is safe because this api has a fallback to en_US
  // and nobody launches the LCU without RiotClient
  const request = new XMLHttpRequest();
  request.open('GET', '//riotclient/product-integration/v1/locale', false);
  request.send(null);
  return JSON.parse(request.responseText);
})();

const clientLocale =
  RiotClientLocale in dictionaries ? (RiotClientLocale as Locale) : 'en_US';

const [locale, setLocale] = createSignal<Locale>(clientLocale);
const dict = createMemo(() => flatten(dictionaries[locale()]));
export const penguI18n = translator(dict);