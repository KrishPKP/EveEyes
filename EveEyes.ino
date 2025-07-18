//2025-07-17
//Added both eyes on 2 screens (one is commented out currently)

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "eye_bitmap.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128

TwoWire wireLeft = TwoWire(0);
TwoWire wireRight = TwoWire(1);

Adafruit_SH1107 displayLeft(SCREEN_WIDTH, SCREEN_HEIGHT, &wireLeft, -1);
Adafruit_SH1107 displayRight(SCREEN_WIDTH, SCREEN_HEIGHT, &wireRight, -1);

enum EyeMode {
  EYE_NEUTRAL,
  EYE_HAPPY,
  EYE_SAD
};

struct EyeState {
  EyeMode mode = EYE_NEUTRAL;
  bool animating = false;
  unsigned long startTime = 0;
};

EyeState eyeLeft;
EyeState eyeRight;

const int smileRiseHeight     = 20;
const int smileRiseDuration   = 1000;
const int smileHoldDuration   = 2000;
const int smileFallDuration   = 400;
const int totalSmileTime = smileRiseDuration + smileHoldDuration + smileFallDuration;

const int sadCoverMaxOffset   = 20;
const int sadRiseDuration     = 1000;
const int sadHoldDuration     = 2000;
const int sadFallDuration     = 400;
const int totalSadTime = sadRiseDuration + sadHoldDuration + sadFallDuration;

int blinkStageLeft = 0;
int blinkStageRight = 0;
unsigned long lastBlinkTimeLeft = 0;
unsigned long lastBlinkTimeRight = 0;
unsigned long blinkIntervalLeft = 4000;
unsigned long blinkIntervalRight = 4000;
const int blinkStepDelay = 60;


void drawTiltedSmile(Adafruit_SH1107 &display, int cx, int cy, int rx, int ry, int offsetY, float tilt) {
  for (int y = -ry; y <= 0; y++) {
    float fy = y / float(ry);
    float fx = sqrt(1.0 - fy * fy);
    int halfWidth = int(rx * fx);
    int shift = int(y * tilt);
    int x0 = cx - halfWidth + shift;
    int x1 = cx + halfWidth + shift;
    int drawY = cy + y - offsetY;
    display.drawFastHLine(x0, drawY, x1 - x0, SH110X_BLACK);
  }
  display.fillRect(0, cy - offsetY + 1, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_BLACK);
}

void drawTiltedFrown(Adafruit_SH1107 &display, int cx, int cy, int rx, int ry, int offsetY, float tilt) {
  for (float y = 0; y <= ry; y += 0.5) {
    float fy = y / float(ry);
    float fx = sqrt(1.0 - fy * fy);
    int halfWidth = int(rx * fx);

    int shift = int(y * tilt);
    int x0 = cx - halfWidth + shift;
    int x1 = cx + halfWidth + shift;
    int drawY = cy + y + offsetY;

    display.drawFastHLine(x0, drawY, x1 - x0, SH110X_BLACK);
  }
  display.fillRect(0, 0, SCREEN_WIDTH, cy + offsetY, SH110X_BLACK);
}

void drawSadRight(Adafruit_SH1107 &display, unsigned long elapsed) {
  int offsetY = 0;
  if (elapsed < sadRiseDuration) {
    offsetY = map(elapsed, 0, sadRiseDuration, 0, sadCoverMaxOffset);
  } else if (elapsed < sadRiseDuration + sadHoldDuration) {
    offsetY = sadCoverMaxOffset;
  } else if (elapsed < totalSadTime) {
    int fallElapsed = elapsed - sadRiseDuration - sadHoldDuration;
    offsetY = map(fallElapsed, 0, sadFallDuration, sadCoverMaxOffset, 0);
  }
  drawTiltedFrown(display, 130, 20, 75, 20, offsetY, 0.5);
}

void drawHappyRight(Adafruit_SH1107 &display, unsigned long elapsed) {
  int smileOffset = 0;
  if (elapsed < smileRiseDuration) {
    smileOffset = map(elapsed, 0, smileRiseDuration, 0, smileRiseHeight);
  } else if (elapsed < smileRiseDuration + smileHoldDuration) {
    smileOffset = smileRiseHeight;
  } else if (elapsed < totalSmileTime) {
    int fallElapsed = elapsed - smileRiseDuration - smileHoldDuration;
    smileOffset = map(fallElapsed, 0, smileFallDuration, smileRiseHeight, 0);
  }
  drawTiltedSmile(display, 84, 105, 55, 20, smileOffset, -0.5);
}


void drawSadLeft(Adafruit_SH1107 &display, unsigned long elapsed) {
  int offsetY = 0;
  if (elapsed < sadRiseDuration) {
    offsetY = map(elapsed, 0, sadRiseDuration, 0, sadCoverMaxOffset);
  } else if (elapsed < sadRiseDuration + sadHoldDuration) {
    offsetY = sadCoverMaxOffset;
  } else if (elapsed < totalSadTime) {
    int fallElapsed = elapsed - sadRiseDuration - sadHoldDuration;
    offsetY = map(fallElapsed, 0, sadFallDuration, sadCoverMaxOffset, 0);
  }

  int cx = 126;
  drawTiltedFrown(display, cx, 20, 75, 20, offsetY, -0.5);  
}

void drawHappyLeft(Adafruit_SH1107 &display, unsigned long elapsed) {
  int smileOffset = 0;
  if (elapsed < smileRiseDuration) {
    smileOffset = map(elapsed, 0, smileRiseDuration, 0, smileRiseHeight);
  } else if (elapsed < smileRiseDuration + smileHoldDuration) {
    smileOffset = smileRiseHeight;
  } else if (elapsed < totalSmileTime) {
    int fallElapsed = elapsed - smileRiseDuration - smileHoldDuration;
    smileOffset = map(fallElapsed, 0, smileFallDuration, smileRiseHeight, 0);
  }
  int cx = SCREEN_WIDTH - 84; 
  drawTiltedSmile(display, cx, 105, 55, 20, smileOffset, 0.5); 
}

void drawBlinkLids(Adafruit_SH1107 &display, unsigned long now, EyeState &eye, int &blinkStage, unsigned long &lastBlinkTime, unsigned long &blinkInterval) {
  bool allowBlink = (eye.mode == EYE_NEUTRAL);

  if (allowBlink) {
    if (blinkStage == 0 && now - lastBlinkTime > blinkInterval) {
      blinkStage = 1;
      lastBlinkTime = now;
    } else if (blinkStage > 0 && now - lastBlinkTime > blinkStepDelay) {
      blinkStage++;
      lastBlinkTime = now;
      if (blinkStage > 6) {
        blinkStage = 0;
        blinkInterval = random(3000, 6000);
      }
    }
  }

  if (blinkStage > 0) {
    int blinkLevel = (blinkStage <= 3) ? blinkStage : 6 - blinkStage;
    int lidHeight = (SCREEN_HEIGHT / 6) * blinkLevel;

    display.fillRect(0, 0, SCREEN_WIDTH, lidHeight, SH110X_BLACK);
    display.fillRect(0, SCREEN_HEIGHT - lidHeight, SCREEN_WIDTH, lidHeight, SH110X_BLACK);
  }
}

void renderEyeRight(unsigned long now) {
  displayRight.clearDisplay();
  displayRight.drawBitmap(0, 0, eye_bitmap, EYE_WIDTH, EYE_HEIGHT, SH110X_WHITE);

  switch (eyeRight.mode) {
    case EYE_HAPPY:
      drawHappyRight(displayRight, now - eyeRight.startTime);
      break;
    case EYE_SAD:
      drawSadRight(displayRight, now - eyeRight.startTime);
      break;
    case EYE_NEUTRAL:
    default:
      break;
  }

  drawBlinkLids(displayRight, now, eyeRight, blinkStageRight, lastBlinkTimeRight, blinkIntervalRight);
  displayRight.display();
}

void renderEyeLeft(unsigned long now) {
  displayLeft.clearDisplay();
  displayLeft.drawBitmap(0, 0, eye_bitmap, EYE_WIDTH, EYE_HEIGHT, SH110X_WHITE);

  switch (eyeLeft.mode) {
    case EYE_HAPPY:
      drawHappyLeft(displayLeft, now - eyeLeft.startTime);
      break;
    case EYE_SAD:
      drawSadLeft(displayLeft, now - eyeLeft.startTime);
      break;
    case EYE_NEUTRAL:
    default:
      break;
  }

  drawBlinkLids(displayLeft, now, eyeLeft, blinkStageLeft, lastBlinkTimeLeft, blinkIntervalLeft);
  displayLeft.display();
}

void updateEyeState(EyeState &eye, unsigned long now) {
  static unsigned long lastTriggerLeft = 0;
  static unsigned long lastTriggerRight = 0;
  static bool nextSadLeft = true;
  static bool nextSadRight = true;

  if (&eye == &eyeLeft) {
    if (eye.mode == EYE_NEUTRAL && now - lastTriggerLeft > random(6000, 10000)) {
      eye.mode = nextSadLeft ? EYE_SAD : EYE_HAPPY;
      eye.animating = true;
      eye.startTime = now;
      lastTriggerLeft = now;
      nextSadLeft = !nextSadLeft;
    }

    if (eye.mode == EYE_HAPPY && eye.animating && now - eye.startTime > totalSmileTime) {
      eye.mode = EYE_NEUTRAL;
      eye.animating = false;
    }

    if (eye.mode == EYE_SAD && eye.animating && now - eye.startTime > totalSadTime) {
      eye.mode = EYE_NEUTRAL;
      eye.animating = false;
    }
  } else if (&eye == &eyeRight) {
    if (eye.mode == EYE_NEUTRAL && now - lastTriggerRight > random(6000, 10000)) {
      eye.mode = nextSadRight ? EYE_SAD : EYE_HAPPY;
      eye.animating = true;
      eye.startTime = now;
      lastTriggerRight = now;
      nextSadRight = !nextSadRight;
    }

    if (eye.mode == EYE_HAPPY && eye.animating && now - eye.startTime > totalSmileTime) {
      eye.mode = EYE_NEUTRAL;
      eye.animating = false;
    }

    if (eye.mode == EYE_SAD && eye.animating && now - eye.startTime > totalSadTime) {
      eye.mode = EYE_NEUTRAL;
      eye.animating = false;
    }
  }
}

void setup() {
  Serial.begin(115200);


  //wireLeft.begin(21, 22);  
  wireRight.begin(19, 18); 

 /* Serial.println("Initializing Left Display...");
  bool leftOk = displayLeft.begin(0x3C, true);
  if (!leftOk) {
    Serial.println("Left display not found.");
  }
*/
  Serial.println("Initializing Right Display...");
  bool rightOk = displayRight.begin(0x3C, true);
  if (!rightOk) {
    Serial.println("Right display not found. Halting.");
    while (1);
  }

//  displayLeft.clearDisplay();
//  displayLeft.display();

  displayRight.clearDisplay();
  displayRight.display();
}

void loop() {
  unsigned long now = millis();

//  updateEyeState(eyeLeft, now);
  updateEyeState(eyeRight, now);

 // renderEyeLeft(now);
  renderEyeRight(now);

  delay(30);
}
