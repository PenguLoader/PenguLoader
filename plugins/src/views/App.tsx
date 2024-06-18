import { Toaster, toast } from './components/Toaster';
import { CommandBar } from './components/CommandBar';
import { Welcome } from './components/Welcome';

export default function App() {
  return (
    <div>
      <Welcome />
      <CommandBar />
      <div onClick={() => {
        toast.remove();
      }}>
        <Toaster
          gutter={8}
          position="bottom-right"
        />
      </div>
    </div>
  )
}