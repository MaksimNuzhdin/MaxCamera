# Security boundaries

## Threat addressed

A person who obtains the requested admin/admin login can operate the exposed camera controls. The application must not give that person a way to browse the local network, forward arbitrary traffic, change stream destinations, access host files or execute commands. Camera availability and scene privacy are not protected against someone who knows the camera login; they can view the scene and use permitted camera controls.

## Enforcement layers

1. HTTP server binds only 127.0.0.1. No router or LAN port is opened by the launcher.
2. Only a separately approved outbound HTTPS tunnel is intended for public access. The tunnel provider is trusted with transported application data; Quick Tunnel is temporary.
3. The launcher opens a connected UDP socket to the locally configured camera, a prebound stream-receive socket and the loopback HTTP listener. No destination is accepted from a browser.
4. Bubblewrap creates private user, mount, PID, IPC and network namespaces. Code and runtime dependencies are read-only; the host home, SSH keys, browser data, router credentials, Docker socket and system bus are not mounted.
5. The process drops capabilities. Libseccomp blocks new IPv4/IPv6/packet/netlink sockets, connect, bind, sendmsg/sendmmsg and sendto with an explicit destination. This matters because an inherited host-network socket would otherwise retain the host's routing context despite the new namespace. Connected send() remains available for the one camera endpoint. Namespace escape primitives and process injection syscalls are also denied.
6. The public API exposes named feature operations from the firmware catalogue, not raw register or memory access. Network/identity reconfiguration, firmware/reset, factory and persistent-save controls, and generic masked sensor writes are denied. Stream destination, host port and packet configuration are internally fixed.
7. Server-side sessions use random cookies, HttpOnly and SameSite=Strict; public deployment requires Secure. Mutations require matching Origin and CSRF token. No CORS access is enabled. Login and mutation rates and request sizes are bounded.
8. CSP blocks third-party script/image/connect origins and framing. Access logs are disabled. No cloud token, authentication secret or user-supplied camera address is stored in source.
9. Camera control has idle and maximum lease limits, heartbeat monitoring, finite capture waits and cleanup on normal shutdown.

## Important limits

These layers reduce the LAN-pivot risk associated with stolen web credentials. They are not a mathematical guarantee against kernel, browser, tunnel-provider, camera-firmware or supply-chain vulnerabilities. A compromised camera itself is a separate networked computer; for defense against that broader threat, place the camera in a dedicated VLAN with switch/router ACLs or on a separate physical interface. This session did not alter the switch/router or verify a camera VLAN, and it must not be described as having done so.

The read-only application mount includes the camera's public source tree and project-local runtime. It is not a secret store. Do not place credentials, private exports or unrelated files in the mounted application directory. The Python user-site dependency directory is read-only but contains installed code; use a minimal dedicated runtime when deploying on another gateway.

Remote writes are constrained by firmware bounds and access policy, but authenticated operators can still choose poor imaging settings, consume the 15-minute lease, modify allowed fan control or load an existing runtime profile. A stolen login does not imply protection against camera disruption or scene disclosure.

## Failure behavior

The launcher fails rather than serving without bubblewrap. The server refuses direct unisolated startup and exits if seccomp loading fails. Unsupported preview formats are rejected before acquisition. A busy camera is not taken over. Camera settings writes occur only while stopped. No automatic persistent store write or firmware load occurs.

On normal release, stop and transport restoration are attempted before CCP ownership is relinquished. Network loss can prevent acknowledgement; the camera heartbeat timeout remains the last recovery mechanism. Never claim that network failure permits guaranteed immediate cleanup at the remote device.

## Local-only maintenance

Change the camera endpoint, login password, deployment units or tunnel configuration only from the gateway host. A stable named tunnel requires separate account setup. Keep GVCP/GVSP off the public Internet. Review source-catalogue changes before updating the camera firmware or exposing additional controls.
