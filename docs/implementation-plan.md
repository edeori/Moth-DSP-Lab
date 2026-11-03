# M0–M1 implementation plan

1. C++20/JUCE 8.0.9 CMake desktop application, shared domain library and headless tests.
2. Explicit AudioTarget, MeasurementEngine, AnalysisEngine, SessionStore, AiAdvisor boundaries.
3. Isolated VST3 and macOS AUv2 scan, parameter editor and offline renderer subprocesses. Parent owns session orchestration; a plugin crash cannot destroy completed sessions. Jobs use argument arrays, never shell commands.
4. Identical mono/stereo WAV stimulus, explicit sample rate/block size/channel mapping, saved initial/final plugin state, latency removal and bounded tail.
5. Offline level, cross-correlation alignment, waveform, windowed FFT magnitude and residual analysis. No implicit gain normalization. Raw outputs retained.
6. Versioned local JSON/WAV artifacts, SHA-256 stimulus identity, UTC timestamps, environment and reproducible settings. Publish completed sessions by directory rename.
7. Tests using known gain, low-pass EQ, clipping and delay; build and real VST3 smoke checks.

## Execution boundaries

The desktop message thread owns widgets and playback controls. A background orchestration thread waits for isolated child processes and performs file I/O/analysis. A child hosts third-party code; offline processBlock runs there with non-realtime mode and a fixed transport. Analysis and storage never run in a device audio callback. Playback uses JUCE's audio source callback only. Hardware playback/capture and routing guards belong to M2.

## Scope and limitations

M1 first increment: one VST3 or macOS AUv2 per side, mono/stereo, native generic parameter editor, explicit file selection/scan. AUv3, plugin chains, automated directory scan, SQLite indexing and full hardware support follow later. Sample-rate conversion is deliberately explicit: input WAV must match requested rate. A plugin with randomness, wall-clock dependence or external state cannot be made deterministic by the host; preserve state and check repeatability. Cross-correlation uses bounded integer-sample alignment; no fractional drift correction or inferred phase/group delay. FFT differences are output spectra, not a measured transfer function.
