import toast, { ToastOptions } from 'solid-toast';

const options: ToastOptions = {
  position: 'bottom-right',
  duration: 5000
};

window.Toast = {

  success(message, options) {
    if (window.Pengu.silentMode)
      return console.log(`Silent mode enabled, user won't recieve this toast`);
    toast.success(message, options);
  },

  error(message, options) {
    if (window.Pengu.silentMode)
      return console.log(`Silent mode enabled, user won't recieve this toast`)
    toast.error(message, options)
  },

  promise(promise, msg) {
    if (window.Pengu.silentMode) {
      return console.log("Silent mode enabled, user won't recieve this toast");
    }
    return toast.promise(promise, msg, options);
  },
};

export { Toaster, toast } from 'solid-toast';
