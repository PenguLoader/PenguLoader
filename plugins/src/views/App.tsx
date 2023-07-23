import { onMount } from "solid-js";
import { Welcome } from "./components/Welcome";
import { Toaster, toast } from "solid-toast";

export default function App() {

  onMount(() => {
    toast.success('Pengu Loader is active!', {
      position: 'bottom-right',
      duration: 7000
    });
  });

  return (
    <div>
      <Welcome />
      <Toaster
        gutter={8}
        position="bottom-right"
      />
    </div>
  )
}