# Verification — November 2, 2026

## Functional camera check

A short, explicitly invoked live test ran only after a fresh read-only CCP check reported the camera free. Connect also rechecked CCP and did not attempt a takeover. Earlier retries that saw a nonzero CCP were deferred.

The verified camera was the project's OV5640 unit at the locally configured endpoint, FPGA processing version 13, original native output 2592×1944 YUV422 YUYV, sensor auto exposure/gain active. The final bounded check:

- Authenticated with admin/admin and valid Origin/CSRF.
- Connected and read 219 currently available values from the 242-feature catalogue.
- Started acquisition after waiting for its asynchronous write acknowledgement.
- Produced 13 complete converted JPEG previews in approximately 5.4 seconds; zero incomplete previews observed by this receiver in that short interval.
- Saved the final 1440×1080 JPEG privately under ignored `.runtime/`; image extrema included 0–255 in each channel. The initial post-start frame was black, later settled frames contained the scene.
- Stopped acquisition, wrote FanDutyCycle back to the same original value (75), reread settings, released camera ownership and logged out.

This establishes basic stream/control/lifecycle functionality. It does not qualify every parameter, all formats, sustained throughput, browser compatibility or sensor image quality. Preview throughput is affected by conversion and intentional frame skipping.

## Offline checks

16 tests pass, covering:

- Authentication required for the catalogue; invalid credentials; login throttling.
- Origin and CSRF rejection; security headers; request/method limits.
- Restricted network, factory, persistent and generic register controls.
- Invalid values, ownership separation and stopped-only write policy.
- RGB/mono/YUYV JPEG conversion and unsupported-format rejection.
- Reordered GVSP payload assembly, early trailer handling, exact payload completion and partial-frame rejection using loopback-only synthetic packets.
- Actual libseccomp enforcement: new network sockets, reconnecting the inherited UDP socket and sendto with an arbitrary destination all return PermissionError.

C compiles with `-O2 -shared -fPIC -Wall -Wextra`. Python files compile. Browser JavaScript passes Node syntax checking. The application starts successfully inside bubblewrap with private namespaces and seccomp enabled.

## Browser review

The in-app browser verified sign-in, disconnected dashboard, 242-feature rendering and the narrow-screen layout. Responsive CSS includes desktop, tablet and phone breakpoints. Physical Safari, Firefox, Android and iOS devices have not all been tested.

## Public deployment status

The local isolated service is running at 127.0.0.1:8787. A Cloudflare outbound HTTPS tunnel is prepared, but automatic approval review rejected starting it because the user had not explicitly named Cloudflare as the external destination for camera data. No public address is claimed until that provider approval and public reachability check succeed. No router forwarding, switch VLAN change or permanent public IP allocation has occurred.

## Evidence handling

Live JPEG and JSON test evidence remain in ignored `.runtime/`; camera scene images are not included in a Git commit or public source push. No camera firmware, Flash, startup UserSet or persistent network configuration was changed by these tests.
