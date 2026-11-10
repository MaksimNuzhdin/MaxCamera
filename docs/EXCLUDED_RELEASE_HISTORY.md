# Excluded portfolio research history

Current-status note, November10,2026: the dated plans below retain their original
scope. The later A53 analytics and same-IP Ethernet web implementation is now
bounded-camera tested. Publication of the current update is authorized. See
[Project status](PROJECT_STATUS.md) and [MicroSD boot](MICROSD_BOOT.md) for the
current implemented/open items; do not interpret an older research-only or
publishing-pending statement as the current feature state.


These notes are retained for provenance, not active portfolio-release tasks.
The user excluded hardware trigger and dataset/custom-training work on
November 1, 2026. No functioning code or historical evidence was deleted.
Earlier estimates below are explicitly historical, not current resource budgets.

## Hardware trigger (excluded)

The earlier proposal required verified connector pins, voltage/protection,
synchronization/filtering and an OV5640/FREX-consistent exposure path, followed
by measured latency/jitter and SFNC Trigger control tests. No trigger feature
is promised or added to the release.

## Earlier AI proposals (not release requirements)

- **First AI prototype - ROI classification.** Select a concrete task
  (part presence/type/correctness), collect and split training/validation data,
  train a compact quantized model, and measure accuracy, latency, FPS, and resources
  with ISP/GigE. Compare FINN, hls4ml, and A53 execution; A53 needs separate
  integration with the current R5 application.
- **AI detection and defect inspection.** Investigate target objects,
  scratches, chips, and assembly errors on reduced frames/ROIs; define minimum
  defect size, recall, and false-positive requirements.
- **Bounded-region OCR.** Read specified markings/digits/displays; collect data
  using the actual optics/lighting and assess detector/recognizer quality/cost
  separately.
- **AI denoise and super-resolution research.** Start with a small ROI and
  compare conventional filters for quality/resources. Check fine-detail retention
  and hallucinated structures; do not promise full 5MP throughput on ZU2CG before
  measurements.
- **AI architecture and resource budget.** Do not add a generic DPU without
  assessment: reference DPUCZDX8G B512 uses 26391 LUT/72 BRAM/118 DSP. The earlier
  two-receiver comparison had 23861 LUT free; the newer integrated single-camera
  RAW design historically had 36667 LUT, 134.5 BRAM36, and 233 DSP free. The
  early combined color increment historically left 30205 LUT, 103 BRAM36 and
  190 DSP free. These are not the current budget. Accepted v9 leaves 8187 LUT,
  44 BRAM36 equivalents and 7 DSP, with +0.451 ns setup/+0.003 ns hold slack.
  Native Xilinx GTM feasibility alone estimates another 16174 LUT/62 DSP;
  appending that mapping exceeds current DSP capacity. Resource arithmetic
  does not establish a working combined design. Check tool versions, layer
  support, runtime/DDR ownership, quantization, and placement.
  Defer heavier models under the current user direction. Receiver-PC migration,
  external accelerators and larger-FPGA/model research are not active tasks.

Sources: [Vitis Vision 2024.2](https://docs.amd.com/r/2024.2-English/Vitis_Libraries/vision/index.html?contentId=TEnfCYcJ_EQOHK4ZnnSwlA),
[DPU resource utilization](https://docs.amd.com/r/4.0-English/pg338-dpu/Resource-Utilization),
[FINN](https://github.com/Xilinx/finn/blob/main/docs/finn/index.rst),
[hls4ml](https://fastmachinelearning.org/hls4ml/intro/faq.html).

