# Deployment notes

The unit examples use the current gateway path. Adjust paths when moving the checkout. The web service deliberately requires bubblewrap user namespaces; do not add a systemd restriction that prevents it from creating those namespaces without retesting isolation.

A local transient `maxcamera-web.service` is currently sufficient for review. To install a boot-persistent user unit after local deployment approval, copy the appropriate example into the gateway user's `~/.config/systemd/user/`, run `systemctl --user daemon-reload`, and enable it. A user service starts with that user's manager; starting before login may require an administrator to enable lingering. No lingering or boot installation is silently assumed.

The public tunnel is a separate service and requires explicit approval of Cloudflare as transport provider in this execution environment. Start the web service with `--secure-cookie` before exposing it. Inspect `journalctl --user -u maxcamera-tunnel` for its exact current Quick Tunnel URL. Quick Tunnel hostnames are temporary and can change on restart. Named tunnels need a separately configured account and stable hostname.

Only 127.0.0.1:8787 is forwarded. Never publish 10.0.0.3, GVCP/GVSP, SSH, host filesystem access or a local network proxy.

For a local-only installation, omit `--secure-cookie` so ordinary loopback HTTP can carry its session cookie. Do not use that mode for public plain HTTP. Stop and Release before restarting the service whenever possible.
