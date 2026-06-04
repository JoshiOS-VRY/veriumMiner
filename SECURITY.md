# Security Policy

## Reporting a vulnerability

Please report security issues **privately**. Do not open a public GitHub issue
for vulnerabilities.

- Preferred: open a [GitHub private security advisory](https://github.com/JoshiOS-VRY/veriumMiner/security/advisories/new).
- Alternatively, contact the maintainer listed in the project `README.md`.

Include as much detail as you can:

- affected version (`cpuminer --version`) and platform,
- a description of the issue and its impact,
- steps to reproduce or a proof of concept,
- any suggested remediation.

We aim to acknowledge reports within a few days and will coordinate a fix and
disclosure timeline with you.

## Supported versions

Security fixes target the latest released `1.x` version. Please upgrade to the
newest release before reporting.

## Verifying downloads

Every release ships a `SHA256SUMS` file. Always verify your download:

```sh
# Linux
sha256sum -c SHA256SUMS
# macOS
shasum -a 256 -c SHA256SUMS
# Windows (PowerShell)
Get-FileHash .\cpuminer.exe -Algorithm SHA256
```

Release binaries are currently **unsigned**. The release pipeline has a signing
stage wired in (Windows Authenticode + macOS notarization) that activates once
signing certificates are provisioned; until then, checksum verification is the
integrity guarantee.

## Hardening notes

- The monitoring API binds to `127.0.0.1` by default. Remote control commands
  (`quit`, `seturl`) are disabled unless `--api-remote` is explicitly set. Do
  not expose the API port to untrusted networks.
- Wallet/worker credentials and pool-provided strings are JSON-escaped before
  being placed in JSON-RPC frames and API output.
- Run the miner as an unprivileged user where possible (see the systemd unit in
  `contrib/systemd/`).
