# Measurement UX

The primary job is to complete and later repeat a hardware comparison without having to infer the application's state. The default view stays in one window and keeps the current operator task beside feedback. Advanced plugin-only work is a separate optional view.

## Information hierarchy

1. Named measurement series and history: identify the work before editing it.
2. Five-stage progress strip: wiring, bypass, clean/EQ, drive, results. The current stage has both a label and underline; completed stages have checkmarks.
3. Next-action banner: name the actual missing selection or confirmation. A disabled Start button must have an explanation here.
4. Task card: only relevant setup controls, short physical-setting instructions, persistent notes and one enabled primary action.
5. Feedback card: recording/processing status, input peak during capture, then automatic graphs and a concise result summary. Detailed provenance and numerical reports are optional.

Mint-filled buttons identify the primary action. Secondary actions use subdued surfaces. Stop uses a distinct red treatment. Idle, processing, complete and error states retain explicit text; colour is supplementary. Read-only guidance has no editable-field border. Editable names/notes retain visible boundaries and focus indicators. Tooltips explain new-versus-repeat actions; tab order follows the task sequence.

## Reuse and recovery

- A series name is optional and saved as it is edited. History retains the date and completion state alongside the name.
- New measurement creates an empty series with the wiring guide.
- Repeat with these settings creates a separate series, preserving route selections and clean/drive notes. It clears completed results and returns to bypass verification with all physical confirmations unchecked.
- Resuming a partially complete series preserves notes and finished captures, but never restores permission to emit audio.
- Result selection changes the inline summary and plots. Showing another result does not change the measurement settings or play audio.
- Old series and raw captures are preserved. A failed plugin render can be retried from the saved capture without another physical recording.

## Result interpretation

A plot names the reference/plugin colours, labels time in seconds and spectrum frequency/level axes, and uses a common waveform display scale. Audio samples are not normalized. The result summary states the stage, timestamp, level difference and correlation; uncertain correlation and the absence of absolute analog calibration remain visible.

## Validation

Automated checks cover named-state restoration, repeating without carrying over results/authorization, source preservation, interrupted work, inline result selection and layout bounds at the minimum supported window size. Native screenshot inspection covers the actual saved series; synthetic screenshots cover new, resumed and active-setup states. These checks are not a substitute for a supervised operator usability session or a new physical capture.
