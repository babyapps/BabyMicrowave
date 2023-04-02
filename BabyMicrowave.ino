/**
 * A normally-open micro switch, ideally a leaf switch, to be pressed (closed) when the microwave door is closed
 * and opened when the microwave door is opened.
 */
#define BUTTON_PIN PB1

/**
 * Optionally connect an LED to this pin.
 * If you just want the cooking sound and a bing without the light, comment this out to save space.
 */
#define LIGHT_PIN PB0
#define LIGHT_ANALOG_PIN A0

/**
 * Optionally connect a speaker to this pin.
 * If you just want a light without a speaker, comment this out to save space.
 * The tone() library used here uses a non trivial amount of bytes.
 */
#define SPEAKER_PIN PB4

/**
 * Optionally connect a potentiometer to this pin if you wish to have a timer.
 * If connected, it will turn the microwave on for a duration between MIN_ON_DURATION_MS and MAX_ON_DURATION_MS, proportional to where the potentiometer is set to.
 * If not, it will default to MIN_ON_DURATION_MS each time the microwave turns on.
 * If you want to leave your options open at compile time, leave this uncommented.
 * However, if you know you will never ever have such a pin, then comment it out to save space.
 */
#define TIMER_PIN PB2
#define TIMER_ANALOG_PIN A1

#define TONE_FREQ_COOKING 80
#define TONE_FREQ_BING 2500
#define BING_DURATION_MS 350
#define ON_DURATION_MIN_MS 2000
#define ON_DURATION_RANGE_SECS 18
#define DEBOUNCE_TIME 100

static unsigned long stateChangeTime = 0;
static byte state = 0;
static int onDuration = ON_DURATION_MIN_MS;

// All these macros which stuff bits into the `state` variable are because
// the there booleans required and associated local variables ended up being
// quite costly for our little ATTiny13a with 1k of storage.
#define IS_CLOSED() ((state & 1) == 1)
#define SET_CLOSED() state = state | 1
#define IS_OPEN() ((~state & 1) == 1)
#define SET_OPEN() state = state & ~1

#define IS_ON() ((state & 2) == 2)
#define SET_ON() state = state | 2
#define IS_OFF() ((~state & 2) == 2)
#define SET_OFF() state = state & ~2

#define WAS_CLOSED() ((state & 4) == 4)
#define SET_WAS_CLOSED() (state = state | 4)
#define WAS_OPEN() ((~state & 4) == 4)
#define SET_WAS_OPEN() (state = state & ~4)

#define IS_BINGING() ((state & 8) == 8)
#define SET_BINGING() state = state | 8
#define IS_FINISHED_BINGING() ((~state & 8) == 8)
#define SET_FINISHED_BINGING() state = state & ~8

#define REMEMBER_IF_CLOSED() IS_CLOSED() ? SET_WAS_CLOSED() : SET_WAS_OPEN()

void setup() {
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  #ifdef TIMER_PIN
  pinMode(TIMER_PIN, INPUT_PULLUP);
  #endif

  #ifdef SPEAKER_PIN
  pinMode(SPEAKER_PIN, OUTPUT);
  #endif
  
  // Set it up as if the door is closed and the microwave is off (no sound, no speaker, appropriate flags set).
  // If the door is in fact open, then the worst case is it will wait DEBOUNCE_TIME before changing to the open state.
  digitalWrite(LIGHT_PIN, LOW);
  stateChangeTime = millis();
  SET_CLOSED();
  SET_OFF();
  SET_FINISHED_BINGING();
}

void loop() {
  if (millis() - stateChangeTime < DEBOUNCE_TIME) {
    return;
  }
  
  REMEMBER_IF_CLOSED();
  if (digitalRead(BUTTON_PIN) == LOW) {
    SET_CLOSED();
  } else {
    SET_OPEN();
  }

  // If closed, need to check for either:
  //  - The door opening, which will cause any tone to stop, but the light to stay on.
  //  - The timer running out, which will turn off both the light and the the tone.
  if (WAS_CLOSED()) {
    
    int timeSinceClose = millis() - stateChangeTime;

    // Door switched to open, stop all tones but leave the light on.
    if (IS_OPEN()) {
    
      SET_OPEN();
      SET_OFF();
      SET_FINISHED_BINGING();
      
      digitalWrite(LIGHT_PIN, HIGH);
      
      #ifdef SPEAKER_PIN
      noTone(SPEAKER_PIN);

      // Also force the pin low to address https://github.com/MCUdude/MicroCore/issues/146.
      digitalWrite(SPEAKER_PIN, LOW);
      #endif
      
    // Timer ran out, turn the light off and stop the tone.
    } else {
      if (IS_ON() && timeSinceClose > onDuration) {
      
        SET_OFF();
        SET_BINGING();
        
        digitalWrite(LIGHT_PIN, LOW);
        
        #ifdef SPEAKER_PIN
        tone(SPEAKER_PIN, TONE_FREQ_BING, BING_DURATION_MS);
        #endif

      } else if (IS_OFF() && IS_BINGING() && timeSinceClose > onDuration + BING_DURATION_MS) {
        SET_FINISHED_BINGING();

        // No need to explicitly stop tone, it was only queued for a certain period of time
        // in the original call to tone(...)
      }
    }

  // Door closed, need to start the timer, play the sound, and turn the light on.
  // Will also record the time this happened so we can time out when complete.
  } else if (WAS_OPEN() && IS_CLOSED()) {

    SET_CLOSED();
    SET_ON();

    /**
     * Read the value from potentiometer connected to TIMER_PIN/TIMER_ANALOG_PIN.
     * Fully off will result in ON_DURATION_MIN_MS time.
     * Fully on will result in (approx) ON_DURATION_RANGE_SECS. It is only approx
     * because for simplicity, it is much easier to multiply the analog reading of
     * between 0 and 1023 by ON_DURATION_RANGE_SECS to get the milliseconds.
     * This means there is an extra 23ms per second, but that is negligible for
     * a baby microwave.
     * 
     * If no potentiometer is connected, then the connection is pulled high due
     * to the internal pullup resistor, so the "reading" below will be zero, and
     * we will turn on for ON_DURATION_MIN_MS millis.
     */
    #ifdef TIMER_PIN
    unsigned int reading = 1024 - analogRead(TIMER_ANALOG_PIN);
    onDuration = ON_DURATION_MIN_MS + reading * ON_DURATION_RANGE_SECS;
    #endif
    
    stateChangeTime = millis();
    digitalWrite(LIGHT_PIN, HIGH);
  
    #ifdef SPEAKER_PIN
    tone(SPEAKER_PIN, TONE_FREQ_COOKING, onDuration);
    #endif
  }

}
