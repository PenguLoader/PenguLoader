import { fallback, translations } from './translations.json';

type TranslationId = keyof typeof translations['en'];

function getLanguage() {
  const lang = document.body.dataset['lang'];
  if (lang === 'vn') return 'vi';
  return lang;
};

export function t(id: TranslationId, placeholders?: Record<string, string>) {
  const lang = getLanguage() || fallback;
  // @ts-ignore
  const translation = translations[lang] || translations[fallback];
  const text: string = translation[id];
  if (typeof text === 'string' && placeholders) {
    return text.replace(/\%(\w+)\%/g, (_, x) => {
      return placeholders[x] || `%${x}%`;
    });
  }

  return text || `!${id}!`;
}