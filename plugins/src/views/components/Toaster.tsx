import toast, { ToastOptions } from 'solid-toast';

const options: ToastOptions = {
  position: 'bottom-right',
  duration: 5000
};

window.Toast = {

  success(message, options) {
    if (window.Pengu.silentMode)
      return console.warn(`Suppressed following message since slient mode is on: ${message}`);
    toast.success(message, options);
  },

  error(message, options) {
    if (window.Pengu.silentMode)
      return console.warn(`Suppressed following message since slient mode is on: ${message}`);
    toast.error(message, options)
  },

  promise(promise, msg) {
    if (window.Pengu.silentMode) {
      return console.warn(`Suppressed following message since slient mode is on: ${msg}`);
    }
    return toast.promise(promise, msg, options);
  },
};

export { Toaster, toast } from 'solid-toast';
