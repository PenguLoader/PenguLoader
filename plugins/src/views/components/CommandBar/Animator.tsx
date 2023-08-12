import { children, createEffect, createSignal, on, onCleanup } from 'solid-js';
import { useRoot } from './root';
import { VisualState } from './types';

const appearanceAnimationKeyframes = [
  { opacity: '0', transform: 'scale(.95)', },
  { opacity: '1', transform: 'scale(1.05)' },
  { opacity: '1', transform: 'scale(1)' },
];

const bumpAnimationKeyframes = [
  { transform: 'scale(1)', },
  { transform: 'scale(.96)', },
  { transform: 'scale(1)', },
];

export function Animator(props) {

  let outerRef: HTMLDivElement;
  let innerRef: HTMLDivElement;
  
  const resolved = children(() => props.children);

  const { visualState, setVisualState } = useRoot();
  const enterMs = 100;
  const exitMs = 100;

  createEffect(on(visualState, (state) => {
    if (state === VisualState.Showing) return;

    const anim = outerRef.animate(appearanceAnimationKeyframes, {
      duration: state === VisualState.AnimatingIn ? enterMs : exitMs,
      easing: state === VisualState.AnimatingOut ? 'ease-in' : 'ease-out',
      direction: state === VisualState.AnimatingOut ? 'reverse' : 'normal',
      fill: 'forwards',
    });

    anim.addEventListener('finish', () => {
      setVisualState((state) => {
        const visible = state === VisualState.AnimatingIn || state === VisualState.Showing;
        return visible ? VisualState.Showing : VisualState.Hidden;
      });
    });
  }));

  const [previousHeight, setPreviousHeight] = createSignal(0);
  createEffect(() => {
    if (visualState() === VisualState.Showing) {
      if (!outerRef || !innerRef) {
        return;
      }

      const ro = new ResizeObserver((entries) => {
        for (const entry of entries) {
          const cr = entry.contentRect;

          if (!previousHeight()) {
            setPreviousHeight(cr.height);
          }

          outerRef.animate(
            [
              { height: `${previousHeight()}px`, },
              { height: `${cr.height}px`, },
            ],
            {
              duration: enterMs / 2,
              easing: 'ease-out',
              fill: 'forwards',
            }
          );

          setPreviousHeight(cr.height);
        }
      });

      ro.observe(innerRef);
      onCleanup(() => ro.unobserve(innerRef));
    }
  });

  const [firstRender, setFirstRender] = createSignal(true);
  createEffect(() => {
    if (firstRender()) {
      setFirstRender(false);
      return;
    }
    outerRef.animate(bumpAnimationKeyframes, {
      duration: enterMs,
      easing: 'ease-out',
    });
  });

  return (
    <div
      ref={outerRef!}
      class="w-full h-full"
      style={appearanceAnimationKeyframes[0]}
    >
      <div ref={innerRef!}>
        {resolved()}
      </div>
    </div>
  )
}