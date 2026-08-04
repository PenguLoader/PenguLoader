import { Component } from 'solid-js'
import { BootBanner } from './BootBanner'
import { UpdateBanner } from './UpdateBanner'

/**
 * Stacks the fixed-bottom notifications so a second one can't land on top of
 * the first. The positioning lives here rather than in each banner, which is
 * why they render as plain blocks.
 *
 * `z-30` — above page content, below the Settings overlay (z-50), so opening
 * settings covers the stack without any extra wiring.
 *
 * Order is bottom-up urgency: the boot warning sits above the update notice,
 * because one says Pengu isn't working and the other says a newer version
 * exists. With every banner hidden this collapses to a zero-height box and
 * nothing intercepts clicks.
 */
export const Banners: Component = () => (
  <div class="fixed bottom-0 left-0 right-0 z-30 flex flex-col">
    <BootBanner />
    <UpdateBanner />
  </div>
)
