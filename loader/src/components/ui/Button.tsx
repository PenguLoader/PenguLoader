import { Component, JSX, splitProps } from 'solid-js'
import { twMerge } from 'tailwind-merge'
import { cva } from 'class-variance-authority'

const buttonVariants = cva(
  [
    'inline-flex items-center justify-center rounded-md text-sm font-medium ring-offset-background transition-colors',
    'focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2',
    'disabled:pointer-events-none disabled:opacity-50',
  ],
  {
    variants: {
      variant: {
        default: 'bg-primary text-primary-foreground hover:bg-primary/90',
        outline: 'border border-foreground/20 hover:bg-foreground hover:text-secondary',
        secondary: 'bg-secondary text-secondary-foreground hover:bg-secondary/80',
        link: 'text-primary underline-offset-4 hover:underline',
      },
      size: {
        default: 'h-10 px-4 py-2',
        sm: 'h-9 rounded-md px-3',
        lg: 'h-11 rounded-md px-8'
      }
    },
    defaultVariants: {
      variant: 'default',
      size: 'default',
    }
  }
)

interface Props extends JSX.HTMLAttributes<HTMLButtonElement> {
  disabled?: boolean
  variant?: 'default' | 'outline' | 'link'
  size?: 'default' | 'sm' | 'lg'
}

export const Button: Component<Props> = (props) => {
  const [local, rest] = splitProps(props, ['class', 'variant', 'size'])
  return (
    <button
      class={twMerge(buttonVariants({ variant: local.variant, size: local.size }), local.class)}
      {...rest}
    />
  )
}