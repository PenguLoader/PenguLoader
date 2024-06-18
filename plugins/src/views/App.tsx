import { Toaster, toast } from './components/Toaster';
import { CommandBar } from './components/CommandBar';
import { Welcome } from './components/Welcome';

export default function App() {
  return (
    <div onClick={() => {
      toast.remove();
    }}>
      <Welcome />
      <CommandBar />
      <Toaster
        gutter={8}
        position="bottom-right"
      />
    </div>
  )
}