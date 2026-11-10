# MakerMotion: FlyPT to ESP32-C3 USB test

This test verifies USB reception of two changing 10-bit target values. Keep the
motor drivers, sensors and external power supplies disconnected throughout.
The sketch uses no GPIO, PWM, sensor or Wi-Fi functions. It is a communication
test and does not provide position control or implement the SMC3 command set.

## 1. Upload

Extract the ZIP. Open MakerMotion_FlyPT_USB_Test.ino from the extracted folder
named MakerMotion_FlyPT_USB_Test. Keep that folder name unchanged.

Use Arduino IDE with the Espressif ESP32 board package installed:

| Setting | Value |
|---|---|
| Board | ESP32C3 Dev Module, or the exact board's C3 profile |
| USB CDC On Boot | Enabled (the user's C3 uses native USB-Serial/JTAG) |
| Port | The C3's current COM port; previously COM7 |
| Erase All Flash Before Sketch Upload | Disabled, to retain test summaries |
| Serial Monitor | 115200 baud |

Upload and press RESET/RST once with BOOT released. Open Serial Monitor.
Every two seconds, a new receiver prints READY, or an earlier saved capture.
No additional Arduino library installation is required: Preferences ships
with the Arduino-ESP32 core. A native ESP32-C3 hardware compile/upload has
not been performed here; compile and upload in Arduino IDE to check your
installed board package and device.

## 2. Check the receiver manually

In Serial Monitor, send this exact text:

    [M,400,600]

Either no line ending or newline is fine. Wait three seconds. The next report
should include valid=1, rejected=0, A last=400, B last=600. The run number may
be greater than one if an earlier test was saved. Identical minimum/maximum
values and changed=NO are expected when only one frame was sent.

The frame format is ASCII [M,A,B]. Both values must be integers from 0 to
1023. The bracket characters and commas are literal. No spaces, fractional
values, extra fields or quotation marks belong inside a frame. Invalid
framed messages are rejected. Unframed bytes count as received bytes but
do not become valid messages.

## 3. Verify FlyPT's serial output with a constant

Close Serial Monitor completely. FlyPT and Arduino Serial Monitor cannot
hold the same COM port at the same time. Arduino IDE may remain open.

Add a Serial Output module in FlyPT. Use:

| Setting | Value |
|---|---|
| Port | The C3's current COM port |
| Format | ASCII (Mover 3.7) / Decimal (Mover 3.5) |
| Baud | 115200 |
| Data bits / parity / stop bits | 8 / none / 1 |
| Output interval | Approximately 50 ms, or 20 messages per second |
| Output string | [M,400,600] |

Do not choose Binary for this sketch. If timing is configured in interval
loops rather than milliseconds, multiply the number of interval loops by
the main-loop period: 25 loops with a 2 ms period gives 50 ms.

Enable/connect this serial output for at least five seconds. Stop/disable
the output and DISCONNECT it from the COM port. Keep the C3 plugged in for
three more seconds. Then open Arduino Serial Monitor at 115200 baud.

Look for a NEW run number, many valid frames, rejected=0 and values 400/600.
An old manual-test result with valid=1 is not proof of FlyPT reception.
The exact number of frames depends on the interval and test duration.

## 4. Generate the slow wave

FlyPT menu names vary with version. The steps below use Mover 3.7 terms;
the corresponding source in Mover 3.5 is also called Loop. Keep the serial
output disconnected while editing.

1. Add a Loop source under the calculated/generated sources.
2. Choose a Sine wave.
3. Set Period to 10 seconds, Amplitude to 5, and vertical shift/offset to 0.
   If your period field explicitly uses milliseconds, enter 10000 instead.
4. Select Pitch position as the generated source value. Disable other
   sources for this test so their values do not combine with this one.
5. Enable/connect the Loop source and check its graph. It should move
   smoothly between approximately -5 and +5 once every ten seconds.
6. Add a Direct actuator module and select this Loop source and its
   Pitch position value as the input. This bypasses platform geometry for
   the USB test; no physical rig dimensions are required.
7. Set the direct actuator's bit output to 10 bits. Give it a range that
   contains -5..+5; for controls with separate minimum and maximum, -20
   and +20 are a suitable test range. Keep gains at their defaults initially.
8. Check that this direct's integer/bit output changes within 0..1023.
   If it remains at one value, inspect source selection, connect/enable
   switches and range/scale. If necessary, use its manual slider first.

Both test channels will deliberately use this ONE direct actuator value.
This keeps the first wave test simple. Future two-motor firmware will use
two independent actuator values.

## 5. Send the changing value

For a Mover 3.7 direct whose module key is D0, use ASCII and this exact string:

    [M,<D0.B>,<D0.B>]

B means the direct actuator's integer bit value. The module name shown in
the UI and the module key may differ. Use the actual bit-value key from
your output variable list; it may be D1.B or another key if other directs
already exist. Drag the same actual bit-value key into both fields.

For Mover 3.5, use the actual key displayed for the direct actuator instead
of D0.B, in both fields. For example, if its actuator key is Test:

    [M,<Test>,<Test>]

Test is an example key, not a universal default. Do not add extra angle
brackets to request decimal places. This receiver accepts integer values.

The generated output preview must contain ordinary numeric text such as:

    [M,450,450]
    [M,487,487]
    [M,512,512]

The numbers shown here illustrate the shape of the messages, not exact
values guaranteed by your direct actuator's range and scaling. Literal
angle-bracket keys, empty fields, values above 1023 or decimal fractions
mean the output setup needs correction before continuing.

Close Serial Monitor. Connect/enable FlyPT's serial output and run the
wave for 20 seconds (two cycles). Stop AND disconnect that serial output.
WAIT THREE SECONDS with the C3 still USB-powered. Open Serial Monitor.

Success means a NEW saved run with:

- valid greater than zero, normally hundreds at a 50 ms interval;
- rejected=0;
- A min lower than A max and changed=YES;
- B min lower than B max and changed=YES;
- both channels showing the same range because the same test wave was sent.

Counts and value ranges will vary. Run is a capture/session number; it
increases after a new stream follows an idle interval. Stopping only the
Loop source may leave the serial output sending neutral values, so stop
and disconnect the serial output itself to trigger summary saving.

## Reading the saved result

The C3 collects byte counts, valid/rejected frame counts, last values and
the minimum/maximum received values. After 1.5 seconds without received
bytes it writes one small summary to Preferences/NVS. Opening Serial
Monitor after that cannot destroy the saved result even if it resets
the C3. The summary is printed every two seconds while no stream is active.

Wait for saving before unplugging, resetting, opening another serial client,
or uploading another sketch. Avoid continuous start/stop testing at short
intervals: this prototype writes flash once per completed capture.

| Observation | What to check |
|---|---|
| No new run | Correct COM port, FlyPT connected and sending repeatedly, three-second wait after disconnect |
| bytes increases but valid=0 | ASCII/Decimal mode and exact [M,A,B] message syntax |
| rejected increases | Missing/wrong keys, fractional/out-of-range values, malformed data |
| valid increases but changed=NO | Source/direct paused, constant output, manual slider not moved, only one sample |
| NVS warning | Summary was not saved; inspect core/flash configuration before relying on reset recovery |
| Port busy | Close Serial Monitor before FlyPT, and disconnect FlyPT before Serial Monitor |
| Only ROM banner | USB CDC On Boot Enabled, uploaded receiver sketch, correct port, reset with BOOT released |

## Validation and scope

The actual parser/capture code passed local C++ tests with AddressSanitizer
and UndefinedBehaviorSanitizer (leak checking disabled because this host
restricts /proc access). Cases included fragmented and concatenated frames,
range boundaries, malformed and oversized inputs, resynchronization,
unchanged targets after rejected packets, saved-summary statistics, idle
timeouts, clock wraparound, and a 2048-frame stream. Hardware upload,
USB driver behavior and NVS writes require verification on your C3.

Passing this test verifies communication only. Position sensors, closed-loop
control, travel limits, motor-driver outputs and stopping behavior remain
separate development steps before the platform moves.

Official/reference sources:

- https://www.flyptmover.com/mover-3-5/sources/calculated
- https://www.flyptmover.com/mover-3-5/outputs/the-decimal-output-string
- FlyPT author's 3.7 key explanation: https://www.xsimulator.net/community/threads/flypt-mover-3-7-0-23.19016/page-29
- https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html
- https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/cdc_dfu_flash.html
