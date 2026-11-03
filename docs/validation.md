# Validation — 2026-11-02

## Signed macOS installer — 2026-11-03

- `./package.sh` completed a Release build and produced `dist/Moth-DSP-Lab-0.1.0.pkg` for arm64. The app and installer have timestamped Developer ID signatures; Apple notarization returned Accepted, stapler validation passed and Gatekeeper accepted the installer as Notarized Developer ID.
- All five CTest suites passed. VST3, AU and hardware-comparison offline integration tests also passed using the staged Developer ID signed app with hardened runtime enabled. No physical audio was emitted and the running app was not replaced.
- The installer contains only the app under `/Applications`; bundle relocation is disabled. Packaging excludes measurements, profiles, credentials and fixture plugins. The installed app uses the existing Documents storage policy. Installation itself and an Intel/universal build were not exercised.
- Signing entitlements allow microphone/interface capture and third-party plugin loading. `package.sh --help`, Bash syntax and entitlement plist validation passed. Distribution artifacts and notarization evidence are Git-ignored under `dist/`.

Environment: macOS arm64, AppleClang 17, Release build, pinned JUCE 8.0.9.

- CMake configure and full build succeeded.
- CTest: 2/2 test suites passed (12 core cases plus end-to-end VST3 integration).
- Core cases: identity, gain, delay/polarity, low-pass EQ, clipping, silence, incompatible audio, WAV/session persistence, settings bounds, binary state encoding, float WAV values above full scale, disabled AI.
- VST3 integration: scan, host-public parameter configuration, actual host-state serialization, -6.0206 dB gain, 137-sample reported latency compensation, exact raw/compensated tail lengths, bit-identical state replay, block-size invariance for the fixture, abrupt worker termination, missing-plugin failure and completed-session preservation.
- Installed-plugin smoke: PhaseMeter versus TB_GonioMeter_v1, 48 kHz stereo, 512-sample blocks, 997 Hz input, 0.2-second tail. Both rendered and exported successfully; gain difference 0 dB, additional delay 0 samples, residual at the -160 dBFS reporting floor. These are transparent meter plugins; this smoke does not establish compatibility with every nonlinear/complex VST3.
- Desktop app launched and remained running. Interactive parameter-window, file-dialog and playback usability still need human acceptance testing; no physical audio/hardware loopback was performed.
- macOS CI workflow added; remote CI has not been run. Windows/Linux unverified.

The installed-plugin smoke artifacts remain locally under `build/smoke/project/sessions/` (ignored by Git). Open their `manifest.json` in the app to inspect the plots and restored session.

## Current milestone status

M0 foundation and the first usable M1 vertical slice are implemented. This is not the entire specification or the full M1 feature set. Remaining M1 work: plugin chains, native plugin GUI/preset tools, directory scan/cache, bypass, FileTarget selection and broader plugin/platform testing. Storage is versioned JSON/WAV; SQLite indexing is deferred. M2 hardware, M3 connected AI, M4 optimization and M5 source-aware changes are not implemented.

## AU support follow-up — 2026-11-02

- macOS build now enables Audio Unit hosting alongside VST3. The UI accepts installed AUv2 `.component` bundles.
- AU descriptors retain their registered type/subtype/manufacturer identifier through session save/load; VST3 path-based loading remains unchanged.
- CTest: 3/3 suites passed, including the existing 12 core cases and VST3 suite plus the new Apple AUHighPassFilter integration suite.
- AU integration verifies parameter changes, host state serialization/restoration, identical AU–AU captures, repeatable rerender and mixed AU–VST3 export.
- The same AU integration script also passed with the installed `/Library/Audio/Plug-Ins/Components/PhaseMeter.component`, including its auxiliary input buses. AUs that reject disabled auxiliary buses retain those buses with zero-filled input; this policy and actual channel counts are recorded in render metadata.
- AUv3 extensions are not included. GUI file-picker/editor interaction remains a manual acceptance check; scan/configure/render/state restoration were exercised through the real isolated workers.

## Guided hardware setup check — 2026-11-02

- Added a four-step Hungarian guide for Scarlett jack output → Palmer DACCAPO → B7K Ultra jack return, plus an explicitly armed setup probe.
- The UI requires interface/channel selection and three manual confirmations. Device enumeration does not open a stream. macOS audio-input permission is requested before device opening; NSMicrophoneUsageDescription was verified in the built app bundle.
- The 2.6-second preallocated probe is capped at -36 dBFS peak. No captured audio is fed back to the outputs. Two burst correlations estimate delay and repeatability; baseline, headroom, SNR and trailing signal are assessed after stopping the device.
- Tests now include 13 core groups. Hardware cases cover known 137-sample delay, silence/disconnection, polarity reversal, overload, repeated-delay drift, persistent tail, bounded output, silent unselected outputs, captured callback data, cancellation, non-finite input, missing channel, high initial baseline and restart state reset.
- Full Release build succeeded; CTest passed 3/3 suites (core, VST3 integration, AU integration).
- No physical test signal was emitted during development. The actual Scarlett/DACCAPO/B7K chain, manual tutorial interactions, microphone permission dialog and device hot-unplug behavior require user acceptance on the connected hardware.
- This completes the connection-guide/setup-check increment only. Absolute analog level calibration and active-pedal-versus-Darkbass capture remain future M2 work. A successful setup check does not verify physical cable identity or certify electrical safety.

## Supervised bench session — 2026-11-02

- Corrected UTF-8 decoding for Hungarian UI/diagnostic strings. Accessibility inspection confirmed “Hardver bekötés / teszt” is now displayed correctly.
- Foreground GUI launches now reuse the existing instance; isolated `--worker`/`--compare` processes remain permitted. All 3 CTest suites passed after the change.
- Connected hardware identified as Scarlett 2i2 USB, rather than the initially assumed Solo. User confirmed physical routing and disabled monitoring before any probe.
- Initial 192 → 48 kHz transition was followed by cancellation after 768 captured samples, before the first probe burst. A subsequent run at 48 kHz completed. The cause of the initial stop is not conclusively identified.
- Direct Output 1 → Input 2 LINE loopback passed: 740 samples / 15.42 ms, 35.81 dB SNR, no reported xruns.
- DACCAPO-only path into Input 2 INST initially had a recognisable but low return. With user-adjusted Input 2 gain approximately 12 o’clock, it passed: 41.20 dB SNR, -42.61 dBFS peak, 740 samples delay, zero repeated-delay difference, no xruns. MONITOR approximately centre; DACCAPO LEVEL approximately 9 o’clock.
- Each diagnostic-routing report was annotated with `operatorVerifiedRouting` and reported physical settings. The application's `profile` value is the guide preset; the annotation identifies the actual loopback or reamp-only wiring. Raw waveforms and numerical analysis were not changed.

### Full B7K bypass chain acceptance

Supervised physical setup test completed at 2026-11-02 19:33:41 UTC (20:33:41 Europe/Budapest).

- Actual chain: Scarlett 2i2 USB Output 1 → Palmer DACCAPO → B7K Ultra INPUT → B7K jack OUTPUT → Scarlett Input 2 INST. Pedal full bypass; Direct Monitor and DAW monitoring disabled, speakers disconnected, as confirmed by the user.
- User-reported physical settings: Scarlett MONITOR approximately centre; DACCAPO LEVEL approximately 9 o’clock; Input 2 GAIN approximately 12 o’clock; DACCAPO ground switch GND.
- Final result: passed at 48 kHz / 256 samples, -36 dBFS probe peak. Return RMS -52.26 dBFS, peak -41.88 dBFS, baseline -83.81 dBFS, SNR 31.54 dB, correlation 0.9724. Round trip 740 samples (15.42 ms); repeated delay difference 0 samples; reported xruns 0.
- Before changing the DACCAPO switch to GND, the full chain had -60.62 dBFS baseline and 8.89 dB SNR. Baseline analysis showed dominant 50 Hz and 150 Hz components. Changing only the ground switch reduced the measured baseline by 23.18 dB. This establishes the quieter switch setting for this setup, not a definitive circuit-level diagnosis.
- Local final report: `~/Library/Moth DSP Lab/setup-checks/20261102T193344Z-5f3dbd8ad20b4f4caa4fa07ac99f0c06/setup-check.json`; probe/capture WAVs are in the same folder. Operator routing and physical settings were added to that report; numerical analysis and raw captures were preserved.
- The output stopped at completion. This verifies the bypass signal path at the recorded settings; active pedal characterisation, absolute analog level calibration and Darkbass matching remain separate work.

## Guided active-pedal / DarkBass comparison — 2026-11-02

- Added in-app bypass, active clean/EQ and active drive stages with physical-setting instructions, manual confirmations and recorded knob notes. Active measurements require a fresh passing bypass check in the current panel and unchanged interface/reamp routing.
- Installed DarkBass AU 0.1.0 was scanned, configured and rendered successfully in isolated workers for both clean and drive profiles. These checks used a saved stimulus; no physical audio was emitted.
- Hardware capture is aligned using the bypass delay and compared with the plugin's left output without normalization or further active-signal alignment. Raw captures, plugin state, settings and provenance are retained. Absolute analog level calibration and similarity scoring remain unavailable.
- Release build succeeded; all four CTest suites passed (core, VST3 integration, AU integration, hardware comparison). The new comparison test verifies known delay and residual, saved artifacts, and rejection of changed routing, incomplete/error captures, missing confirmations/notes and failed bypass checks.
- New active-pedal captures still require the operator to follow and confirm the displayed settings and start each measurement in the application.

## Persistent guided workflow — 2026-11-03

- Replaced the ambiguous hardware entry/result handoff with a Hungarian guided flow, explicit next-action banner, stage overview, separate completed-result selector and saved workflow history. Wiring pages progressively reveal route/confirmation/capture controls. The result window names the stage and local timestamp; hardware playback stays disabled and idle stop controls are disabled.
- Added atomic per-workflow progress, stage-specific notes, remembered route selections, project restoration, fresh bypass/confirmation requirements after reopening, and non-destructive new workflows. Legacy hardware captures are grouped by bypass provenance. Recovery recognizes committed results missed by an interrupted UI update. Interrupted raw captures are repeated; preserved full captures can be rendered again without audio output using their recovery metadata.
- Tests cover interrupted capture state, retained notes/results, clean-versus-drive completion, missing artifacts, crash between commit and journal update, idempotent legacy import, corrupt JSON and write failure. UI tests instantiate the actual panel to check restored gating, note edits, completed/new flows and unsuccessful offline retry preserving raw capture.
- Release build and all five CTest suites passed during validation (core/workflow persistence, VST3, AU, hardware comparison, workflow UI). Native macOS screenshots inspected for the completed real series and result screen; synthetic panel snapshots inspected for resumed and new workflows. Opening a graph closes the wizard automatically.
- Existing November 2 clean/EQ and drive captures were retained and imported into a workflow. No new physical test signal was emitted. End-to-end physical operation of the changed prompts still requires the operator's next supervised measurement; absolute analog calibration is unchanged.

## Single-window task and feedback view — 2026-11-03

- Embedded the guided hardware component in the main window; removed the hardware dialog and manual graph handoff. The default layout keeps the current task on the left and capture/result feedback on the right. Optional plugin A/B controls use the same window; native file selection and the advanced isolated plugin editor retain their existing dialogs.
- Short task instructions replace the long default guide. Normal progression uses Next/Advance rather than a manual measurement-mode selector. Route controls appear only during bypass setup, and changed routing can explicitly return to a fresh bypass check. The complete report is optional; inline graphs and a short stage/time/level/correlation summary appear automatically on completion or saved-result selection.
- Added capture progress and a peak-held input level, transferred from the audio callback through an atomic value. No recording-buffer reads, allocations or UI calls were added to the callback. Project/mode switching is disabled while capture, preparation or analysis is running; switching back from offline playback stops and closes playback first.
- Native accessibility inspection confirmed one application window with the existing drive result visible. UI regression checks verify inline result selection, no desktop window for the plot, and visible-control bounds at the minimum supported window size. Existing restore/authorization/retry tests remain in place. No physical audio was emitted during verification.

## UX and reusable series — 2026-11-03

- Added a shared dark theme with distinct task/result cards, labeled stage progress, primary/secondary/danger button treatment, visible keyboard focus and a logical tab order. Pending confirmations now have specific next-action messages.
- Series names save automatically and appear in history. Repeating a completed series creates a new ID with remembered route selections and stage notes, without results or physical confirmations. The original series is preserved.
- Added persistence and UI tests for named restoration, repetition, source preservation and fresh capture gating. Chart axes now show seconds, frequency and dB labels. New/active UI screenshots and minimum-size bounds were inspected; physical capture was not exercised.
