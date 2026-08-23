# Security

## Reporting

Report a suspected vulnerability privately through GitHub's
[Report a vulnerability](https://github.com/kvit-s/sqeezeamp/security/advisories/new)
form rather than as a public issue.

This is a single-maintainer hobby project. Expect a best-effort response rather
than a guaranteed turnaround, and no bounty. If a report is valid and I can fix
it, I will; if I cannot, I would rather say so than leave you waiting.

## What the attack surface actually is

Worth stating plainly, because it is unusually small and that is by design.

**There is no network listener.** SqeezeAmp opens outbound connections to the
Lyrion Music Server you configure, and nothing listens on a TCP port — not on
any interface, not on loopback. This is a product rule rather than an
implementation detail: the application deliberately exposes no remote-control
surface, and it is verified against a running build with `netstat`, which shows
only outbound connections to the server's HTTP and CLI ports plus an ephemeral
UDP client socket for discovery replies.

**The one endpoint is a named pipe.** `\\.\pipe\SqeezeAmp-instance-<username>`
exists so a second launch raises the running window instead of starting a
second copy, and so a remapped key can send a transport command without paying
Qt's start-up cost. Its properties:

- It is a pipe, not a socket. Nothing reachable from another machine.
- It is per-user. Anything already running as you can write to it — which is the
  same trust boundary as you pressing a media key.
- It accepts five verbs (`next`, `previous`, `playpause`, `stop`, `activate`)
  and ignores everything else. An unrecognised message stays unrecognised
  rather than falling through to a default.
- Nothing is readable from it. No queries, no state, no player id — it is
  write-only from the sender's point of view.

**Credentials.** If your server requires HTTP basic auth, the password goes to
the Windows Credential Manager under `SqeezeAmp/<host>:<port>`. It is never
written to `QSettings`, never logged, and never placed in the registry
alongside the other settings.

**The audio engine is a separate process.** SqeezeAmp supervises a stock,
unmodified `squeezelite.exe` and talks to it only through documented
command-line arguments and its log output. It is not patched, and nothing it
prints is executed.

**The lyrics sidecar reads files you point it at.** If you set a music folder,
the app opens `.lrc` files under that root, located by matching the tail of the
path the server reports. Only sidecar files named after a track the server
already described are opened; nothing enumerates the folder and nothing writes
to it. Left empty — the default — nothing is read from disk at all.

## What is out of scope

- **The server connection is not encrypted**, because the Lyrion CLI and
  JSON-RPC interfaces are plain HTTP and TCP. Treat the link to your server as
  you would any other LAN service. This is a property of the protocol, not a
  defect in this client.
- **Anything running as your Windows user is already inside the boundary.** It
  can write to the pipe, read the credential store through the same API the app
  uses, and read the settings. A local attacker with your account is not a
  threat this application can defend against.
- **The bundled `squeezelite.exe` is upstream's.** Vulnerabilities in it belong
  to [that project](https://github.com/ralph-irving/squeezelite). If one affects
  the version pinned in `packaging/engine-version.txt`, please do tell me, so
  the pin can move.
