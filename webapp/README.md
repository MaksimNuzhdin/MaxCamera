# MaxCamera Vision Console

An English, responsive camera workspace with session authentication, native GVCP/GVSP acquisition, a 242-feature firmware-derived explorer, camera sharing, and Linux network isolation.

See [the complete user guide](USER_GUIDE.md), [security boundaries](SECURITY.md), and [verification results](VERIFICATION.md).

## Run locally

Requires Linux, Python 3.10, a C compiler, bubblewrap, libseccomp, Pillow and NumPy. The launch fails closed if isolation cannot be established.

```bash
cd webapp
python3 -m pip install --target .runtime/python -r requirements.txt
mkdir -p .runtime
cc -O2 -shared -fPIC -Wall -Wextra receiver.c -o .runtime/receiver.so
python3 launch.py --camera 10.0.0.3 --port 8787
```

Open `http://127.0.0.1:8787` on the gateway computer. Sign in with `admin` / `admin`. Opening the page does not take camera ownership. Connect only when needed; Release after use.

The current portable launcher mounts the gateway's Python 3.10 user site packages read-only for Pillow/NumPy. Install those dependencies locally or adjust that one deployment path for another Python version. This is an application gateway; it does not build or reflash the camera firmware.

## Tests

```bash
PYTHONPATH=.runtime/python python3 -m pytest tests/test_offline.py -q
```

Offline tests use loopback sockets and never access hardware. `tests/live_check.py` is a separately invoked, bounded real-camera check; do not include it in routine CI.

## Public access

Use an approved outbound HTTPS reverse tunnel targeting only `http://127.0.0.1:8787`. Start the server with `--secure-cookie` when using that tunnel. Never expose GVCP/GVSP, the camera's own network interface, or an unauthenticated HTTP service through router forwarding.

The current deployment uses Cloudflare Quick Tunnel with the operator's explicit approval; public HTTPS login and protected endpoints have been verified. A Quick Tunnel URL is temporary and may change on restart. A stable hostname needs a separately configured named tunnel/account; this project does not imply a reserved public IP.

No cloud credential is embedded in the project. `MAXCAM_PASSWORD` can override the requested default password at server startup.
