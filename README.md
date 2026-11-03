# Moth DSP Lab

Native C++20/JUCE desktop application for local, offline VST3 and macOS Audio Unit (AUv2) A/B measurements. The first M0–M1 increment implements one plugin per side; a guided hardware setup check is available. Exploratory active-pedal/DarkBass comparisons are available; an opt-in Codex CLI advisor now creates versioned, executable hardware scenarios. Absolute analog level calibration remains future work.

## Build and run (macOS)

Requires Xcode command-line tools, CMake 3.22+ and a C++20 compiler. The first configure downloads the pinned JUCE 8.0.9 revision; subsequent measurement and analysis work offline. JUCE licensing applies to distribution.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 6
ctest --test-dir build --output-on-failure
open "build/MothDSPLab_artefacts/Release/Moth DSP Lab.app"
```

For a local JUCE checkout, add `-DJUCE_SOURCE_DIR=/absolute/path/to/JUCE` at configure time. Use the pinned 8.0.9 revision for reproducible builds. `-DBUILD_TESTING=OFF` excludes tests and the fixture plugin. Windows/Linux build portability is intended but unverified.

## Signed macOS installer

Run `./package.sh` to build the Release app, sign a staged copy with hardened runtime,
create a signed installer, submit it to Apple notarization, staple the accepted ticket
and verify Gatekeeper acceptance. Output: `dist/Moth-DSP-Lab-0.1.0.pkg` (version comes
from the app), with SHA-256 and notarization evidence. Failed verification stops the
script before replacing the final installer. `./package.sh --skip-build` packages an
already built Release app; it still performs every signing and notarization check.

Requires the Developer ID Application and Developer ID Installer identities in the
local Keychain and the working `moth-notary` notarytool Keychain profile, as in the
other Moth app packaging scripts. No credentials are stored in the repository.
Override `CODESIGN_IDENTITY`, `INSTALLER_IDENTITY` and `NOTARY_PROFILE` when needed;
`BUILD_DIR`, `DIST_DIR` and `JOBS` are also configurable. The build's configured
architecture is retained (a new build defaults to the current Mac). For a universal
installer, first configure a separate directory with
`cmake -S . -B build-universal -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64'`,
then run `BUILD_DIR="$PWD/build-universal" ./package.sh`.

The installer targets `/Applications/Moth DSP Lab.app`, with bundle relocation
disabled so an existing development copy is not overwritten. The running build is
never signed in place. Audio-input and third-party plugin loading entitlements are
included. No recordings, profiles, test fixtures, plugins or Codex CLI credentials
are bundled. Installed measurements default to `~/Documents/Moth DSP Lab`; existing
development measurements remain in the repository's ignored `.local-data/` folder
and can be opened with the project-folder selector. AI planning requires a separately
installed and authenticated Codex CLI. The script does not install or launch the app.

## Use

1. Select a project folder; completed measurements are stored under `sessions/`.
2. Choose a reference and candidate `.vst3` or installed macOS `.component` bundle. AU–AU and mixed AU–VST3 comparisons use the same pipeline. Each selection scans in an isolated process. If the bundle contains multiple plugin types, select one from the menu.
3. Open each **parameters** window, adjust the host-published parameters, then close the window to retain state. These windows also run in separate processes. A native generic editor is used for both formats.
4. Choose a mono/stereo WAV. Its sample rate populates the render setting. The file and requested sample rates must match; resample externally if needed.
5. Set block size, output channel layout and explicit tail duration. Mono inputs duplicate into stereo; stereo inputs average into mono. Sidechain buses are disabled where supported; AU auxiliary buses that must remain enabled receive silence. Unsupported layouts produce an error.
6. **Render A/B** sends the identical preserved WAV to both plugins. Inspect A/B waveform and output spectra, level difference, additional delay and residual RMS. **Residual / spectrum difference** switches the charts to the aligned residual and per-bin B−A difference. **Stop / cancel** stops playback or terminates an active worker.
7. **Play A/B** auditions the saved captures at a fixed -12 dB monitoring gain (0.25 amplitude). This monitoring gain does not alter saved captures or analysis. **Open session** restores settings, input and initial plugin states from `manifest.json`, allowing a rerun.

Parameters and render operations are serialized in this increment. Plugin-native GUI, bypass, chain editing, persistent plugin browser and dedicated preset import/export remain future work. A plugin may still depend on random state, external files or time; the host cannot promise bit-identical output for every third-party plugin.

AU support currently covers installed AUv2 components on macOS. Typical folders are `~/Library/Audio/Plug-Ins/Components` and `/Library/Audio/Plug-Ins/Components`. AU loading uses the macOS registered type/subtype/manufacturer identifier preserved in the plugin description; the selected path is provenance, not a separate way to choose between duplicate registrations of the same AU. Install/register the component before selecting it. AUv3 app-extension browsing and hosting are not included. VST3 remains available on other platforms.

## Scarlett / Palmer DACCAPO / B7K Ultra setup guide

Open **Hardver bekötés / teszt** in the main window. The Hungarian four-step guide covers preparation, the outgoing cable chain, the return connection and the final checks. It stops this window's audition playback and opens a modal setup panel; it does not start audio automatically.

- Scarlett rear line output → balanced 6.35 mm TRS-to-XLR male cable → DACCAPO balanced XLR input.
- DACCAPO TS output → short TS instrument cable → B7K Ultra input.
- B7K Ultra jack output → TS cable → Scarlett jack input (typically Input 2 on Solo 2nd/3rd generation).
- Disconnect speakers; disable Direct Monitor and DAW/software input monitoring. Start with low physical levels and the pedal in full bypass. The guide suggests LINE input mode initially and asks you to verify the actual physical channels.

The instructions are based on the [Palmer DACCAPO manual](https://www.palmer-germany.com/en/downloads/file/id/590923153) and [Focusrite reamping guidance](https://support.focusrite.com/hc/en-gb/articles/208364625-Using-a-Focusrite-interface-when-re-amping-guitars). The application cannot read the physical gain knobs, pedal bypass switch or Direct Monitor button.

Choose the same full-duplex audio interface for input/output, then explicitly choose one physical input and output. No default device is opened and no channel is preselected. On the final step, confirm wiring, disabled monitoring and bypass/low levels, then press **Bekötve — teszt indítása**. macOS requests audio-input permission before opening the device. No signal is sent if permission is denied.

The 2.6-second probe contains initial silence, two identical faded noise bursts capped at **-36 dBFS peak**, and trailing silence. The callback only emits the precomputed probe: it never routes incoming audio to the output. Recording buffers are preallocated; correlation and disk writes happen after capture, on a background thread. The interface is requested at 48 kHz / 256 samples and its actual settings are saved.

The result checks signal recognition, level/headroom, noise, two-pulse delay consistency and persistent trailing signal. Initial thresholds: positive correlation ≥0.7 for both pulses, return RMS ≥-65 dBFS, peak <-1 dBFS, baseline ≤-45 dBFS, SNR ≥20 dB and repeated delay difference ≤2 samples. The latency search covers 0–200 ms. These are conservative bypass setup heuristics, not pedal-model quality scores. Some valid but strongly filtered/noisy paths may need investigation rather than passing.

**VÉSZLEÁLLÍTÁS**, window closure, missing channels, non-finite input, input magnitude ≥0.95, initial baseline RMS above -30 dBFS, device errors and an 8-second watchdog stop/invalidate the test. Reported xruns invalidate the result when the driver supports the counter. There is no automatic output-level escalation.

Completed/aborted checks save `setup-check.json`, `probe.wav` and `capture.wav` under the selected project's `setup-checks/`, or the user application-data `Moth DSP Lab/setup-checks/` folder if no project is selected. Aborted captures have a valid prefix length in `capturedSamples`; the remaining buffer is zero. Close-during-capture cancels without waiting for report generation. **Riport mappája** reveals the last saved result.

A pass means a recognisable, stable bypass return was measured. It does **not** verify physical cable identity, absolute pedal input voltage, correct analog gain matching to Darkbass or electrical safety. Repeat after any routing/gain/knob change. The active-pedal/DarkBass workflow below uses this fresh bypass result for timing.

## Guided B7K / DarkBass comparison

The hardware panel now contains three measurement stages: **1. Bypass ellenőrzés**, **2. Aktív pedál / tiszta EQ**, and **3. Aktív pedál / drive**. A fresh passing bypass check in the current panel is required to unlock the active stages; archived successes are not silently reused.

For each active stage, the panel displays requested B7K settings and the proposed DarkBass preset. Click **DarkBass profil betöltése** to scan the locally installed `DarkBass.component` and prepare its state in isolated workers. No audio is emitted by this button. Changing stage requires preparing the corresponding plugin state again. The initial profile is tied to the inspected DarkBass 0.1.0 parameter schema; a missing/renamed parameter causes preparation to stop rather than silently substitute a setting.

Read the physical instructions, enter the actual pedal positions/deviations in the notes field, confirm that the interface/reamp settings are unchanged, and confirm the requested pedal state. **Beállítva — mérés indítása** then captures the same bounded 2.6-second / -36 dBFS probe and renders it through the prepared AU. The panel requests manual settings; it cannot move or read physical knobs. There is no automatic level escalation or automatic chaining of measurements.

Active capture is saved before plugin rendering. Completed sessions under `sessions/` contain the probe, raw hardware capture, stereo plugin captures, mono comparison WAVs, residual, analysis and manifest with physical settings, plugin parameters/state, and the embedded bypass result. The host removes the fresh bypass round-trip delay from the hardware capture and the plugin's reported latency from its render; it compares only the common retained duration. The candidate's left stereo channel is used, and additional active-signal correlation alignment is disabled. No gain normalization or quality score is applied. The analog reamp/ADC gain and pedal input voltage remain uncalibrated: these are exploratory comparisons, not proof of model accuracy. A short, low-level noise probe does not characterize high-drive nonlinear behaviour fully.

The selected waveform, spectrum and residual appear automatically beside the current task in the same window. Audition buttons are disabled for hardware-reference sessions to avoid playing a capture into the still-connected reamp chain. Interface/channel changes or capture/device failure invalidate the bypass gate. Raw capture survives a subsequent plugin-render failure.

The offline `--compare-hardware job.json` command can compare preserved captures without opening an audio device; see `tests/hardware_comparison.py` for the job structure and provenance checks.

## Measurement artifacts

A session is written to `.pending-<uuid>` and promoted to a unique UTC-stamped directory only after successful rendering and analysis. Failed/cancelled work retains a `failed` manifest for diagnosis; existing completed sessions remain untouched.

- `manifest.json`: version, UTC creation time, processing version, OS, render settings, plugin descriptions/versions, initial/final state and input SHA-256. Source commit is explicitly `null` until source-project integration exists.
- `stimulus.wav`: exact copied input bytes.
- `reference-raw.wav`, `candidate-raw.wav`: uncompensated float32 plugin captures, including reported leading latency and selected tail.
- `reference.wav`, `candidate.wav`: float32 captures with reported plugin latency removed, with equal stimulus-plus-tail duration.
- `residual.wav`: B minus A over the overlap after additional integer alignment, without gain normalization.
- `analysis_summary.json`: raw levels/peaks, clipping flags, alignment confidence, residual level and frequency-bin A/B/difference values.
- `report.md`, `recommendations.json`: local report and explicit disabled-AI result.
- `*-job.json`, `*-job-result.json`: original worker request/response audit records. Paths reflect the pre-promotion staging directory; rerun through the session manifest rather than replaying these audit files directly.

The level difference includes the selected tail. The spectrum is a 4096-point Hann-windowed averaged **output magnitude spectrum**, not an inferred frequency response. Alignment searches ±4096 samples on the first 65536 samples, requires |correlation| ≥ 0.5 and rejects boundary peaks; it does not resolve fractional delay, clock drift or periodic ambiguities. Negative correlation retains polarity inversion. Silence has uncertain alignment. A nonlinear residual is not a standalone quality score. No automatic EQ or gain matching is applied.

## Architecture and validation

`core`: domain/settings, WAV/JSON storage. `audio-host`: isolated VST3/AU scan/render/editor. `measurement`: session orchestration. `analysis`: offline alignment, level, FFT and residual. `ui`: native JUCE controls, plots and buffered audition. `ai-bridge`: provider interface with `NoAiAdvisor`. `optimization`: reserved for M4.

The application message thread owns UI; a background worker orchestrates child processes and offline analysis. No analysis, AI or plugin hosting runs on the device playback callback. File audition uses a buffered background reader. Scans have a 30-second timeout, render jobs a 180-second timeout, parameter editors a one-hour timeout; closing the app cancels outstanding children.

Tests cover synthetic identity/gain/EQ/clipping/delay/polarity/silence, invalid inputs, WAV/session persistence, and a built VST3 fixture exercised through the actual application subprocess protocol. The integration test verifies gain, reported latency removal, saved-state replay, bit-identical rerenders, export, block-size invariance and preservation of previous sessions on abrupt worker termination or load failure. The test plugin is never copied into system plugin directories. On macOS, an additional integration test uses Apple’s built-in AUHighPassFilter to check AU parameters, state replay and mixed AU/VST3 sessions.

A command-line comparison is also available:

```sh
"build/MothDSPLab_artefacts/Release/Moth DSP Lab.app/Contents/MacOS/Moth DSP Lab" --compare /absolute/path/to/job.json
```

The job has `project`, `input`, `settings`, `reference`, and `candidate`; target fields are `plugin` (absolute bundle path; may be empty for a registered AU identifier), `descriptionXml` (JUCE scan result), and `state` (standard base64). See `tests/integration.py` for an executable example. No shell/build commands or network actions are accepted by this protocol.

## Next increments

Complete broader M1 coverage with plugin chains, native editors/preset browsing, directory scan/cache, explicit bypass, FileTarget UI, repeatability comparisons and cross-platform CI. The first M2 increment provides the guided connection check; next are calibrated active-pedal measurement, saved physical control snapshots and direct comparison with Darkbass. AI adapters, optimization and source-aware refinement follow M3–M5.

Local verification results and remaining acceptance work: [validation record](docs/validation.md).

See [development specification](docs/moth-dsp-lab-spec.md) and [implementation plan](docs/implementation-plan.md).

## Guided hardware workflow and recovery

The default screen embeds the entire guided measurement flow in one window. The current task and its controls are on the left; capture progress, input peak and results are on the right. Optional offline two-plugin controls are under **Plugin A/B** in the same window. The wizard shows the next required action and leads through wiring, a live bypass check, clean/EQ capture, drive capture, and saved results. No audio starts merely by opening a window, restoring a workflow, preparing a plugin, or viewing results. A new capture requires the explicit start button and fresh operator confirmations. The pedal deviations/notes field is optional: when left blank, the physical-state confirmation records that the requested settings were followed exactly, together with those settings in the capture metadata.

Progress is saved atomically in `<project>/workflows/<id>.json`: per-stage notes, remembered route selections, completed result locations and the current operation. The last project is remembered locally. The workflow selector reopens earlier work; **Új mérés** preserves the old workflow and its captures. Existing completed hardware sessions are imported once, grouped by their bypass provenance. A committed measurement whose UI update was interrupted is recovered by its workflow ID.

On return to an incomplete series, saved results and notes remain available, but live bypass authorization and checkboxes are cleared. The user repeats the bypass check before the next missing physical measurement. An interrupted physical recording is repeated, not continued mid-buffer. If the physical capture completed but plugin rendering failed or was cancelled, **Mentett felvétel feldolgozása újra — hang nélkül** retries the preserved capture without emitting audio. Raw capture and recovery metadata live in `setup-checks/`; failed staging sessions remain available for diagnosis.

The completed-results selector distinguishes clean/EQ and drive. Selecting a completed capture immediately updates the inline graph and summary. **Mérési részletek** switches the feedback pane to the full report; **Eltérés mutatása** displays the residual. No additional measurement or result window opens. The result screen names the stage and local timestamp, shows level difference and correlation, and explains the uncalibrated comparison. Waveforms share an automatic display scale; saved audio is not normalized. Hardware audition remains disabled. This workflow does not add absolute analog calibration or a model-accuracy score.

### Named series and repeat measurements

Give the current series a name in the top-left field; edits save automatically and the name appears in history. After a series completes, **Ismétlés ezekkel a beállításokkal** starts a separate series with the remembered interface/channels and pedal notes. It preserves the source series, clears results and confirmations, and starts at fresh bypass verification. **Új mérés** starts empty from the wiring guide.

The visual system and interaction rules are documented in [docs/ux-design.md](docs/ux-design.md). The progress strip, next-action banner and primary-button styling distinguish where you are, what is needed next, and which action advances the task. Spectrum axes and waveform time labels are shown directly on the charts.


## Integrated AI measurement scenarios

Open **AI terv / eredmények értékelése** in the measurement window. Review/edit the B7K Ultra device profile and measurement goal, inspect the outbound JSON, enable sending, then generate a plan. The adapter uses the installed `~/.local/bin/codex` and its existing CLI authentication; no key is stored in the project. CLI access, authentication and usage limits belong to that installation/account. Planning is asynchronous, cancellable, and bounded to 360 seconds. A CLI error leaves the approved plan intact. No network call happens without the application's send control.

The planner receives the profile, goal, a shortened prior plan, physical-setting notes and scalar numerical analysis summaries. Spectrum arrays are excluded. The preview and transmitted request use the same UTF-8 byte-budgeted context; omitted historical measurements and shortened notes are explicitly marked. It does not receive WAVs or source code. The subprocess uses an isolated temporary working directory, read-only sandbox, ignored user configuration/rules, disabled shell and web search, ephemeral sessions and schema-constrained output. Results also pass independent application validation. The adapter uses an argument array, not shell interpolation. Model text cannot execute commands or change audio levels/routing.

Review the complete proposed steps before **Átnéztem — terv jóváhagyása**. Approval stores a new revision and archives the previous plan and result links. Each executable step has a stable ID, unambiguous preamp/distortion state, physical settings, rationale, expected observations and validated normalized DarkBass parameter overrides. Each step resets the plugin defaults before applying its overrides. Measurements require a fresh bypass check and human confirmation; approval alone never emits audio. The result selector lists individual scenario steps, including repeated steps in the same pedal mode. Crash recovery matches workflow, revision and step identity; old revisions cannot satisfy a new plan. Reopening requires another live bypass check.

Return to the planner after measurements to ask for interpretation and remaining informative steps. `no_change` can recommend ending the series with an explicit explanation and limitations; its approval is human review, not automatic proof that the scientific objective has been met. Offline legacy workflows still work without an AI provider.

**Current capability boundary:** the executable stimulus remains the guarded 2.6-second noise probe at at most −36 dBFS, with RMS, residual, correlation and clipping analysis. Arbitrary sequences of EQ/drive/blend settings are supported. Sweeps, stepped sine/THD, DI playback and calibrated analog drive levels are not yet executable; the planner must expose this limitation rather than label a noise capture as those tests. Physical knob-to-plugin mappings, including Master/Attack/Grunt equivalence, are not calibrated. Claude is not yet an implemented provider.

Validation includes schema rejection, repeated same-mode steps, revision isolation, commit-before-journal crash recovery, CLI transport with pipe-pressure/timeout/cancellation, and in-window plan approval without audio authorization. An opt-in live smoke test is available with `MOTH_TEST_LIVE_CODEX=1 build/lab_tests_artefacts/Release/lab_tests`; it sends a synthetic device profile only.

The active measurement instructions and plan preview now share the same scenario-step renderer. Each step distinguishes physical instructions, the unchanged guarded-noise stimulus, effective plugin parameter changes (including resets to defaults), and exact repeats. Textually different physical instructions are not automatically treated as equivalent; the comparison is against the previous planned step, not sensed hardware state. Capture metadata records `operatorInstructionsVersion: 2`. Older captures lack this marker and may have been made while the UI displayed the fixed baseline instead of scenario-specific settings; verify operator notes before treating them as the requested EQ/drive variation.


## Measurement profiles and storage

The planner includes **Terv mentése profilként**, a saved-profile selector, and **Profilok mappája**. A `.moth-profile.json` file contains a versioned portable device description, measurement goal and scenario, without captures, completion flags, routing authorization or credentials. Each save creates a separate file. Loading creates a draft for review and does not replace the approved scenario until explicit approval. To reuse a profile for a separate run, create a new measurement workflow and load the profile in its planner.

When running the app from its configured CMake build directory, the default project is `<source>/.local-data/Moth DSP Lab`; `.local-data/` is excluded from Git. A copy installed outside the build directory defaults to `~/Documents/Moth DSP Lab`. Projects contain `profiles/` (reusable plans), `workflows/` (resumable run journals), `sessions/` (results) and `setup-checks/` (captures and recovery jobs). The existing project-folder selector can override the location; hover it to see the full path. Profile files can be copied between project `profiles/` directories.

The development session previously stored under `~/Library/Moth DSP Lab` was copied to the repository data directory with internal JSON paths updated and WAV hashes verified. The original folder remains as a backup. This was a one-time migration of the current workspace, not automatic deletion/migration of arbitrary users' data on startup.


New AI plans must include a coverage table for all 13 pedal controls. Entries identify planned step IDs and physical states, or an explicit deferral reason; an unchanged switch mentioned in instructions is not evidence of coverage. Attack/Grunt hardware tests may keep the plugin fixed when no matching control is verified, and must not be described as calibrated model comparisons. Mid-frequency selector tests require non-flat gain in the corresponding EQ band. Legacy plans remain readable with a missing-coverage warning.

A separate `operator-review.json` with status `excluded` preserves a capture but rejects it as evidence for its requested physical settings. Excluded captures do not satisfy scenario completion and their numeric metrics are omitted from AI evidence; the exclusion reason remains visible to the planner and in result descriptions. A valid replacement capture wins over an excluded original during crash recovery. The previously labelled Bass-boost capture was annotated after the operator confirmed the Bass knob had not moved; the original audio and manifest remain intact.


## Codex handoff reports

Every new hardware comparison writes a detailed `report.md` alongside its WAVs, full plugin-state manifest and numerical/spectral JSON. Reports identify the measurement type, requested and operator-confirmed physical settings, stimulus, route/calibration, plugin parameters, analysis, exclusions and interpretation limits. Old captures are not upgraded into stronger evidence merely by generating a report.

**Codex átadócsomag** in the measurement window creates a local snapshot under `<project>/exports/`. It contains `README.md`, `workflow.json`, `index.json`, package-relative measurement/calibration folders, detailed reports, WAVs, metadata and SHA-256 checksums. Historical reference captures are included; excluded captures remain clearly marked for audit and must not be used to fit the claimed setting. The active future plan is distinguished from recorded results. No AI upload is performed. Review the package before sharing: manifests preserve original local paths and operator notes, and plugin binaries are not bundled. Export again after the nine supplemental captures; snapshots do not update themselves.


### Measurement storage

Hardware captures are grouped under `measurements/<profile title--plan fingerprint>/<run name--workflow id>/`:

- `results/`: committed comparisons, original WAVs, exact settings and `report.md`.
- `captures/`: raw acquisitions, bypass checks and recovery jobs, including failed attempts.
- `ATADAS/` at the project root: self-contained, immutable, clean consolidated ZIP handoffs; `LATEST.txt` names the latest ZIP. Older per-run `exports/` snapshots remain historical.
- `run.json` and `README.md`: plan/progress snapshot and a readable step index.

The “Mérési sorozat mappája” button opens the selected run. Profile fingerprints derive from the approved plan, so identical plans can share a profile folder while workflow IDs keep runs separate. The run path is pinned when first used; renaming a run does not move its recordings. Approving a replacement plan selects its profile group; prior revision references remain intact. `profiles/` holds reusable templates and `workflows/` remains the authoritative resume journal. Offline plugin comparisons without a hardware workflow retain `sessions/` compatibility. Legacy flat sessions remain readable.

Development data lives in `.local-data/Moth DSP Lab`; installed builds use `~/Documents/Moth DSP Lab`. Moving old flat data is explicit: close the application and run `python3 scripts/organize_measurements.py <project> --app <application-executable>` to preview, then add `--apply`. The migration creates a full sibling backup, rewrites live path references, preserves immutable exports, verifies every WAV SHA-256 and records the mapping in `storage-migration.json`. Unknown early captures go to `archive/unassigned-captures`, never to a guessed profile. On failure it restores the original project and retains the failed migration for inspection.


### Unified collections and front-panel initialization

Use **Sorozatok összevonása** to persist the other source runs belonging to the current measurement collection. The current run and its historical revisions are included automatically. Preview shows the combined numbered inventory. **Codex átadócsomag** produces one numbered folder and matching ZIP in `ATADAS/`; excluded results are omitted, source timestamps/revisions and explicit comparison references survive. Missing included artifacts fail export rather than silently disappearing. **Hibás — kihagyás az exportból** marks the selected capture invalid and makes its scenario step eligible for a new recording; historical source data remains recoverable, but the clean handoff contains no excluded audio.

**Új mérés → Eszköz / fotó / AI terv** starts generic initialization: pedal, amplifier head or other outboard. Add JPEG/PNG front-panel photos (including photos taken on a phone), authorize the visible text and images, run recognition, edit and confirm the control inventory, verify the proposed return path, then generate and approve a plan. Direct camera capture is not implemented; the native file picker imports photos. Each generic plan covers exactly the confirmed control inventory and contains a reviewed low-gain reference procedure. Photos are normalized to owned PNG copies without source EXIF metadata, stored locally under `assets/panels`, retained in profiles, and sent only on an explicit authorized request. Recognition and planning are separate operations; recognition never approves hardware settings.

Generic captures compare the hardware recording to its own fresh low-gain recorded baseline; they never instantiate DarkBass. Baseline and physical-route confirmation are required again after resuming. Amplifier-head routing requires a compatible rated load and the loadbox's documented line/DI return; a front-panel photo cannot establish load ratings or analog calibration. The existing low-level guarded-noise probe, clipping/noise/correlation/drift checks and emergency stop remain in force. This is exploratory capture, not THD, high-level nonlinear characterization or absolute voltage calibration. Legacy B7K/DarkBass workflows remain readable and keep their existing comparison engine.

Validation includes offline synthetic baseline/gain capture, dynamic control schema, mocked CLI image attachment transport, changed-photo hash rejection, saved profiles/initialization, safe UI approval gates, cycle-safe collection merging, excluded-audio omission and ZIP contents checked against SHA-256. Real photo recognition quality and the physical head/loadbox chain still require the operator's first session.
