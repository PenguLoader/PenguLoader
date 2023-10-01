import toast, { ToastOptions } from 'solid-toast';

const options: ToastOptions = {
  position: 'bottom-right',
  duration: 5000
};

window.Toast = {

  success(message) {
    toast.success(message, options);
  },

  error(message) {
    toast.error(message, options)
  },

  promise(promise, msg) {
    return toast.promise(promise, msg, options);
  },
};

export { Toaster, toast } from 'solid-toast';