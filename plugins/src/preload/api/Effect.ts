import native from './native';

window.Effect = {

  get current() {
    return <EffectName>native.GetWindowEffect();
  },

  apply(name, options) {
    const previous = native.GetWindowEffect();
    const success = native.SetWindowEffect(name, options);
    if (success) {
      window.dispatchEvent(new CustomEvent('effect-changed', {
        detail: { previous, name, options }
      }));
    }
    return success;
  },

  clear() {
    native.SetWindowEffect(false);
    window.dispatchEvent(new CustomEvent('effect-changed'));
  }
};