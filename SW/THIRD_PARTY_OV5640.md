# OV5640 source attribution

`src/Ov5640Tuning.inc` adapts the register/value initialization table from
Linux 6.6.40 `drivers/media/i2c/ov5640.c`, licensed GPL-2.0-or-later:

Copyright (C) 2011-2013 Freescale Semiconductor, Inc. All Rights Reserved.
Copyright (C) 2014-2017 Mentor Graphics Inc.

Upstream source: https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/drivers/media/i2c/ov5640.c?h=v6.6.40

The new Ov5640 driver files are also marked GPL-2.0-or-later. The license
is included in `LICENSES/GPL-2.0.txt`. Register definitions and control
semantics were checked against the locally supplied OV5640 datasheet v2.03.

On 2026-10-23 the table was refreshed from torvalds/linux master:
https://github.com/torvalds/linux/blob/master/drivers/media/i2c/ov5640.c
All 219 register/value/delay entries are preserved in order, including the
3008 power-up write and 4837 clock-period baseline. The caller gates output
before loading the table. Mode PLL/timing/format values are applied later
by Configure. Calibration values remain the Linux baseline, not a measured
calibration of this particular lens/module.
