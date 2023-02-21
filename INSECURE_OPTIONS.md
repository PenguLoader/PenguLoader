## Insecure options

> For development purposes only. Use at your own risk with ban rate < 2%.

These options help you to bypass default UX security.<br>
To enable them, just edit the `config`:

```ini
[Main]

; Disable all web security features
;   e.g CORS request and many unpleasant web policies
DisableWebSecurity=1

; Allow requests through a proxy
NoProxyServer=1

; Replace default Chromium command line args
ChromiumArgs="--use-flag" "--use-flag-with-value=some_value"
```
