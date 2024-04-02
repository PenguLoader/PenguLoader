import { rcp } from "./RCP.js";
import { socket } from "./socket.js";

document.addEventListener("Pengu.importmap.ready", function(){
    for (const plugin of Pengu.plugins) {
        import(`https://plugins/${plugin}`, {}).then(function(module) {
            if (typeof module.init == "function") module.init({rcp, socket })
        })
    }
}, { once: true });