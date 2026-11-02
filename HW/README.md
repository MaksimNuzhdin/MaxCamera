# Hardware design

Target: ALINX AXU2CGB, `xczu2cg-sfvc784-1-e`, AN5641/OV5640 on the populated CSI
connector. Top module `logic`; video domain200 MHz. Vivado, Vitis HLS and Vitis
2024.2 are the pinned tool generation. Install the applicable AMD IP licenses
(including camera/video dependencies); this repository does not redistribute them.

## Source recreation

1. Obtain official AMD Vitis Libraries tag `v2024.2_rel`, commit
   `80bc4808a99ab76ab304d80640047c2a64ae47dd`. `bash HW/fetch_vision.sh` explicitly
   downloads the official archive only when missing and verifies SHA256
   `5fd9e52642116d0bcc180cafe7bb265f02ada1fec57aa9ba1911dfc75ab2dfe7`.
   Preserve its upstream license. Install OpenCV development headers and Python3.
2. Set `VITIS_VISION_INCLUDE` to that tree's `vision/L1/include`, and
   `OPENCV_INCLUDE` to your OpenCV4 include directory. With Vitis HLS2024.2 on PATH:

   ```bash
   vitis_hls -f HW/build_hls.tcl
   ```

   This performs C simulation, synthesis and source-IP export for seven cores:
   RAW correction v5, color pipeline v9, scene metrics v8, RGB output, local
   contrast, adaptive tone and integer remap. Generated IP stays in `HW/build/hls`.
3. With Vivado2024.2 on PATH:

   ```bash
   vivado -mode batch -source HW/recreate.tcl
   ```

   This creates a fresh project, registers custom/HLS IP, sources the accepted
   block-design Tcl, explicitly pins MIPI bank64/pinsAC9/AE9/AB8 and the32-bit
   control interconnect, validates and generates the wrapper. It does not synthesize
   or access hardware. An existing generated IP root can be selected explicitly
   using `MAXCAMERA_HLS_IP_ROOT`; no development-workspace path is embedded.
4. For an intentional full build, use a fresh output directory:

   ```bash
   MAXCAMERA_HW_BUILD=HW/build/full MAXCAMERA_HW_BUILD_BITSTREAM=1 \
     vivado -mode batch -source HW/recreate.tcl
   ```

   Review timing, utilization and DRC before pairing the XSA/bitstream with SW.
   This command does not program the board. Full-build timing and bit-identical
   reproducibility of this export have not been measured.

`design/design_1.tcl` is deliberately retained generated **source configuration**;
so are `ip/MAX_FAN_CONTROLLER_1_0/component.xml`, XGUI/BD Tcl, RTL, XDC and software
register headers. Generated XCI implementations, runs, caches, DCP and bitstreams
are excluded. The fan IP's ownership/license needs confirmation before publication.
Do not omit it: it is an actual instantiated dependency.

A fresh HLS export must preserve the firmware register ABI. Compare exported
`*_hw.h` offsets against `SW/gige/GigeFpga*Registers.h`; do not silently replace
firmware headers after a mismatch. Header and part defaults alone do not prove
compatibility. Main-tree v13 board evidence remains a separate historical claim.

The accepted `design/design_1.bd` is retained as source-configuration provenance.
Use the Tcl recreation entry point rather than importing it directly: fresh IP
default propagation can change metadata. The original interconnect decomposition
is not promised bit-identical; a regenerated image requires full implementation
and board validation before replacing the qualified image.
