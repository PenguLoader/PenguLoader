/* load theme from URL */
function injectCSS(url) {
  const link = document.createElement('link');
  link.setAttribute('rel', 'stylesheet');
  link.setAttribute('type', 'text/css');
  link.setAttribute('href', url);
  document.body.appendChild(link);
}

window.addEventListener('load', () => {
  const url = 'https://link/to/your/css/file.css';
  /*           ^----- put your link here */
  /*           the server must support HTTPS, otherwise the theme will be denied */
  injectCSS(url);
});