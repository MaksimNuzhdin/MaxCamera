# Source export validation

Prepared2026-11-02 without changing the working development tree or camera.

Passed:

- Exported R5 compile/link using Vitis2024.2 and the existing qualified standalone
  platform: text655344, data3108, BSS154802864 bytes. This new ELF is not the sealed
  board-qualified ELF and was not loaded.
- Exported native GigE protocol/image/resend/timestamp tests.
- Remap map/control/reader/poll tests, seven upload tests and five map compiler tests.
- Synchronous/asynchronous SCCB and production arbitration/cache sanitizer tests.
- HDR merge/session/worker sanitizer tests, C++ linkage and six plateau tests.
- Five distortion-map tests; source English-language check.
- Fresh Vivado project/block-design validation using explicit existing HLS IP
  exports. Top-level external interfaces, scalar/interface connections and address
  map match the accepted design. No source relies on the original project path.

Not run or not established:

- A fresh full seven-core HLS synthesis/export, full FPGA synthesis/routing and
  new-image camera acceptance. Existing generated HLS IP was supplied explicitly
  to the recreation smoke check; it is not bundled.
- Bit-identical reproduction. Fresh Vivado interconnect internals differ from the
  accepted generated project. The original Tcl and accepted BD source configuration
  are both retained; internal defaults must be reviewed with full implementation.
- New-platform BSP generation; software test used the existing matching BSP.
- Optional external GenApi XSD/Aravis runtime checks and formal certification.
- HDR scene/exposure association, optical calibration, wider4x modes and endurance.

Initial smoke failures were packaging issues: Vitis CMake needed its bundled
runtime libraries, and one test helper was missing from the selected source tree.
Both were corrected and rerun successfully. Direct BD import also changed metadata;
the supported entry point uses the exported Tcl. No board acceptance was relaxed.

Generated test/build output and detailed local logs are preserved outside this
export. `tools/validate_source_tree.py` reports intended paths, sizes and SHA256,
rejects common credential shapes/workstation paths and checks Markdown targets.
This is a focused source-publication review, not a guarantee from a commercial
secret scanner. No credential files or private workstation directories are included.
