import { native } from './native';

const NSVisualEffectMaterial = {
  Titlebar: 3,
  Selection: 4,
  Menu: 5,
  Popover: 6,
  Sidebar: 7,
  HeaderView: 10,
  Sheet: 11,
  WindowBackground: 12,
  HudWindow: 13,
  FullScreenUI: 15,
  Tooltip: 17,
  ContentBackground: 18,
  UnderWindowBackground: 21,
  UnderPageBackground: 22,
}

const WinToMacMaterial = {
  transparent: NSVisualEffectMaterial.UnderWindowBackground,
  blurbehind: NSVisualEffectMaterial.HudWindow,
  acrylic: NSVisualEffectMaterial.FullScreenUI,
  unified: NSVisualEffectMaterial.Popover,
  mica: NSVisualEffectMaterial.HeaderView,
}

const Win11MicaMaterial = {
  auto: 0,
  none: 1,
  mica: 2,
  acrylic: 3,
  tabbed: 4,
}

type EffectName =
  | 'transparent'
  | 'blurbehind'
  | 'acrylic'
  | 'unified'
  | 'mica'
  | 'vibrancy'

const WinBackdropType = {
  transparent: 0,
  blurbehind: 1,
  acrylic: 2,
  unified: 3,
  mica: 4,
}

function parseHexColor(color: string): number {
  if (typeof color === 'string') {
    if (color.startsWith('#')) {
      let hex = color.slice(1)
      let size = hex.length
      let i = 0, step = size > 4 ? 1 : 0

      let r = parseInt(hex[i] + hex[i += step], 16);
      let g = parseInt(hex[++i] + hex[i += step], 16)
      let b = parseInt(hex[++i] + hex[i += step], 16)
      let a = 255

      if (size === 4 || size === 8) {
        a = parseInt(hex[++i] + hex[i += step], 16)
      }

      return ((a << 24) | (b << 16) | (g << 8) | r) >>> 0
    }
  }
  return 0
}

function applyWindowEffectMac(name: EffectName, options) {
  if (name === 'vibrancy') {
    const material = String(options.material)
    const alwaysOn = Boolean(options.alwaysOn)
    if (material in NSVisualEffectMaterial) {
      const state = alwaysOn ? 1 : 0
      native.SetWindowVibrancy(NSVisualEffectMaterial[material], state)
    } else {
      console.warn('Unsupported vibrancy material: %s', material)
    }
  }
  else if (name in WinToMacMaterial) {
    native.SetWindowVibrancy(WinToMacMaterial[name], 0)
  } else {
    console.warn('Unknown window visual effect: %s', name)
  }
}

function applyWindowEffectWin(name: EffectName, options) {
  if (name in WinBackdropType) {
    if (name === 'mica') {
      const material = String(options.material || 'mica')
      if (material in Win11MicaMaterial) {
        native.SetWindowVibrancy(WinBackdropType.mica, Win11MicaMaterial[material])
      } else {
        console.warn('Unsupported mica material: %s', material)
      }
    } else {
      const color = parseHexColor(options.color)
      native.SetWindowVibrancy(WinBackdropType[name], color)
    }
  } else {
    console.warn('Unknown window visual effect: %s', name)
  }
}

window.Effect = {

  apply(name, options) {
    options = options || {}
    if (window.Pengu.isMac) {
      applyWindowEffectMac(name, options)
    } else {
      applyWindowEffectWin(name, options)
    }
  },

  clear() {
    native.SetWindowVibrancy(null);
  },

  setTheme(theme) {
    if (theme === 'light')
      native.SetWindowTheme(false)
    else (theme === 'dark')
    native.SetWindowTheme(true)
  },

}