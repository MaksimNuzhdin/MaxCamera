# Resource-bounded adaptive RGB display mapping

Status: isolated numerical C/RTL tests and full v11 integration/routing pass.
The authorized 0.3.13/v11 board series also passes a bounded RGB8 synthetic
profile with 20 us packet spacing: first-frame bypass, final-frame integer
oracle, high/low/restart behavior, about 14.293 FPS and transport integrity.
These are camera-generated test-pattern frames, not real-scene optical benefit.
See board evidence (archived development reference; see [verification scope](VERIFICATION.md)) and
[integration](ADAPTIVE_V11_INTEGRATION.md). Zero-delay host reception lost packets;
this accepted profile does not qualify every geometry/format or endurance.

This is adaptive RGB8 display mapping, not the stock AMD luminance GTM algorithm,
not a calibrated radiometric transform and not multi-exposure HDR. It cannot
recover clipped input information. The same nonlinear channel curve can change
chromaticity; preserving RGB ratios is not claimed. Real-scene noise amplification,
exposure/AWB interaction and perceived quality require camera validation.

## Algorithm and controls

For each input pixel, accumulate integer brightness `(R + 2*G + B) >> 2` before
applying the curve. The rounded frame mean drives the following frame. A target
of 16..224 is configurable; no default target is silently calibrated. Desired
Q8 gain is rounded `target*256/mean`, clamped to 128..512. Actual gain moves at
most 16 Q8 units (1/16) per frame. A zero input mean holds the gain rather than
dividing by zero. A 256-entry LUT uses rounded
`255*g*x / (255 + (g-1)*x)`, where `g` is the Q8 gain divided by 256.
It fixes black and white and is monotonic. The same curve applies to each RGB
channel; replicated small memories provide three lookups per cycle.

First enabled frame, epoch/target/dimension changes and re-enabling after bypass
reset gain to unity and bypass that frame. Disabled operation preserves all AXIS
pixel/metadata bits. Enabled operation changes only RGB data, preserving metadata.
Invalid dimensions or targets consume no stream data, invalidate history and
return deterministic status gain 256 / previous mean 0. Firmware must reject
invalid configuration before starting acquisition to avoid a pipeline stall.

The block supports up to 2592x1944 and has no full-frame buffer. Controls must be
stopped-only during initial integration; live LUT/history mutation is not part
of this increment. Epoch changes invalidate history. Readbacks describe HLS
invocation state, not a tagged GVSP result. The integration must advertise the
capability only for the matching bitstream and retain a clean legacy bypass.

## Verified scope

The independent double-precision test oracle checks every output byte over 32
small frames: grayscale ramp, color ramp, black/white, first frame, bypass,
geometry/target/epoch changes, gain upper bound, slew and metadata. An additional
invalid-geometry transaction checks deterministic status. Exported RTL passes
all 33 transactions with XSim 2024.2; it is not an optical image-quality test.

Additional native tests pass at the full 2592x1944 frame size, including the
maximum white-input accumulator, gain lower clamp, zero-mean hold and invalid
target rejection. These maximum-size tests are C execution, not RTL timing.

Final HLS estimate: 4698 LUT, 4292 FF, 3 BRAM18, 1 DSP; pixel loop II=1 at a 5 ns
clock target. The small-frame co-simulation reports maximum 683 cycles per
invocation. LUT preparation and statistics add bounded frame overhead; this is
not proof of unchanged camera FPS. Full-design routing and measured modes remain
required. The baseline v10 has only seven unused DSPs and tight hold slack.

Initial synthesis used seven DSPs because unconstrained arithmetic expanded
multipliers. Bounded pixel-count/scaled-value arithmetic reduced this to one
without changing the mathematical mapping. The first sandbox RTL launch failed
in XSim Tcl startup. A subsequent host simulation exposed invalid-geometry
status outputs not being assigned, which left co-simulation awaiting output-valid.
That defect was fixed and the same RTL regression passes. Failed logs are retained.

Build in the configured local Xilinx environment:

```bash
"$XILINX_VITIS/bin/vitis_hls" -f tools/build_adaptive_tone.tcl
```

Source, build logs and reports: `diagnostics/adaptive-tone-20261101/`.
