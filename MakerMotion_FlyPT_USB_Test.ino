/*
 * MakerMotion: ESP32-C3 / FlyPT USB reception test, version 1.
 * Keep all motor drivers, sensors and external power disconnected.
 * Board: ESP32C3 Dev Module; USB CDC On Boot: Enabled.
 * Wire protocol: ASCII [M,A,B], with integer A and B in 0..1023.
 * Examples: [M,512,512]  [M,400,600]
 * This is a custom test protocol, not SMC3 motor-control firmware.
 *
 * Send a stream from FlyPT, stop/disconnect its serial output, then WAIT
 * THREE SECONDS with USB power connected. The receiver saves one summary
 * after 1.5 seconds of silence. Open Arduino Serial Monitor at 115200 to
 * inspect the summary, even if opening the monitor resets the chip.
 * One small NVS write per completed capture; no writes per received frame.
 * No GPIO, PWM, sensor, Wi-Fi or motor-control functions are used.
 */
#include <stddef.h>
#include <stdint.h>

namespace MakerMotion {
constexpr uint32_t MAGIC = 0x4D4D0101;
constexpr uint32_t IDLE_MS = 1500;

struct Summary {
  uint32_t magic = MAGIC;
  uint32_t run = 0;
  uint32_t bytes = 0;
  uint32_t valid = 0;
  uint32_t rejected = 0;
  uint16_t lastA = 0, lastB = 0;
  uint16_t minA = 1023, maxA = 0;
  uint16_t minB = 1023, maxB = 0;
};

class FrameParser {
 public:
  enum Event { NONE, ACCEPTED, REJECTED };

  void reset() { inside_ = false; used_ = 0; }
  bool partial() const { return inside_; }

  Event push(uint8_t ch, uint16_t &a, uint16_t &b) {
    if (ch == '[') {
      const bool interrupted = inside_;
      inside_ = true;
      used_ = 0;
      return interrupted ? REJECTED : NONE;
    }
    if (!inside_) return NONE;  // Ignore CR/LF and unframed noise.
    if (ch == ']') {
      body_[used_] = '\0';
      const bool ok = decode(body_, a, b);
      reset();
      return ok ? ACCEPTED : REJECTED;
    }
    if (ch < 32 || ch > 126 || used_ >= sizeof(body_) - 1) {
      reset();
      return REJECTED;
    }
    body_[used_++] = static_cast<char>(ch);
    return NONE;
  }

 private:
  bool inside_ = false;
  size_t used_ = 0;
  char body_[32] = {};

  static bool number(const char *&p, uint16_t &out) {
    if (*p < '0' || *p > '9') return false;
    uint32_t value = 0;
    while (*p >= '0' && *p <= '9') {
      value = value * 10 + static_cast<uint32_t>(*p++ - '0');
      if (value > 1023) return false;
    }
    out = static_cast<uint16_t>(value);
    return true;
  }

  static bool decode(const char *p, uint16_t &a, uint16_t &b) {
    uint16_t candidateA = 0, candidateB = 0;
    if (*p++ != 'M' || *p++ != ',') return false;
    if (!number(p, candidateA) || *p++ != ',') return false;
    if (!number(p, candidateB) || *p != '\0') return false;
    a = candidateA;
    b = candidateB;
    return true;
  }
};

class Capture {
 public:
  Summary last;
  Summary current;
  bool active = false;

  void receive(uint8_t ch, uint32_t now) {
    // The Arduino loop closes the preceding capture after an idle interval.
    if (!active) {
      current = Summary{};
      current.run = last.run + 1;
      parser_.reset();
      active = true;
    }
    lastByteMs_ = now;
    ++current.bytes;
    uint16_t a = 0, b = 0;
    const FrameParser::Event event = parser_.push(ch, a, b);
    if (event == FrameParser::REJECTED) ++current.rejected;
    if (event != FrameParser::ACCEPTED) return;
    ++current.valid;
    current.lastA = a;
    current.lastB = b;
    if (a < current.minA) current.minA = a;
    if (a > current.maxA) current.maxA = a;
    if (b < current.minB) current.minB = b;
    if (b > current.maxB) current.maxB = b;
  }

  bool finishIfIdle(uint32_t now) {
    if (!active || static_cast<uint32_t>(now - lastByteMs_) < IDLE_MS)
      return false;
    if (parser_.partial()) ++current.rejected;
    parser_.reset();
    last = current;
    active = false;
    return true;
  }

 private:
  FrameParser parser_;
  uint32_t lastByteMs_ = 0;
};
}  // namespace MakerMotion

#ifndef MAKERMOTION_HOST_TEST
#include <Arduino.h>
#include <Preferences.h>

Preferences testStorage;
MakerMotion::Capture capture;
bool storageReady = false;
bool savedOK = true;
uint32_t lastReportMs = 0;

// Explicit prototypes also keep Arduino's sketch preprocessor unambiguous.
void printSummary();
void setup();
void loop();

void printSummary() {
  if (!Serial) return;
  if (!storageReady) Serial.println("WARNING: NVS storage unavailable.");
  if (!savedOK) Serial.println("WARNING: Last summary was not saved to flash.");
  const MakerMotion::Summary &s = capture.last;
  if (s.run == 0) {
    Serial.println("READY: send [M,512,512]. No saved capture yet.");
    return;
  }
  Serial.printf("SAVED run=%lu bytes=%lu valid=%lu rejected=%lu\n",
                static_cast<unsigned long>(s.run),
                static_cast<unsigned long>(s.bytes),
                static_cast<unsigned long>(s.valid),
                static_cast<unsigned long>(s.rejected));
  if (s.valid == 0) {
    Serial.println("NO VALID FRAMES: check ASCII format and [M,A,B] syntax.");
    return;
  }
  Serial.printf("A last=%u min=%u max=%u changed=%s\n",
                static_cast<unsigned>(s.lastA), static_cast<unsigned>(s.minA),
                static_cast<unsigned>(s.maxA), s.minA != s.maxA ? "YES" : "NO");
  Serial.printf("B last=%u min=%u max=%u changed=%s\n",
                static_cast<unsigned>(s.lastB), static_cast<unsigned>(s.minB),
                static_cast<unsigned>(s.maxB), s.minB != s.maxB ? "YES" : "NO");
}

void setup() {
  Serial.begin(115200);
  // USB logging must not block reception if FlyPT does not read replies.
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  storageReady = testStorage.begin("mm-usb-test", false);
  if (storageReady && testStorage.isKey("summary") &&
      testStorage.getBytesLength("summary") == sizeof(MakerMotion::Summary)) {
    MakerMotion::Summary stored;
    if (testStorage.getBytes("summary", &stored, sizeof(stored)) ==
            sizeof(stored) && stored.magic == MakerMotion::MAGIC) {
      capture.last = stored;
    }
  }
  // Do not wait for Serial: FlyPT may open its port with different DTR state.
}

void loop() {
  // Bound each batch so timeout handling and the scheduler can run.
  for (size_t count = 0; count < 256 && Serial.available() > 0; ++count) {
    const int ch = Serial.read();
    if (ch >= 0) capture.receive(static_cast<uint8_t>(ch), millis());
  }
  if (capture.finishIfIdle(millis())) {
    savedOK = storageReady &&
        testStorage.putBytes("summary", &capture.last, sizeof(capture.last)) ==
            sizeof(capture.last);
    lastReportMs = millis() - 2000;  // Report immediately if a monitor is open.
  }
  if (!capture.active && static_cast<uint32_t>(millis() - lastReportMs) >= 2000) {
    printSummary();
    lastReportMs = millis();
  }
  delay(1);
}
#endif
