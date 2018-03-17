const int PO_READ_PIN = A0;         // pin to read bpm from, PO sync input
const int BPM_READ_PIN = A3;        // pin to read from, analog pot
const int CLICK_HI_DURATION = 6;    //ms necessary for a hi value to click
const int MIN_TRIPLET_BPM = 90;     // the bottom threshold for non-doubled triplets
const int TRIPLET_INDEX = 3;        // index with arrays that signifies the triplet

int bpm = 120;

// BPM Ouput [2x, 1x, 1/2x, triplet]
unsigned long outputPin[] = {A0, A1, A2, A7};
unsigned long lastTrig[] = {0, 0, 0, 0};
bool isLow[] = {0, 0, 0, 0};
int pulseDuration[] = {0,0,0,0};

// Triplet specifics
int tripletCount = 0;
int tripletPulseRemainder = 0;

// Read mode
bool readMode = false;
unsigned long lastReadTrig = 0;
bool isReadLow = true;

void setup() {
  // PO sync algorithm: high for 4-6ms, 2 clicks ber beat
  // seems like it cannot go less than 60bpm without malfunctioning
  Serial.begin(57600);
  setBPM(bpm);
}

void loop() {
  unsigned long curTime = millis();
  
  if (readMode) {
    readInputBPM(curTime);
  } else {
    readControlBpm();
    sendBPM(curTime, 0); // 2x
    sendBPM(curTime, 1); // 1x
    sendBPM(curTime, 2); // 1/2x
    sendBPM(curTime, 3); // triplet
  }
}

/**
 * set a new bpm value
 */
void setBPM(int newBpm) {
  bpm = newBpm;
  int standardPulseDurarion = getBPMOrPulseLength(bpm);

  // 2x;
  pulseDuration[0] = standardPulseDurarion / 2;
  
  // 1x;
  pulseDuration[1] = standardPulseDurarion;

  // 1/2x;
  if (bpm >= 120) {
    pulseDuration[2] = standardPulseDurarion * 2;
  } else {
    // cannot handle bpm lower than 60
    pulseDuration[2] = standardPulseDurarion; // 1x
  }

  // triplet;
  pulseDuration[3] = getTripletPulseLength(bpm); 
  tripletPulseRemainder = getTripletPulseRemainder(bpm);
}

/**
 * read analog pot and translate to new bpm
 * 60bpm - 240bpm
 */
void readControlBpm() {
  int potValue = analogRead(BPM_READ_PIN);

  int newBpm = 60 + float(potValue/(1024/180));
  if (newBpm != bpm) {
    setBPM(newBpm);
  }
}

/**
 * Send bpm triggers
 */
void sendBPM(unsigned long curTime, int bpmIndex) {
  if (isLow[bpmIndex]) {
      // see if we need to go high
      int clickDuration = curTime - lastTrig[bpmIndex];
      int pulseRemainder = ((bpmIndex == TRIPLET_INDEX && tripletCount % 3 == 2) ? tripletPulseRemainder : 0);
      
      if (clickDuration >= pulseDuration[bpmIndex] + pulseRemainder) {
        //go high
        isLow[bpmIndex] = false;
        lastTrig[bpmIndex] = curTime;

        if (bpmIndex == TRIPLET_INDEX) {
          tripletCount++;
        }
      }
  } else {
    // go low if we need to stop clicking
    isLow[bpmIndex] = (curTime >= lastTrig[bpmIndex] + CLICK_HI_DURATION);
  }
  
  analogWrite(outputPin[bpmIndex], isLow[bpmIndex] ? 0 : 255);
}

/**
 * read PO sync bpm
 */
void readInputBPM(unsigned long curTime) {
  int sensorValue = analogRead(PO_READ_PIN);

  if (isReadLow) {
    if (sensorValue != 0) {
      // went high
      int clickDuration = curTime - lastReadTrig;
      bpm = getBPMOrPulseLength(clickDuration);
      
      isReadLow = false;
      lastReadTrig = curTime;
    }
  } else if (sensorValue == 0) {
    // went low
    isReadLow = true;
  }
}

/**
 * Get pulse duration for 1x speed (2 clicks per beat)
 */
int getBPMOrPulseLength(int fromValue) {
  return ceil(float(30000) / fromValue);
}

/**
 * Get pulse duration for triplet click
 * - double triplet speed if less than min bpm
 */
int getTripletPulseLength(int fromValue) {
  if (bpm < MIN_TRIPLET_BPM) {
    // we need to double the spped, otherwise its too slow
    return (getBPMOrPulseLength(fromValue) * 8) / 12;
  } else {
    return (getBPMOrPulseLength(fromValue) * 8) / 6;
  }
}

/**
 * 3 note length needs to add up exactly to 1x pulse length
 * we may need to add some time to one of the clicks to match up
 */
int getTripletPulseRemainder(int fromValue) {
  return (getBPMOrPulseLength(fromValue) * 4) % ((bpm < MIN_TRIPLET_BPM) ? 6 : 3);
}

