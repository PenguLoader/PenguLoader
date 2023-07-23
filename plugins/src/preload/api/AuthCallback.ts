// import native from './native';

// window.AuthCallback = {

//   createURL() {
//     return native.CreateAuthCallbackURL();
//   },

//   readResponse(url, timeout) {
//     if (typeof timeout !== 'number' || timeout <= 0) {
//       timeout = 180000;
//     }
    
//     return new Promise(resolve => {
//       let fired = false;

//       native.AddAuthCallback(url, response => {
//         fired = true;
//         resolve(response);
//       });

//       setTimeout(() => {
//         native.RemoveAuthCallback(url);
//         if (!fired) {
//           resolve(null);
//         }
//       }, timeout);
//     });
//   }
// }