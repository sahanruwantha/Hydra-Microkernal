# Presentation

[PowerPoint](Hydra_Research_Summary.pptx) ·
[PDF](Hydra_Research_Summary.pdf) ·
[36-second demo video](hydra_qemu_checkpoint_recovery.mp4)

Slides 1–12 cover about ten minutes; slides 13–15 provide supporting statistics,
fault recovery and references. Speaker notes include timings and explanations.
Keep the MP4 beside the PowerPoint for the slide 8 link, or play it separately.
Video playback inside PowerPoint has not been verified.

## Terminal images

| Evidence | Image | Transcript or annotated version |
|---|---|---|
| Both invocation paths | [Terminal](01_both_call_paths.png) | [Commands and output](01_both_call_paths.txt) |
| Historical performance analysis | [Terminal](02_historical_performance.png) | [Commands and output](02_historical_performance.txt) |
| Checkpoint and restore | [Clean](03_checkpoint_restore_clean.png) | [Arrows and highlights](03_checkpoint_restore_annotated.png) |
| Task failure and recovery | [Clean](04_fault_recovery_clean.png) | [Arrows and highlights](04_fault_recovery_annotated.png) |

The images are terminal-style renderings of captured command output. The video
is an edited replay of real QEMU serial output with pauses for reading. These
captures precede the current source revision; startup output, PIDs and fresh
timings can differ. The clean images are ready for your own arrows.

## Original evidence

- [Call-path and analysis capture](call_path_evidence.zip): full successful
  KERNEL/USERSPACE logs, historical analysis, metadata and source patch for
  images 01 and 02. The captured boots use a timeout to stop the kernel;
  successful completion is checked using PASS markers and restore counts.
- [Checkpoint/recovery recording](hydra_qemu_demo_evidence.zip): original timed
  recording, UART logs, source/build identity, edit schedule and regression
  evidence for the video and images 03 and 04.

Both historical invocation paths run in supervisor mode. Historical timing
observations and later acknowledged/isolation tests are separate results.
The local send/receive BENCH tags are placeholders for transport.

For current commands, objectives and a live rehearsal, use the
[demo guide](../demo.md) and `./scripts/presentation_demo.sh 60`. Each new run
saves its own logs and build identity under `build/demo/`.
