
/**
 * Returns a promise that resolves when the DOM is fully loaded.
 */
export function whenDomReady(): Promise<void> {
  return new Promise((resolve) => {
    if (document.readyState === 'complete' || document.readyState === 'interactive') {
      resolve()
    } else {
      window.addEventListener('DOMContentLoaded', () => resolve(), { once: true })
    }
  })
}

/**
 * Waits for an element matching the selector to appear in the DOM.
 */
export function waitForElement(selector: string): Promise<Element> {
  return new Promise((resolve) => {
    const element = document.querySelector(selector)
    if (element) {
      resolve(element)
      return
    }
    const observer = new MutationObserver(() => {
      const el = document.querySelector(selector)
      if (el) {
        resolve(el)
        observer.disconnect()
      }
    })
    observer.observe(document.body, { childList: true, subtree: true })
  })
}

/**
 * Watches for an element matching the selector to appear in the DOM.
 */
export function watchElement(selector: string, callback: (el: Element) => void) {
  let last: Element | null = document.querySelector(selector)
  if (last) {
    callback(last)
  }
  const observer = new MutationObserver(() => {
    const el = document.querySelector(selector)
    if (el !== last) {
      last = el
      if (el) {
        callback(el)
      }
    }
  })
  observer.observe(document.documentElement, {
    childList: true,
    subtree: true
  })
  return () => observer.disconnect()
}