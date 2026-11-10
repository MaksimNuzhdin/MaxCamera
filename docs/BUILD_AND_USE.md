# Build and operate

November10 update: [current project status](PROJECT_STATUS.md) supersedes older
research-only status prose. [The Ethernet console](../SW/ai/web/ETHERNET.md)
shares the camera IP with GigE Vision and has bounded physical video/face/HTTP
evidence. [MicroSD preparation](MICROSD_BOOT.md) is integrity checked; physical
cold boot is still unqualified. The November9 hour run remains failed. No new
optical, maximum-mode, endurance or certification acceptance is inferred.


The portable entry points are [HW recreation](../HW/README.md) and
[SW compilation/tests](../SW/README.md). They replace development scripts that
cloned previous generated projects or embedded workstation paths.

Tools: AMD Vivado/Vitis/Vitis HLS2024.2, applicable video/MIPI IP licenses,
Vitis Vision2024.2_rel, OpenCV development headers, Python3, native C/C++ compilers
and sanitizer runtimes. Vendor tools, SDKs and generated platform/IP files are
external dependencies, not source assets in this repository.

The exported SW compiled and linked using the existing qualified R5 platform.
Vivado source recreation/validation was exercised using existing generated HLS IP
as explicit inputs; a fresh seven-core HLS build and complete new route were not
run for packaging. Generated interconnect internals can differ from the accepted
project. Do not claim bit-identical or board-qualified output from source recreation.

Loading is intentionally not automated. Use an exclusive camera session, a
matched XSA/bitstream/ELF and the board's existing volatile JTAG/FSBL workflow.
Snapshot runtime settings, stop acquisition, load only the approved compatible
pair, run bounded acceptance, restore settings and release control. Do not infer
Flash/boot/UserSet persistence permission from build instructions.

See [verification scope](VERIFICATION.md), [GenICam/ISP guide](ISP_FEATURES.md)
and the [driver documentation](drivers). No endurance run starts automatically.
