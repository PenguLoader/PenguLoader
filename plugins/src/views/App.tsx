import { Welcome } from "./components/Welcome";
import { Toaster } from "solid-toast";

export default function App() {
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