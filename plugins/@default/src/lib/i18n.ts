import { fallback, translations } from './translations.json';

type TranslationId = keyof typeof translations['en'];

export function t(id: TranslationId, placeholders?: Record<string, string>) {
  const lang = document.body.dataset['lang'] || fallback;
  // @ts-ignore
  const translation = translations[lang] || translations[fallback];
  const text: string = translation[id];
  if (typeof text === 'string' && placeholders) {
    return text.replace(/\%(\w+)\%/g, (_, x) => {
      return placeholders[x] || '';
    });
  }

  return text || `%${id}%`;
}