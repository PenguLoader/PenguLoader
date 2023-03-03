/* @refresh reload */

/* import theme */
import './theme.scss';

/* intialize */
init();

async function init() {
  /* create div elements */
  const wrapper = document.createElement('div');
  const banner = document.createElement('div');
  // add classes
  banner.classList.add('test-banner');
  wrapper.classList.add('test-wrapper');
  // append to body
  wrapper.appendChild(banner);
  document.body.appendChild(wrapper);

  /* put your theme helpers here */
}