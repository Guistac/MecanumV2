# Radio reliability (RFM23BP)

Investigation and fixes for two problems seen on both the robot board and the remote board
(Teensy 4.1 + RFM23BP, October 2026):

1. The radio sometimes refuses to initialize at power-up, even after many retries.
   In bad cases only several real power cycles, or leaving the board unpowered for a few minutes, helps.
2. The firmware sometimes hangs, apparently waiting for a radio event that never comes.

The firmware side is addressed on branch `radio-hardening`. The hardware side needs a PCB revision,
which is not planned in the short term.

| Document | Contents |
| --- | --- |
| [01 - Issues and root causes](01-issues-and-root-causes.md) | What happens, why, and how sure we are |
| [02 - Firmware hardening](02-firmware-hardening.md) | What was changed in the firmware, how it behaves, settings |
| [03 - Diagnostics to run](03-diagnostics-to-run.md) | Bench measurements that are still needed to confirm the causes |
| [04 - PCB revision recommendations](04-pcb-revision-recommendations.md) | Hardware changes for the next board revision |

Hardware references used: `Mecanum_V2 PCB/Robot_PCB` and `Mecanum_V2 PCB/Remote_PCB` KiCad projects,
and the datasheets in `Mecanum_V2 PCB/Remote_PCB/Datasheets` (RFM23BP, TXB0108).
