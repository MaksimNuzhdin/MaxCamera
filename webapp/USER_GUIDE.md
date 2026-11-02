# MaxCamera Vision Console — complete user guide

Version 1.0 · English interface · Linux gateway · November 2, 2026

## 1. Purpose and operating model

MaxCamera Vision Console gives a browser access to the MaxCamera OV5640 GigE Vision camera. The gateway runs on the computer attached to the camera's Ethernet network. A browser communicates with the authenticated web server; only that server communicates with the fixed camera endpoint. The browser never receives a direct GVCP or GVSP connection.

The workspace contains a live preview, start/stop controls, snapshots, a searchable feature explorer, favorites, runtime setting exports and device diagnostics. Its catalogue is derived from the project's firmware metadata and GenICam XML. It includes 242 named controls and measurements. Features unsupported by the installed FPGA image or unavailable in the present camera mode are disabled or shown without a value.

The camera is shared with eBUS Player and other tools. Merely opening the website or signing in does not acquire the camera. Connect explicitly requests control if the camera is free. The application refuses to take over an existing controller. Stop ends acquisition while keeping the control lease. Release ends acquisition, restores the stream transport configuration saved at connection time, and relinquishes ownership.

Camera runtime image settings changed intentionally by an operator remain changed after Release. Release restores transport settings, not every image parameter. No automatic Flash, firmware, factory-preset or boot-policy write occurs. These persistent maintenance controls are excluded from remote writes.

## 2. Access and sign-in

### Local access

On the gateway computer open `http://127.0.0.1:8787`.

Enter:

- Username: `admin`
- Password: `admin`

The username is case sensitive. The login session lasts one hour; restarting the server clears existing sessions. The password is verified against a startup-derived scrypt hash. Passwords and session tokens are not written to access logs.

The Show button reveals the password field while you are entering it. It does not change the password. Click the AD circle in the upper-right corner to sign out. Sign-out releases a camera controlled by that session.

### Public access

The prepared deployment uses an outbound HTTPS tunnel aimed only at the loopback web server. It does not forward Ethernet or UDP camera ports. The operator explicitly approved Cloudflare as the external transport provider for the current deployment.

Use the exact HTTPS URL returned by the tunnel. HTTPS encrypts the browser connection, including the requested login credentials and preview. The tunnel provider terminates the public TLS connection, so it is part of the trust boundary. Use `--secure-cookie` when launching the server for this deployment; session cookies then require HTTPS.

A Cloudflare Quick Tunnel provides a temporary public hostname, not a reserved IP address. Its URL can change when the tunnel restarts. It has no uptime guarantee, and the gateway computer must stay powered on, awake and online. A fixed public hostname requires a named tunnel and an account/domain configuration. No fixed IP or permanent hostname is implied by this implementation.

The requested `admin/admin` credentials grant camera access to anyone who knows them. Network containment restricts that access to the camera application's supported operations. A local operator can supply `MAXCAM_PASSWORD` at startup to replace the default; there is no remote password-change endpoint.

## 3. Your first acquisition

1. Ensure eBUS Player and other camera tools are not controlling the camera. Leave their firmware, Flash and boot configuration alone.
2. Sign in and press **Connect camera**.
3. Wait for the state to become Connected and for the feature values to populate. The console reads available parameters; it does not automatically load a different image preset.
4. Check Width, Height and Pixel Format in Quick controls. The tested native configuration is 2592×1944 YUV422 YUYV on FPGA v13. Do not assume the full-resolution preview reaches the sensor's nominal frame rate.
5. Press **Start stream**. The server configures a private UDP receiver, MTU-sized packets and temporary packet pacing, then starts acquisition.
6. Wait for sensor exposure and gain to settle. An initial black frame was observed during verification; later frames contained the scene. The viewer displays only complete frames, not partially received images.
7. Press **Stop** before changing image settings.
8. Press **Release** when finished. The camera becomes available to other applications.

If the camera is busy, the console displays a message and leaves ownership unchanged. Continue offline work or wait for the other application to finish. Retry Connect later. Repeatedly forcing a connection is unnecessary; no ForceIP, reset or controller-stealing operation is exposed.

## 4. Understanding camera sharing

The server maintains one camera owner at a time, associated with one authenticated session. Another signed-in session cannot modify or release that owner's camera. Multiple tabs sharing the same browser session share that ownership identity; avoid competing controls from several tabs.

The maximum lease is 15 minutes. This prevents a long-lived page from keeping the shared camera indefinitely. A visible page refreshes its status every two seconds. If the server does not receive activity for 30 seconds, it automatically stops and releases the camera. A hidden tab does not keep polling. Returning to a tab may therefore require Connect again.

Closing or navigating away from the page sends a best-effort Release request. Browser termination or a lost network cannot guarantee delivery, so the server timeout remains the fallback. A streaming receive or conversion fault also triggers release. A gateway process killed without cleanup relies on the camera's own controller heartbeat timeout; do not immediately force ownership from another tool.

**Stop** keeps your lease so you can adjust settings and restart. **Release** gives up the lease. **Sign out** releases your lease and clears the browser's login cookie. The application does not continuously rediscover or reconnect to the camera in the background while disconnected.

## 5. Workspace layout

### Header and navigation

The header identifies MaxCamera and offers Guide and sign-out. The desktop sidebar provides Live acquisition, Feature explorer and Diagnostics shortcuts. Smaller layouts hide the sidebar and retain the same core tools in the main workspace.

Live acquisition selects Quick controls and brings the preview into view. Feature explorer selects the full catalogue. Diagnostics selects the diagnostic group. These shortcuts do not open an additional camera connection.

### Connection controls

The state chip reads Disconnected, Connected or Busy. Connect obtains a free camera. Release relinquishes it. Buttons become disabled while a request is in progress, or when the action is not appropriate for the current state.

A notice above the viewer displays camera or network errors. Read its message before retrying. Firmware rejection is distinct from a web login failure: camera validation may reject a value because a mode, dependency or capability requirement is not met.

### Live view

The viewer fits the camera image inside its available space while preserving aspect ratio. Empty areas can appear when the image and viewport have different proportions.

- **Composition grid:** overlays a rule-of-thirds grid. It changes only the display.
- **1:1 / Fit:** toggles between actual preview pixel dimensions and fitting the viewport. The JPEG preview may already be smaller than the camera's native output.
- **Fullscreen:** asks the browser to expand the preview. Browser support and platform policies apply. Escape exits fullscreen.
- **Start stream:** starts the fixed camera's acquisition.
- **Stop:** stops acquisition and clears the displayed preview.
- **Snapshot:** downloads the most recent displayed JPEG. It is a preview snapshot, not an original sensor payload or a lossless measurement file.

The composition grid, zoom behavior and fullscreen never modify sensor geometry or image-processing registers.

### Status cards

**Preview rate** is the number of complete, converted previews produced by the web gateway divided by elapsed acquisition time. It includes startup and conversion overhead. It is not the nominal sensor rate, a network packet rate, or a guaranteed delivery rate to every browser.

**Output size** reports the configured camera Width×Height. The browser preview is downscaled to fit within 1440×1080, preserving aspect ratio. The configured output and downloaded preview can therefore have different dimensions.

**Complete previews** counts frames that passed the receiver's complete-payload checks and were converted for the browser. The incomplete count covers frames observed by this bounded preview receiver. Intentional skipping of frames during conversion and packets discarded between preview calls are not a complete sensor or network loss metric. Inspect camera DroppedFrames, CsiErrorEvents and related diagnostics when investigating transport quality.

**Control lease** counts down the maximum 15-minute ownership interval. It does not show the separate one-hour login timeout or the 30-second inactivity timeout.

## 6. Finding and organizing features

### Quick controls

Quick controls collect common acquisition and image settings: frame rate, automatic/manual exposure, automatic/manual gain, white balance mode, FPGA processing enable, FPGA black level, Width, Height and Pixel Format.

### All features

Choose All features for the complete catalogue. The group dropdown filters the current view. A search matches feature names, readable labels, descriptions and group names. A nonempty search spans the whole catalogue, even when Quick controls or Starred is selected. Clear the search to return to the selected tab and group.

Press `/` while focus is outside an input to jump to search. Use Tab to move through controls, Enter to apply a numeric value and Escape to dismiss the guide dialog.

### Starred

Use the star beside a feature to add or remove it from Starred. Favorites are stored in that browser's local storage. They persist through ordinary page reloads and sign-out, but do not synchronize to other computers or browsers. Clearing site data removes them. Favorites contain feature names only, not passwords or camera ownership tokens.

### Reading current values

Connect reads available values. **Read** refreshes them while stopped. Each successful write also refreshes stopped-state values, because changing a mode or selector may affect other settings. This produces a short burst of control reads; it does not create an additional camera owner.

Values shown while streaming are the last stopped-state settings. They are not continuously sampled hardware telemetry. The frame count and preview rate update independently. Stop and Read for a new diagnostic snapshot.

An em dash means no current value was obtained. A disabled control can mean disconnected, streaming, an operation is running, a hardware dependency is unavailable, or the control is restricted. Read-only and Local-only badges explain access restrictions.

## 7. Editing parameters safely

1. Stop acquisition.
2. Find the parameter.
3. Check its description by hovering its name or using the full catalogue description.
4. Select a dropdown entry, enter a number, move a slider, or change a checkbox.
5. Press that feature's **Apply** button. Moving a slider changes the pending input only; it does not issue camera writes continuously.
6. Wait for the updated readback or the error notice.
7. Restart acquisition when the configuration is ready.

Changes are sent one feature at a time. There is no multi-feature transaction or automatic rollback of earlier successful edits. When configuring related geometry, choose an order that remains valid after each write. Firmware performs the final validation at write or acquisition start.

Numeric bounds come from firmware metadata. Float controls convert engineering units to the camera's underlying fixed-point registers. Exposure uses the active sensor line period rather than a hard-coded time increment. Temperature thresholds convert between degrees Celsius and the hardware ADC representation. Quantization can cause the readback to differ slightly from the requested decimal value.

The server rejects non-finite values, unknown enum numbers, fractional integer inputs, unknown feature names, unsupported booleans, out-of-range values and remote writes to restricted controls. Width uses the current firmware increment and maximum; Height uses an even increment and the current maximum. A value accepted by the input field may still be rejected by firmware because of coupled mode constraints.

All web writes require stopped acquisition, including the live-capable FPGA black-level feature. This conservative web policy avoids interrupting shared acquisition unexpectedly. Advanced frame-latched live-black tests in the camera firmware documentation do not imply live editing support in this UI version.

## 8. Feature families and workflows

### Image format and geometry

Width and Height select output dimensions. Pixel Format selects the camera's transport representation. Some modes require specific FPGA capabilities or sensor paths. A smaller output can reduce Ethernet, conversion and browser load, but changing geometry can also alter the sensor's field of view and cadence depending on the active path.

The browser preview currently supports uncompressed RGB8, BGR8, Mono8, BayerBG8, BayerBG10 and supported 422 YUV byte-order variants. Bayer previews show intensity; they are not demosaiced color images. Other catalogue formats remain selectable, but Start rejects them with an explanation when a preview converter is unavailable. Compressed JPEG acquisition is not supported by the fixed-size preview receiver in this version.

Do not confuse sensor resolution, FPGA input size, FPGA ROI, output size and preview size. They can all differ. Use the camera project's ISP and scaler documentation when configuring complex resampling.

### Exposure and gain

For automatic exposure choose ExposureAuto=Continuous. For manual exposure set ExposureAuto=Off first, then edit ExposureTime in microseconds. The gateway converts time using the current sensor line period. Exposure values are quantized to line units.

For manual gain set GainAuto=Off, then edit GainFactor. Gain is a multiplicative factor; higher gain can increase noise. AutoGainLimit and exposure limit controls bound the sensor's automatic behavior. ExposureMaximumLines and SensorLinePeriod help explain the active range. SensorBanding chooses Off, Auto, 50 Hz or 60 Hz behavior where supported.

Changing frame rate can change the available exposure interval. Do not assume a long exposure and a high frame rate are simultaneously possible. The camera's ResultingFrameRate is nominal sensor timing; the delivered preview may be slower.

### White balance and orientation

BalanceWhiteAuto controls the sensor's automatic white balance. Set it Off before changing WhiteBalanceRed, Green or Blue. RAW and FPGA-derived processing paths have different white-balance and orientation constraints. A sensor RAW path may have no valid sensor AWB statistics, and ReverseX/ReverseY can be locked to preserve Bayer phase.

FPGA manual gains and FPGA AWB are distinct from sensor white balance. Use the available capability and active-state controls to determine which pipeline applies. Do not enable two automatic systems without understanding their interaction.

### FPGA image processing

FpgaProcessingAvailable, FpgaProcessingVersion and FpgaProcessingCapabilities describe the installed hardware image. Availability flags identify individual blocks. A parameter in the catalogue is not proof that a block exists or is active.

The family includes RAW black-level correction, Bayer bad-pixel correction, channel gains, demosaic/color adjustments, color matrix coefficients, gamma, median/bilateral processing, sharpening, lens-shading grids, ROI/scaling, scene statistics, local contrast, adaptive display tone and distortion-map controls where implemented.

Most settings are staged while stopped and take effect at the next acquisition. If an image does not change, inspect enable flags, format, input path, block availability and active-state diagnostics. Sharpening can amplify noise and halos. Gamma or adaptive display curves do not recover clipped sensor information. Upscaling interpolates existing pixels; it does not create optical resolution.

ROI must fit the selected FPGA input dimensions. Area scaling reduces dimensions. Polyphase supports bounded enlargement, subject to the firmware's geometry and throughput limits. The qualified RGB4x example in the source project is bounded; arbitrary large 4x modes are not guaranteed by this application.

Distortion-map staging is an advanced, volatile operation. Index/data controls and Begin/Commit/Abort commands expose the existing firmware mechanism, but this UI does not provide a calibration or map-file upload workflow. Follow the camera project's map encoding, CRC, geometry, lease and drain rules. An identity or synthetic translation map is not optical lens calibration.

### Sensor ISP, effects and calibration tables

Sensor ISP control bits configure supported masked sensor processing functions. Effects, gamma tables, lens-correction tables and overlay parameters can be staged. Their named Apply commands commit the staged settings after acquisition is stopped.

A selector chooses the element being edited. Changing a selector can change the interpretation of its associated value field. Apply one selector, inspect the new readback, then edit the value. Do not assume a table-value control edits the whole table at once.

Read the per-feature description before executing an Apply command. The command button asks for confirmation because it changes camera runtime. This is an in-app camera operation, not a Flash or firmware update.

### Test controls

TestPattern and related sensor pattern controls help distinguish optical, sensor and transport problems. They replace or modify the scene on the camera. Record the original values, stop acquisition before changes, and restore them after testing. The application does not automatically replace a dark scene with synthetic imagery.

### Diagnostics and board controls

Diagnostics show sensor state/errors, capture/transmit/drop counters, CSI errors, Ethernet errors, uptime, clocks, capabilities and recovery status. These are snapshots refreshed while stopped. Some telemetry is intentionally cached or deferred by firmware under bandwidth pressure.

DeviceTemperature is the FPGA die temperature, not the OV5640 sensor temperature. FanStartTemperature and FanStopTemperature implement thermal hysteresis; the start threshold must be above the stop threshold. FanDutyCycle is commanded PWM duty, not measured fan RPM. StatusLEDs reports logical bits; it does not prove the physical LEDs were visually observed.

### Actuator, identity and OTP

VCM controls are available only on a module with a wired, supported actuator. The tested unit reports VCM unavailable; these controls remain disabled. Do not enable actuator functions based only on catalogue presence.

OTP values are read-only shadow data and can be unavailable until the existing refresh command succeeds. The console does not program OTP. An unavailable OTP shadow must not prevent connecting to the rest of the camera.

### UserSets and persistent maintenance

UserSetSelector chooses the profile. UserSetLoad loads the selected existing profile into runtime while stopped. It can change many settings; review the profile before loading, then inspect refreshed values.

UserSetSave, factory saving, UserSetDefault/startup selection, device reset, raw masked sensor writes and network configuration are restricted to local maintenance. The web application has no factory-password entry, Flash upload, firmware loader, arbitrary register endpoint, ForceIP, network scan or destination-address field.

## 9. Snapshots and settings export

Snapshot downloads the latest browser JPEG at the current preview size. It may have been resized and color converted. It is not appropriate as a lossless RAW dataset, a calibrated color reference or a frame-tagged exposure measurement.

Export downloads `maxcamera-settings.json` with schema version 1 and the currently cached writable runtime values. It excludes commands and controls restricted to local maintenance. It is a record for review and manual reapplication. This version does not implement settings-file import or an atomic restore operation.

Keep the export together with firmware/FPGA identity, lighting, optical setup and test notes when comparing experiments. Export before changing a complex configuration. Loading a UserSet and changing a selector can make an old export's values context dependent.

## 10. Troubleshooting

### Sign-in fails

Check case, username and password. The default is admin/admin unless the local operator supplied MAXCAM_PASSWORD. Ten failed or successful login attempts per peer within five minutes, and a global cap, limit repeated attempts. Wait for the window to expire. Restarting the service clears in-memory sessions and rate-limit state.

### Page signs out after restart

Sessions are intentionally memory-only. Sign in again. An old camera lease is not restored from disk.

### Connected in one tab, blocked in another browser

The first session owns the camera. Release in that session or wait for its idle/maximum lease timeout. A separate browser does not share its session cookie.

### Camera is busy

Stop and disconnect the camera in the application that owns it when that work is finished. Do not kill another user's process or reset the camera. This console will not steal control.

### Camera does not respond

Check camera power, switch link, cable, configured camera IP and host routing. Discovery, DHCP reconfiguration and camera readdressing are intentionally not available over the public API. The gateway's local operator must resolve these conditions.

### Connected, but Start fails

Check the error notice. Unsupported browser formats or compression are rejected before acquisition. Invalid ROI/scaler geometry, absent FPGA blocks, pending sensor state or firmware mode constraints can also prevent start. Choose a known supported runtime profile while stopped, or coordinate with the local camera operator.

### Preview is black

Allow initial auto-exposure/gain settling. Verify lighting, lens/cap and TestPattern. Inspect exposure/gain mode and active pipeline. Do not assume black pixels mean a transport failure: complete black frames can be valid camera output. The console never substitutes a sample photograph for a live frame.

### Preview freezes or disappears

Stale frames are not presented as live indefinitely. A frame older than three seconds causes the frame endpoint to report staleness. Eight seconds without a complete frame stops the receiver and releases the camera. Reconnect only after checking the reported error and ensuring the camera is free.

### Preview is slower than expected

Check output dimensions, host CPU load, Ethernet capacity and browser/network latency. Conversion of full-resolution YUV is expensive. This receiver prioritizes complete previews and bounded memory, skips frames while processing, and does not request retransmission. Its incomplete metric is not a full transport qualification. A smaller known-good camera output may improve responsiveness, with the associated geometry/cadence tradeoffs.

### Manual exposure, gain or white balance is rejected

Set the corresponding automatic mode Off first. A RAW path or another mode can impose additional constraints. Stop acquisition before all edits.

### Value changes slightly after Apply

Hardware uses integers and fixed-point representations. Rounding and the current sensor line period determine the readback. The displayed value is the accepted readback rather than an unverified echo of your input.

### Slider moved, image did not change

Press Apply. If acquisition was running, stop first. Some staged parameters also need the family's Apply command; most FPGA settings need the next acquisition. Confirm that the block is available and enabled in the active image path.

### Setting is Local only

The restriction is intentional. Perform network, firmware, startup-preset and protected maintenance work using an appropriate local tool. The public API does not accept a bypass flag or arbitrary register address.

### Public URL stops working

Check the local server first, then the tunnel service and Internet connection. A Quick Tunnel URL can change after restart. Do not fix this by opening camera UDP ports on the router. A named tunnel is required for a managed stable hostname.

## 11. Keyboard, mobile and browser behavior

Use current browsers with JavaScript, Fetch, CSS Grid/Flexbox and secure cookies. The layout adapts to narrow screens: the feature panel moves beneath the viewer, the sidebar is hidden, and connection/acquisition controls remain available. Landscape mode gives more room for the image.

Keyboard focus is visible. Labels are associated with controls. Error/status notices use live roles. Reduced-motion preferences disable transitions. Fullscreen and download behavior can vary by browser and mobile platform. iOS, Firefox, Safari and Android devices have not all been physically verified; responsive CSS is not a claim of exhaustive device certification.

## 12. Local installation and operation

The gateway is deliberately Linux-specific because it uses bubblewrap namespaces and libseccomp. It needs Python 3.10, a C compiler, Pillow, NumPy, FastAPI, Uvicorn, bubblewrap and libseccomp. These are gateway dependencies, not camera firmware build dependencies.

From the repository's webapp directory:

```bash
mkdir -p .runtime
python3 -m pip install --target .runtime/python -r requirements.txt
cc -O2 -shared -fPIC -Wall -Wextra receiver.c -o .runtime/receiver.so
python3 launch.py --camera 10.0.0.3 --port 8787
```

The project-local pip directory avoids changes to the system Python. The existing portable launcher also mounts the gateway's Python 3.10 user site directory read-only; adapt that deployment path when moving to another Python layout. No eBUS license or Aravis runtime is required by the implemented gateway.

The listener binds only 127.0.0.1. The launcher opens fixed sockets before entering isolation, mounts code and dependencies read-only, hides the host home directory and enters a private network namespace. The server then loads syscall restrictions before accepting operations.

For an approved public HTTPS tunnel, start with:

```bash
python3 launch.py --camera 10.0.0.3 --port 8787 --secure-cookie
```

Configure the outbound tunnel to forward HTTP only to `http://127.0.0.1:8787`. Do not forward the camera address, GVCP port, GVSP stream, shell, SDK tools or host filesystem. Cloudflared must come from Cloudflare's official release source. The tunnel provider is a trusted external transport; the current operator has approved Cloudflare.

The running local deployment uses the user service `maxcamera-web.service`. Useful local commands:

```bash
systemctl --user status maxcamera-web
journalctl --user -u maxcamera-web -n 100 --no-pager
systemctl --user stop maxcamera-web
systemctl --user restart maxcamera-web
```

A transient systemd-run service is not an installed boot-time unit. It survives the chat process but is not promised to start after reboot. The repository includes example service units and installation notes in deploy/. Installing a persistent user unit and enabling lingering must follow the gateway's local administration policy. No root password or router credentials are included.

Stop and release through the UI before planned service maintenance. A normal SIGTERM to Uvicorn performs lifespan cleanup. A sudden power loss or SIGKILL relies on the camera heartbeat timeout.

## 13. API and integration boundaries

All API routes except login and health require an authenticated cookie. Mutations require both a matching Origin and the session's CSRF header. The server accepts JSON bodies with a 4096-byte limit. Login, session and camera state are kept in memory.

- GET /healthz: liveness and declared isolation mode, no camera connection.
- POST /api/login: authenticate and obtain the CSRF token; sets the session cookie.
- GET /api/session: recover the current session's CSRF token after reload.
- GET /api/catalog: bounded feature metadata, no raw address inputs.
- GET /api/status: lease, cached feature values, preview counters and feature availability errors.
- POST /api/connect: obtain a free camera lease.
- POST /api/start and /api/stop: acquisition lifecycle.
- POST /api/feature: apply one named allowlisted value while stopped.
- POST /api/refresh: refresh stopped-state values.
- GET /api/frame: latest complete JPEG; unavailable/stale frames return an error.
- GET /api/settings: cached writable settings export.
- POST /api/release: release the session's camera lease.
- POST /api/logout: release owned camera and invalidate session.

This is not a generic GenICam proxy, HTTP proxy, file manager or remote shell. There is no endpoint for arbitrary hosts, URLs, shell commands, register addresses, files or firmware images. A settings export is not a programmable script.

## 14. Verification and known limits

The bounded live check on November 2, 2026 connected to the free FPGA v13 camera, read 219 currently available values, started native 2592×1944 YUYV acquisition, produced 13 complete JPEG previews in about 5.4 seconds with zero observed incomplete previews, stopped, performed a same-value FanDutyCycle write, and released ownership. The last saved preview had nonzero color content. This is a short functional check, not endurance, packet-loss certification or all-feature validation.

The offline test suite covers authentication, Origin/CSRF checks, input limits, login throttling, restricted feature writes, invalid numeric values, ownership checks, streaming write locks, image conversion, frame assembly with reordered packets, partial-frame rejection and the actual Linux syscall restrictions against new network sockets and inherited-socket retargeting.

Known limitations include fixed-camera addressing, one camera lease, fixed-size uncompressed preview formats, intensity-only Bayer display, sequential single-feature edits, no settings import, no video recording/audio, no WebRTC, no lossless RAW download, no automatic optical calibration and no complete browser/device qualification. UserSet persistence, factory access, network configuration and generic sensor-register writes remain local-only.

The feature appendix below lists the source catalogue, including controls not currently available on the installed mode or hardware. Firmware and FPGA versions must remain compatible with the catalogue. Review metadata changes when upgrading firmware; this gateway does not download or interpret arbitrary camera-supplied executable code.

## Appendix A. Full feature reference

Access describes camera metadata. Web access can be more restrictive. Numeric raw bounds below are firmware register bounds; the UI converts Float, exposure and temperature values to engineering units. Availability depends on the installed camera image and active mode.

### Diagnostics

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| SensorInitialized | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| SensorBusy | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| SensorState | RO / read-only or local | Bound, Ready, Configured, Streaming, Fault | See current readback, capability and firmware constraints. |
| SensorLastError | RO / read-only or local | Integer; raw -1000 to 0 | See current readback, capability and firmware constraints. |
| SensorTransportError | RO / read-only or local | Integer; raw -1000 to 0 | See current readback, capability and firmware constraints. |
| SensorErrorRegister | RO / read-only or local | Integer; raw 0 to 65535 | See current readback, capability and firmware constraints. |
| GigEVisionReady | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| EthernetLinkUp | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| DeviceUptime | RO / read-only or local | Integer; raw 0 to 4294967295; s | See current readback, capability and firmware constraints. |
| CapturedFrames | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| DroppedFrames | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| CsiErrorEvents | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| TransmittedFrames | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| StreamSendRetries | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| EthernetReceivedPackets | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| EthernetTransmittedPackets | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| EthernetErrors | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| EthernetReceiveDrops | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| DiagnosticUartBaudRate | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| SystemTimerFrequency | RO / read-only or local | Integer; raw 0 to 4294967295; Hz | See current readback, capability and firmware constraints. |
| SensorI2cFrequency | RO / read-only or local | Integer; raw 0 to 4294967295; Hz | See current readback, capability and firmware constraints. |
| SensorChipID | RO / read-only or local | Integer; raw 0 to 65535 | See current readback, capability and firmware constraints. |
| SensorAverageLuminance | RO / read-only or local | Integer; raw 0 to 255 | Sensor AEC weighted luminance readout; refreshed with live telemetry. |
| NativePixelFormatsAvailable | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| RawPixelFormatActive | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| PacketResendRequests | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| RetransmittedPackets | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| PacketResendUnavailable | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| PacketResendQueueOverflows | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| CameraRecoveryCount | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| SensorTelemetryDeferred | RO / read-only or local | Boolean; raw 0 to 1 | High-bandwidth streaming defers SCCB telemetry above 105 MB/s to preserve delivered FPS. Cached values refresh during supported active acquisition or synchronous sensor operations; stopped background reads are disabled. |
| SensorRawActive | RO / read-only or local | Boolean; raw 0 to 1 | The sensor uses its Bayer RAW tap, including FPGA RGB conversion. Sensor AWB and orientation are unavailable in this mode. |

### BoardControl

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| DeviceTemperature | RO / read-only or local | Float; raw -273150 to 230000; C | FPGA die temperature from board ADC, not the image sensor temperature. |
| FanStartTemperature | RW / editable | TemperatureADC; raw 0 to 1023 | Hardware hysteresis upper threshold. Must exceed FanStopTemperature. |
| FanStopTemperature | RW / editable | TemperatureADC; raw 0 to 1023 | Hardware hysteresis lower threshold. Must be below FanStartTemperature. |
| FanDutyCycle | RW / editable | Integer; raw 0 to 100; % | PWM duty when hardware thermal hysteresis enables the fan. No tachometer/RPM feedback is wired. |
| StatusLEDs | RO / read-only or local | Integer; raw 0 to 15 | Logical active-high status bits: 0 heartbeat, 1 sensor ready, 2 GigE ready, 3 streaming blink on images with independent video reset (version 10 and later). Physical LEDs are active-low. |

### SensorTiming

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| SensorInputClock | RO / read-only or local | Integer; raw 0 to 4294967295; Hz | Configured 12MHz module XCLK assumption, supported by measured frame timing. |
| SensorSystemClock | RO / read-only or local | Integer; raw 0 to 4294967295; Hz | See current readback, capability and firmware constraints. |
| SensorLineLength | RO / read-only or local | Integer; raw 0 to 65535 | See current readback, capability and firmware constraints. |
| SensorFrameLength | RO / read-only or local | Integer; raw 0 to 65535 | See current readback, capability and firmware constraints. |
| AcquisitionFrameRate | RW / editable | Float; raw 200 to 30000; Hz | Sensor frame rate limit through vertical blanking only; clocks remain unchanged. Actual transmitted rate can be lower because of conversion or network bandwidth. |
| SensorLinePeriod | RO / read-only or local | Float; raw 0 to 4294967295; us | See current readback, capability and firmware constraints. |
| ExposureMaximumLines | RO / read-only or local | Integer; raw 1 to 65531 | See current readback, capability and firmware constraints. |
| AcquisitionFrameRateEnable | RW / editable | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| AcquisitionFrameRateMinimum | RO / read-only or local | Float; raw 1 to 30000; Hz | See current readback, capability and firmware constraints. |
| AcquisitionFrameRateMaximum | RO / read-only or local | Float; raw 1 to 30000; Hz | See current readback, capability and firmware constraints. |
| ResultingFrameRate | RO / read-only or local | Float; raw 0 to 30000; Hz | Nominal sensor rate from current HTS/VTS, not network delivery FPS. |
| WidthIncrement | RO / read-only or local | Integer; raw 2 to 16 | See current readback, capability and firmware constraints. |
| WidthMaximum | RO / read-only or local | Integer; raw 160 to 2592 | See current readback, capability and firmware constraints. |
| HeightMaximum | RO / read-only or local | Integer; raw 120 to 1944 | See current readback, capability and firmware constraints. |

### SensorControl

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| ExposureAuto | RW / editable | Off, Continuous | See current readback, capability and firmware constraints. |
| GainAuto | RW / editable | Off, Continuous | See current readback, capability and firmware constraints. |
| ExposureTime | RW / editable | Float; raw 1 to 65531; us | Exposure in microseconds; Line period follows the active image mode. Stop acquisition and set ExposureAuto=Off to change. |
| GainFactor | RW / editable | Float; raw 16 to 1023; x | See current readback, capability and firmware constraints. |
| ExposureLimit50Hz | RW / editable | Integer; raw 1 to 65531; lines | See current readback, capability and firmware constraints. |
| ExposureLimit60Hz | RW / editable | Integer; raw 1 to 65531; lines | See current readback, capability and firmware constraints. |
| AutoGainLimit | RW / editable | Float; raw 16 to 1023; x | See current readback, capability and firmware constraints. |
| SensorNightMode | RW / editable | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| SensorBanding | RW / editable | Off, Auto, Hz50, Hz60 | See current readback, capability and firmware constraints. |
| ReverseX | RW / editable | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| ReverseY | RW / editable | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| AutoExposureTarget | RW / editable | Integer; raw 8 to 240 | Average-luminance target in sensor units; updates stable and fast AEC thresholds. |

### WhiteBalance

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| BalanceWhiteAuto | RW / editable | Off, Continuous | Continuous AWB is available in ISP formats. RAW has no valid AWB statistics; entering RAW seeds manual unity gains. Legacy RAW presets load with AWB Off and their saved gains. |
| WhiteBalanceRed | RW / editable | Float; raw 1 to 4095; x | See current readback, capability and firmware constraints. |
| WhiteBalanceGreen | RW / editable | Float; raw 1 to 4095; x | See current readback, capability and firmware constraints. |
| WhiteBalanceBlue | RW / editable | Float; raw 1 to 4095; x | See current readback, capability and firmware constraints. |

### TestControl

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| TestPattern | RW / editable | Off, ColorBars, Random, Square, Black | See current readback, capability and firmware constraints. |
| SensorTestPatternStyle | RW / editable | Integer; raw 0 to 3 | See current readback, capability and firmware constraints. |
| SensorTestRolling | RW / editable | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| SensorTestTransparent | RW / editable | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| SensorTestMonochrome | RW / editable | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |

### IspControl

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| SensorIspControl0 | RW / editable | Integer; raw 0 to 167 | Masked sensor ISP control. Reserved bits are preserved. |
| SensorIspControl1 | RW / editable | Integer; raw 0 to 167 | Masked sensor ISP control. Reserved bits are preserved. |
| SensorIspControl3 | RW / editable | Integer; raw 0 to 7 | Masked sensor ISP control. Reserved bits are preserved. |
| SensorSharpness | RW / editable | Integer; raw 0 to 255 | Manual CIP strength in sensor register units. Has no effect on RAW output. |
| SensorDenoise | RW / editable | Integer; raw 0 to 255 | Manual CIP strength in sensor register units. Has no effect on RAW output. |
| SensorSharpnessAuto | RW / editable | Off, Continuous | Selects automatic CIP threshold; manual strength takes effect when Off. |
| SensorDenoiseAuto | RW / editable | Off, Continuous | Selects automatic CIP threshold; manual strength takes effect when Off. |

### ImageEffects

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| EffectEnableBits | RW / editable | Integer; raw 0 to 255 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectHueCos | RW / editable | Integer; raw 0 to 255 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectHueSin | RW / editable | Integer; raw 0 to 255 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectU | RW / editable | Integer; raw 0 to 255 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectV | RW / editable | Integer; raw 0 to 255 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectYOffset | RW / editable | Integer; raw 0 to 255 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectContrast | RW / editable | Integer; raw 0 to 255 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectBrightness | RW / editable | Integer; raw 0 to 255 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectSigns | RW / editable | Integer; raw 0 to 63 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectManualSaturation | RW / editable | Integer; raw 0 to 1 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectUvThreshold1 | RW / editable | Integer; raw 0 to 255 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectUvThreshold2 | RW / editable | Integer; raw 0 to 511 | Staged setting. Stop acquisition then execute EffectApply to commit all effect parameters. |
| EffectApply | WO / editable | Command; raw 0 to 1 | See current readback, capability and firmware constraints. |

### ImageCalibration

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| GammaPointSelector | RW / editable | Integer; raw 0 to 15 | Staged table setting; execute the corresponding Apply command while stopped. |
| GammaPointValue | RW / editable | Integer; raw 0 to 255 | Staged table setting; execute the corresponding Apply command while stopped. |
| GammaControl | RW / editable | Integer; raw 0 to 3 | Staged table setting; execute the corresponding Apply command while stopped. |
| LensCorrectionSelector | RW / editable | Integer; raw 0 to 61 | Staged table setting; execute the corresponding Apply command while stopped. |
| LensCorrectionValue | RW / editable | Integer; raw 0 to 255 | Staged table setting; execute the corresponding Apply command while stopped. |
| LensCorrectionEnable | RW / editable | Integer; raw 0 to 1 | Staged table setting; execute the corresponding Apply command while stopped. |
| GammaApply | WO / editable | Command; raw 0 to 1 | See current readback, capability and firmware constraints. |
| LensCorrectionApply | WO / editable | Command; raw 0 to 1 | See current readback, capability and firmware constraints. |

### SensorOverlay

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| OverlayLeft | RW / editable | Integer; raw 0 to 4095 | See current readback, capability and firmware constraints. |
| OverlayRight | RW / editable | Integer; raw 0 to 4095 | See current readback, capability and firmware constraints. |
| OverlayTop | RW / editable | Integer; raw 0 to 2047 | See current readback, capability and firmware constraints. |
| OverlayBottom | RW / editable | Integer; raw 0 to 2047 | See current readback, capability and firmware constraints. |
| OverlayU | RW / editable | Integer; raw 0 to 255 | See current readback, capability and firmware constraints. |
| OverlayV | RW / editable | Integer; raw 0 to 255 | See current readback, capability and firmware constraints. |
| OverlayEnable | RW / editable | Integer; raw 0 to 1 | See current readback, capability and firmware constraints. |
| OverlayApply | WO / editable | Command; raw 0 to 1 | See current readback, capability and firmware constraints. |

### SensorActuator

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| SensorVcmAvailable | RO / read-only or local | Boolean; raw 0 to 1 | Per-unit capability. Enable only for a module with a wired VCM actuator. |
| VcmTarget | RW / editable | Integer; raw 0 to 1023 | See current readback, capability and firmware constraints. |
| VcmSlew | RW / editable | Integer; raw 0 to 15 | See current readback, capability and firmware constraints. |
| VcmDivider | RW / editable | Integer; raw 0 to 4095 | See current readback, capability and firmware constraints. |
| VcmCurrent | RW / editable | Integer; raw 0 to 7 | See current readback, capability and firmware constraints. |
| VcmRih | RW / editable | Integer; raw 0 to 1 | See current readback, capability and firmware constraints. |
| VcmPowerDown | RW / editable | Integer; raw 0 to 1 | See current readback, capability and firmware constraints. |
| VcmApply | WO / editable | Command; raw 0 to 1 | See current readback, capability and firmware constraints. |

### SensorRegisters

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| SensorRegisterSelector | RW / read-only or local | RegisterSelector; raw 0 to 65535 | Named register catalogue from Ov5640Registers.h. Register values require stopped acquisition. |
| SensorRegisterValue | RO / read-only or local | Integer; raw 0 to 255 | See current readback, capability and firmware constraints. |
| SensorFeatureMask | RW / read-only or local | Integer; raw 0 to 255 | See current readback, capability and firmware constraints. |
| SensorFeatureValue | RW / read-only or local | Integer; raw 0 to 255 | See current readback, capability and firmware constraints. |
| SensorFeatureApply | WO / read-only or local | Command; raw 0 to 1 | Stopped-only masked write to supported ISP/analog image-control ranges. Clock/reset/format/OTP/MCU registers are write-protected. |

### SensorIdentity

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| SensorOtpSelector | RW / editable | Integer; raw 0 to 31 | See current readback, capability and firmware constraints. |
| SensorOtpValue | RO / read-only or local | Integer; raw 0 to 255 | See current readback, capability and firmware constraints. |
| SensorOtpRefresh | WO / editable | Command; raw 0 to 1 | Refresh the 32-byte OTP shadow. Read only; never programs OTP. |
| SensorOtpValid | RO / read-only or local | Boolean; raw 0 to 1 | The OTP read shadow becomes valid after SensorOtpRefresh succeeds. |

### SensorCapabilities

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| SensorJpegAvailable | RO / read-only or local | Boolean; raw 0 to 1 | JPEG transport is implemented in the matching CSI-byte production image; select VGA YUYV and ImageCompressionMode=JPEG. |
| SensorFrexAvailable | RO / read-only or local | Boolean; raw 0 to 1 | Unavailable until the corresponding board/transport integration is qualified. |
| SensorMcuFirmwareAvailable | RO / read-only or local | Boolean; raw 0 to 1 | Unavailable until the corresponding board/transport integration is qualified. |
| ImageCompressionMode | RW / editable | Off, JPEG | JPEG requires VGA YUYV. Select 640x480 YUV422_YUYVPacked first. GVSP payload type JPEG; PayloadSize is maximum allocation, each leader carries actual encoded length. |
| SensorJpegQuality | RW / editable | Integer; raw 0 to 63 | OV5640 quantization scale, smaller values give higher quality/larger encoded frames. |

### UserSetControl

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| UserSetStorageAvailable | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| UserSetBusy | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| UserSetLastError | RO / read-only or local | Integer; raw -1000 to 0 | See current readback, capability and firmware constraints. |
| UserSetSelector | RW / editable | Default, UserSet1, UserSet2, UserSet3, UserSet4, UserSet5, UserSet6, UserSet7 | See current readback, capability and firmware constraints. |
| UserSetLoad | RW / editable | Command; raw 0 to 1 | See current readback, capability and firmware constraints. |
| UserSetSave | RW / read-only or local | Command; raw 0 to 1 | Save UserSet1..7. Default is read-only through this standard command; factory provisioning uses the password-protected FactoryUserSetSave command. |
| UserSetDefault | RW / read-only or local | Default, UserSet1, UserSet2, UserSet3, UserSet4, UserSet5, UserSet6, UserSet7 | See current readback, capability and firmware constraints. |
| UserSetValid | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| UserSetSavedMask | RO / read-only or local | Integer; raw 0 to 255 | See current readback, capability and firmware constraints. |
| FactoryAccessUnlocked | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FeatureAccessLock | WO / read-only or local | Command; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FactoryUserSetSave | RW / read-only or local | Command; raw 0 to 1 | Factory maintenance only: save the current profile into the factory slot. Requires UserSetSelector=Default and an unlocked factory password. Not part of standard UserSetSave. |

### NetworkControl

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| DeviceConfigurationPending | RO / read-only or local | Boolean; raw 0 to 1 | Network/identity changes are journaled automatically while stopped. IP mode and identity apply on the next firmware boot. |
| DeviceConfigurationLastError | RO / read-only or local | Integer; raw -1000 to 0 | See current readback, capability and firmware constraints. |
| DeviceIdentityProvisioned | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |

### DeviceControl

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| DeviceReset | WO / read-only or local | Command; raw 0 to 1 | Asynchronously return to the power-up configuration: reset sensor/video and protocol, load UserSetDefault, and apply saved network/identity. Acquisition and ownership are released. Rediscover/reconnect after completion. |

### DeviceMaintenance

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| DeviceResetBusy | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| DeviceResetLastError | RO / read-only or local | Integer; raw -1000 to 0 | See current readback, capability and firmware constraints. |

### FpgaImageProcessing

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| FpgaProcessingVersion | RO / read-only or local | Integer; raw 0 to 4294967295 | See current readback, capability and firmware constraints. |
| FpgaProcessingCapabilities | RO / read-only or local | Integer; raw 0 to 4294967295 | Integrated PL capabilities: bit 0 black-level subtraction, bit 1 Bayer bad-pixel correction, bit 2 manual gains, bit 3 RGB demosaic, bit 4 RGB median, bit 5 ROI/area downscale, bit 6 CCM, bit 7 gamma, bit 8 FPGA AWB/statistics, bit 9 bilateral filtering, bit 10 sharpening, bit 11 RAW lens shading, bit 12 focus/motion analysis, bit 13 RGB-to-YUV output, bit 14 Xilinx polyphase resampling, bit 15 local contrast with shared RAW preprocessing, bit 16 adaptive RGB display tone mapping, bit 17 bounded integer-map remapping, bit 18 four-byte-per-clock framebuffer writer. Capability bits describe the installed FPGA image. |
| FpgaProcessingAvailable | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FpgaProcessingEnable | RW / editable | Boolean; raw 0 to 1 | Applied at the next AcquisitionStart while stopped. Processing supports BayerBG10/BayerBG10p and, when color capability is present, RGB8/BGR8. Other formats use byte bypass. |
| FpgaBadPixelCorrectionEnable | RW / editable | Boolean; raw 0 to 1 | Applied at the next AcquisitionStart while stopped. Processing supports BayerBG10/BayerBG10p and, when color capability is present, RGB8/BGR8. Other formats use byte bypass. |
| FpgaBlackLevel | RW / editable | Integer; raw 0 to 1023; DN | 10-bit RAW black offset before BPC and gains. On audited v9/v10 active RAW processing, a live write applies to a subsequent complete HLS frame without restarting acquisition. Readback is the requested setting, not a frame-tagged applied value. Otherwise configure while stopped; applies at next AcquisitionStart. |
| FpgaBalanceRed | RW / editable | Float; raw 0 to 4096; x | Q8.8 Bayer gain, unity 1.0. Applied after BPC, saturated to 10 bits. Applies at next AcquisitionStart. |
| FpgaBalanceGreen | RW / editable | Float; raw 0 to 4096; x | Q8.8 Bayer gain, unity 1.0. Applied after BPC, saturated to 10 bits. Applies at next AcquisitionStart. |
| FpgaBalanceBlue | RW / editable | Float; raw 0 to 4096; x | Q8.8 Bayer gain, unity 1.0. Applied after BPC, saturated to 10 bits. Applies at next AcquisitionStart. |
| FpgaProcessingActive | RO / read-only or local | Boolean; raw 0 to 1 | True during acquisition when a supported RAW10 or RGB processing path is selected. |
| FpgaMedianAvailable | RO / read-only or local | Boolean; raw 0 to 1 | Reports whether the installed FPGA image includes this RGB processing block. |
| FpgaScalerAvailable | RO / read-only or local | Boolean; raw 0 to 1 | Reports whether the installed FPGA image includes this RGB processing block. |
| FpgaMedianEnable | RW / editable | Boolean; raw 0 to 1 | Applied at the next AcquisitionStart. Requires FPGA processing and RGB8/BGR8 or supported FPGA YUV output. |
| FpgaScalerEnable | RW / editable | Boolean; raw 0 to 1 | Applied at the next AcquisitionStart. Requires FPGA processing and RGB8/BGR8 or supported FPGA YUV output. |
| FpgaRoiEnable | RW / editable | Boolean; raw 0 to 1 | Applied at the next AcquisitionStart. Requires FPGA processing and RGB8/BGR8 or supported FPGA YUV output. |
| FpgaInputWidth | RW / editable | Integer; raw 16 to 2592; px | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| FpgaInputHeight | RW / editable | Integer; raw 8 to 1944; px | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| FpgaRoiOffsetX | RW / editable | Integer; raw 0 to 2591; px | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| FpgaRoiOffsetY | RW / editable | Integer; raw 0 to 1943; px | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| FpgaRoiWidth | RW / editable | Integer; raw 1 to 2592; px | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| FpgaRoiHeight | RW / editable | Integer; raw 1 to 1944; px | Input and ROI dimensions for FPGA color resampling. Width/Height select output size. ROI must fit the RAW input. Area mode only reduces; Polyphase permits up to 4x enlargement per ROI axis within the 2592x1944 output bounds. Mixed up/down axes are supported. Validated at AcquisitionStart. |
| FpgaColorAdjustmentAvailable | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FpgaColorTransformationEnable | RW / editable | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FpgaGammaEnable | RW / editable | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FpgaBalanceWhiteAuto | RW / editable | Off, Continuous | FPGA gray-world AWB: previous-frame admitted samples, gains 0.25x to 4x, quarter-step smoothing, green reference unity. Dark/saturated frames hold gains. Restart resets AWB to unity. |
| FpgaColorTransformationSelector | RW / editable | Integer; raw 0 to 8 | Stopped-only table selector. Matrix order is RGB rows then RGB columns. Channel order is red, green, blue. |
| FpgaColorOffsetSelector | RW / editable | Integer; raw 0 to 2 | Stopped-only table selector. Matrix order is RGB rows then RGB columns. Channel order is red, green, blue. |
| FpgaGammaChannel | RW / editable | Integer; raw 0 to 2 | Stopped-only table selector. Matrix order is RGB rows then RGB columns. Channel order is red, green, blue. |
| FpgaGammaIndex | RW / editable | Integer; raw 0 to 255 | Stopped-only table selector. Matrix order is RGB rows then RGB columns. Channel order is red, green, blue. |
| FpgaColorTransformationValue | RW / editable | Float; raw -8388608 to 8388607 | Signed Q20 coefficient or DN offset in [-8,8). Applied before RGB8 conversion; vendor Q8 product intermediates and truncation apply. |
| FpgaColorOffsetValue | RW / editable | Float; raw -8388608 to 8388607 | Signed Q20 coefficient or DN offset in [-8,8). Applied before RGB8 conversion; vendor Q8 product intermediates and truncation apply. |
| FpgaGammaValue | RW / editable | Integer; raw 0 to 255 | Selected channel/index RGB8 LUT value. Default is identity. All 768 LUT bytes persist in UserSet. |
| FpgaStatisticsLatch | WO / editable | Command; raw 0 to 1 | Snapshot the latest complete FPGA frame statistics. Allowed during acquisition; read fields remain stable until the next latch. |
| FpgaStatisticsSequence | RO / read-only or local | Integer; raw 0 to 4294967295 | Latched complete-frame statistics. Sequence restarts at acquisition reset and is not the GVSP FrameID. |
| FpgaStatisticsValidPixels | RO / read-only or local | Integer; raw 0 to 5038848 | Latched complete-frame statistics. Sequence restarts at acquisition reset and is not the GVSP FrameID. |
| FpgaClippedPixels | RO / read-only or local | Integer; raw 0 to 5038848 | Latched complete-frame statistics. Sequence restarts at acquisition reset and is not the GVSP FrameID. |
| FpgaDarkPixels | RO / read-only or local | Integer; raw 0 to 5038848 | Latched complete-frame statistics. Sequence restarts at acquisition reset and is not the GVSP FrameID. |
| FpgaBalanceMeanRed | RO / read-only or local | Float; raw 0 to 67043328; DN | Latched Q16 mean. Color means precede gains/CCM and exclude samples with any channel below 8 or at least 1000 DN. Intensity is output RGB8 luma. |
| FpgaBalanceMeanGreen | RO / read-only or local | Float; raw 0 to 67043328; DN | Latched Q16 mean. Color means precede gains/CCM and exclude samples with any channel below 8 or at least 1000 DN. Intensity is output RGB8 luma. |
| FpgaBalanceMeanBlue | RO / read-only or local | Float; raw 0 to 67043328; DN | Latched Q16 mean. Color means precede gains/CCM and exclude samples with any channel below 8 or at least 1000 DN. Intensity is output RGB8 luma. |
| FpgaIntensityMean | RO / read-only or local | Float; raw 0 to 67043328; DN | Latched Q16 mean. Color means precede gains/CCM and exclude samples with any channel below 8 or at least 1000 DN. Intensity is output RGB8 luma. |
| FpgaAppliedBalanceRed | RO / read-only or local | Float; raw 0 to 4096; x | Latched gains actually applied to the completed frame, independent of configured manual gains during AWB. |
| FpgaAppliedBalanceGreen | RO / read-only or local | Float; raw 0 to 4096; x | Latched gains actually applied to the completed frame, independent of configured manual gains during AWB. |
| FpgaAppliedBalanceBlue | RO / read-only or local | Float; raw 0 to 4096; x | Latched gains actually applied to the completed frame, independent of configured manual gains during AWB. |
| FpgaDetailProcessingAvailable | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FpgaBilateralEnable | RW / editable | Boolean; raw 0 to 1 | AMD 3x3 RGB bilateral filter after gamma/median and before ROI/scaling. Replicate border; disabled by default. |
| FpgaBilateralSigmaColor | RW / editable | Integer; raw 1 to 255; DN | RGB bilateral range sigma; default 24 DN. |
| FpgaBilateralSigmaSpace | RW / editable | Float; raw 64 to 1024; px | RGB bilateral spatial sigma; Q8 value, default 1 pixel. |
| FpgaSharpenStrength | RW / editable | Float; raw 0 to 256 | RGB cross-Laplacian strength [0,1] using AMD filter2D, saturated per channel. Zero bypasses. Constant-zero filter border can brighten image edges; assess halos and noise amplification. |
| FpgaLensShadingAvailable | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FpgaLensShadingEnable | RW / editable | Boolean; raw 0 to 1 | Programmable RAW10 lens shading after black/BPC and before demosaic or RAW manual gains. Four-phase 7x9 bilinear Q12 grid; default disabled with unity table. A real uniform-field calibration is required for optical correction. |
| FpgaLensShadingPhase | RW / editable | Blue, GreenBlueRow, GreenRedRow, Red | BGGR calibration plane; the two green CFA phases have independent tables. |
| FpgaLensShadingRow | RW / editable | Integer; raw 0 to 6 | See current readback, capability and firmware constraints. |
| FpgaLensShadingColumn | RW / editable | Integer; raw 0 to 8 | See current readback, capability and firmware constraints. |
| FpgaLensShadingGain | RW / editable | Float; raw 0 to 16384; x | Selected grid gain in [0,4], Q12 rounded interpolation and saturated RAW10 output. Default 1x. All 252 gains persist in UserSet; selectors do not. |
| FpgaSceneAnalysisAvailable | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FpgaSceneAnalysisEnable | RW / editable | Boolean; raw 0 to 1 | Post-ROI RGB luma focus/motion diagnostics. Both output dimensions must be at least 32 for analysis; smaller images pass with zero/invalid metrics. First frame after acquisition restart has invalid motion. Illumination changes can trigger motion, and noise can increase focus. |
| FpgaFocusMean | RO / read-only or local | Float; raw 0 to 66846720; DN | Mean absolute four-neighbor luma Laplacian over the output interior, Q16 DN. Sensitive to ISP settings and noise. Latched with the common completed-frame statistics sequence. |
| FpgaMotionMean | RO / read-only or local | Float; raw 0 to 16711680; DN | Mean absolute difference of current/previous 32x32 luma-bin means, Q16 DN. First frame is invalid. Latched with the common completed-frame statistics sequence. |
| FpgaMotionValid | RO / read-only or local | Boolean; raw 0 to 1 | Latched valid preceding-frame comparison. False when analysis is disabled, dimensions are below 32, or history was reset. |
| FpgaYuvOutputAvailable | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FpgaYuvRange | RW / editable | Limited, Full | BT.601 output levels for the FPGA RGB-to-YUV path. PixelFormat selects YUV444, four averaged-chroma YUV422 orders or legacy 2x2-chroma YUV420. Requires FPGA processing enabled and RAW10 sensor acquisition; native sensor YUV bypass retains its own timing and levels. RGB/BGR bypass is unchanged. Default Limited. |
| FpgaPolyphaseAvailable | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FpgaScalerInterpolation | RW / editable | Area, Polyphase | Resample RGB/BGR or FPGA YUV using Area (default, downscale only) or six-tap, 64-phase Xilinx VPSS. Polyphase allows up to 4x enlargement per ROI axis, within 2592x1944 output and PixelFormat alignment limits. Larger downscale ratios use an area prefilter. Mixed up/down axes are supported. Equal geometry bypasses VPSS; an unchanged axis uses identity coefficients. Configure while stopped; output pixel rate and Ethernet payload can limit FPS. |
| FpgaLocalContrastAvailable | RO / read-only or local | Boolean; raw 0 to 1 | See current readback, capability and firmware constraints. |
| FpgaLocalContrastEnable | RW / editable | Boolean; raw 0 to 1 | Four-by-four-tile Xilinx CLAHE on RGB value, after optional scaling. Preserves RGB ratios with bounded fixed-point rounding; black remains black. Requires color FPGA processing, output dimensions at least 32, height multiple of 4 and width multiple of 8. Uses the preceding frame histogram; first frame after Start, geometry/clip change or reset bypasses. Can amplify noise or react to illumination changes. Default off; no optical calibration is implied. |
| FpgaLocalContrastClipLimit | RW / editable | Integer; raw 1 to 8 | Integer Xilinx CLAHE histogram clip multiplier, 1 through 8, default 4. Higher values allow stronger contrast/noise amplification. Configure while stopped. |
| FpgaBlackLevelLiveActive | RO / read-only or local | Boolean; raw 0 to 1 | True only while healthy audited v9/v10 FPGA RAW processing accepts frame-latched live Black Level writes. False during stop/drain/recovery, bypass and legacy processing. Other controls retain their existing locks. |
| FpgaUpscaleAvailable | RO / read-only or local | Boolean; raw 0 to 1 | Firmware supports bounded enlargement using the installed VPSS (processing v8 or later). Requires processing/scaler enabled and Polyphase selected. No additional FPGA resources; camera FPS acceptance is separate. |
| FpgaUpscaleMaximum | RO / read-only or local | Integer; raw 1 to 4; x | Maximum supported enlargement per ROI axis: 4 with VPSS, otherwise 1 (no enlargement). Output remains at most 2592x1944 and subject to PixelFormat alignment and link bandwidth. Area never enlarges. |
| FpgaAdaptiveToneAvailable | RO / read-only or local | Boolean; raw 0 to 1 | Matching v11 image includes the adaptive RGB display curve after CLAHE and before final scene statistics/output packing. This is not HDR radiance reconstruction. |
| FpgaAdaptiveToneEnable | RW / editable | Boolean; raw 0 to 1 | Opt-in shared per-channel rational RGB8 curve driven by preceding input-frame weighted luma. Black/white endpoints fixed; gain 0.5x to 2x changes by at most 1/16 per frame. Can change color ratios and temporal brightness. First/start/reset/dimension/target-change frame bypasses. Configure while stopped. Default off; requires color FPGA processing. |
| FpgaAdaptiveToneTarget | RW / editable | Integer; raw 16 to 224; DN | Desired preceding-input weighted-luma mean, (R+2G+B)/4, default 128. Target is a bounded adaptation reference, not guaranteed output brightness. Configure while stopped; new acquisition resets history. |
| FpgaAdaptiveToneAppliedGain | RO / read-only or local | Float; raw 128 to 512; x | Unlatched current HLS invocation gain; unity before first valid invocation. Not associated with a specific GVSP frame or common statistics latch. |
| FpgaAdaptiveTonePreviousMean | RO / read-only or local | Integer; raw 0 to 255; DN | Unlatched preceding input-frame weighted-luma mean used by the current HLS invocation. First/reset invocation reports zero. Not frame-tagged; gain/mean reads are not an atomic pair. |
| FpgaDistortionAvailable | RO / read-only or local | Boolean; raw 0 to 1 | Matching image/control support for bounded640x480 nearest correction, +/-7 source rows. Does not imply calibrated lens parameters. |
| FpgaDistortionEnable | RW / editable | Boolean; raw 0 to 1 | Volatile correction enable. Requires a completely validated map matching current color geometry. Default off; full-resolution bypass remains supported. Not saved in UserSet. |
| FpgaDistortionMapWidth | RW / editable | Integer; raw 1 to 640 | Staged map dimension. Set only after Abort, before Begin, with correction disabled. Must match output geometry; no implicit scaling. |
| FpgaDistortionMapHeight | RW / editable | Integer; raw 1 to 480 | Staged map dimension. Set only after Abort, before Begin, with correction disabled. Must match output geometry; no implicit scaling. |
| FpgaDistortionExpectedCrc32 | RW / editable | Integer; raw 0 to 4294967295 | CRC32 of raster little-endian32-bit map words, polynomial0xedb88320, standard initial/final XOR. Set before Begin; integrity check is not optical calibration. |
| FpgaDistortionMapBegin | WO / editable | Command; raw 0 to 1 | Volatile map transfer command. Begin selects a staging bank; Commit starts bounded asynchronous validation; Abort invalidates staging without releasing a bank the reader may still access. Configure while stopped and disabled. |
| FpgaDistortionMapCommit | WO / editable | Command; raw 0 to 1 | Volatile map transfer command. Begin selects a staging bank; Commit starts bounded asynchronous validation; Abort invalidates staging without releasing a bank the reader may still access. Configure while stopped and disabled. |
| FpgaDistortionMapAbort | WO / editable | Command; raw 0 to 1 | Volatile map transfer command. Begin selects a staging bank; Commit starts bounded asynchronous validation; Abort invalidates staging without releasing a bank the reader may still access. Configure while stopped and disabled. |
| FpgaDistortionMapIndex | RW / editable | Integer; raw 0 to 307199 | Raster map word index. Upload sequentially; explicit index/value pairs make identical retries idempotent. |
| FpgaDistortionMapData | RW / editable | Integer; raw 0 to 4294967295 | Map word: valid bit31, source x bits9:0, source y bits18:10, reserved bits30:19 zero. Zero means black. Coordinates must be in bounds and within seven source rows; unsupported entries fail validation. |
| FpgaDistortionMapState | RO / read-only or local | Empty, Uploading, Validating, Ready, Error | See current readback, capability and firmware constraints. |
| FpgaDistortionWordsUploaded | RO / read-only or local | Integer; raw 0 to 307200 | See current readback, capability and firmware constraints. |
| FpgaDistortionWordsValidated | RO / read-only or local | Integer; raw 0 to 307200 | See current readback, capability and firmware constraints. |
| FpgaDistortionMapError | RO / read-only or local | None, InvalidCoordinate, ChecksumMismatch | See current readback, capability and firmware constraints. |
| FpgaDistortionHardwareStatus | RO / read-only or local | Integer; raw 0 to 7 | Last observed reader status: bit0 geometry, bit1 map encoding/window, bit2 input framing. Zero alone does not prove successful acquisition. |

### ImageFormatControl

| Feature | Camera access / web | Type and range | Description |
|---|---|---|---|
| Width | RW / editable | Integer; raw 160 to 2592 | Output dimensions. With FPGA processing/scaler enabled, InputWidth/InputHeight and ROI select sensor input; Polyphase supports bounded enlargement. Otherwise uses native sensor geometry/scaling. Stop before changes. |
| Height | RW / editable | Integer; raw 120 to 1944 | Output dimensions. With FPGA processing/scaler enabled, InputWidth/InputHeight and ROI select sensor input; Polyphase supports bounded enlargement. Otherwise uses native sensor geometry/scaling. Stop before changes. |
| PayloadSize | RO / read-only or local | Integer; raw 0 to 16000000 | See current readback, capability and firmware constraints. |
| PixelFormat | RW / editable | Mono8, YUV422Packed, YUV422_YUYVPacked, RGB8, BGR8, RGB565p, BayerBG8, BayerBG10, BayerBG10p, YUV8_UYV, RGB555_16BE, RGB444_16BE, YUV420Legacy, YUV422_YVYU, YUV422_VYUY, YUV422_8_UYVY, YUV422_8 | Native CSI capture or optional FPGA RAW10-derived RGB/BGR and YUV processing on available hardware. With FPGA processing disabled, RGB8/BGR8 use sensor YUV422 and CPU conversion. YUV444 remains limited to 640x480. Custom formats are documented byte layouts, not standard PFNC. RAW uses an unbinned crop with fixed BG phase; ReverseX/Y are locked in RAW. Output format and payload can change sensor cadence and Ethernet limits. |
