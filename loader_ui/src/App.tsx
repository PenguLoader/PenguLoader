import type { Component } from 'solid-js';

console.log('App loaded');

const App: Component = () => {
  return (
    <p class="text-4xl text-green-700 text-center py-20">
      Hello{' '}
      <a
        class="text-pink-600 hover:font-bold hover:border-1"
        href="https://antfu.me/posts/reimagine-atomic-css"
        target="atomic-css"
      >
        Atomic CSS
      </a>
      !
    </p>
  );
};

export default App;
