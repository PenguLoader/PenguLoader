## Insecure options

> For development purposes only.
> Use at your own risk! Ban rate <= 3% 🤔

<br>

We also provide insecure options to bypass CEF's web secuity.

To enable them, just put these options into `config.cfg`:

```ini
[Main]
DisableWebSecurity=1
```

- Disable all web security features, e.g like CORS request and many unpleasant web policies.

```ini
[Main]
IgnoreCertificateErrors=1
```

- Ignore all error/invalid SSL certificates.
