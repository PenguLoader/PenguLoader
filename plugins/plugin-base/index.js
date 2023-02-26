/* this plugin will show a banner image
  when your League ready and disappear after 10s */

/* import from utils.js */
import { gretting } from './utils';
/* decorate our HTML with css */
import './assets/style.css';

window.addEventListener('load', () => {
  /* print gretting */
  console.log(gretting);

  /* create div elements */
  const wrapper = document.createElement('div');
  const banner = document.createElement('div');
  // add classes
  banner.classList.add('test-banner');
  wrapper.classList.add('test-wrapper');
  // append to body
  wrapper.appendChild(banner);
  document.body.appendChild(wrapper);

  // the banner will be removed after 10s
  setTimeout(() => {
    wrapper.remove();
  }, 10000);
});