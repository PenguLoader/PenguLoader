## Insecure options

> For development purposes only. Use at your own risk with ban rate < 2%.

These options help you to bypass default UX security.<br>
To enable them, just edit the `config`:

```ini
[Main]

; Disable all web security features
;   e.g CORS request and many unpleasant web policies
DisableWebSecurity=1

; Ignore all error/invalid SSL certificates
;	i.e requesting to self-assigned SSL/untrusted HTTPS server
IgnoreCertificateErrors=1

; Allow requests through a proxy, for debugging purposes only
NoProxyServer=0

; Append custom Chromium command line args
ChromiumArgs=--disable-gpu --some-flag "--flag-with-value=some_value"

```
