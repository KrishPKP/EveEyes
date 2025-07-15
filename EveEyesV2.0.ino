#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "eye_bitmap.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128

Adafruit_SH1107 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

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

EyeState eye;


// Smile animation 
const int smileRiseHeight     = 20;
const int smileRiseDuration   = 1000;
const int smileHoldDuration   = 2000;
const int smileFallDuration   = 400;
const int totalSmileTime = smileRiseDuration + smileHoldDuration + smileFallDuration;

// Sad animation  
const int sadCoverMaxOffset   = 20;
const int sadRiseDuration     = 1000;
const int sadHoldDuration     = 2000;
const int sadFallDuration     = 400;
const int totalSadTime = sadRiseDuration + sadHoldDuration + sadFallDuration;

// Blink animation
int blinkStage = 0;
unsigned long lastBlinkTime = 0;
unsigned long blinkInterval = 4000;
const int blinkStepDelay = 60;


void drawTiltedSmile(int cx, int cy, int rx, int ry, int offsetY, float tilt) {
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

void drawTiltedFrown(int cx, int cy, int rx, int ry, int offsetY, float tilt) {
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

void drawSad(unsigned long elapsed) {
  int offsetY = 0;

  if (elapsed < sadRiseDuration) {
    offsetY = map(elapsed, 0, sadRiseDuration, 0, sadCoverMaxOffset);
  } else if (elapsed < sadRiseDuration + sadHoldDuration) {
    offsetY = sadCoverMaxOffset;  
  } else if (elapsed < totalSadTime) {
    int fallElapsed = elapsed - sadRiseDuration - sadHoldDuration;
    offsetY = map(fallElapsed, 0, sadFallDuration, sadCoverMaxOffset, 0);
  }

  drawTiltedFrown(130, 20, 75, 20, offsetY, 0.5);
}


void drawHappy(unsigned long elapsed) {
  int smileOffset = 0;

  if (elapsed < smileRiseDuration) {
    smileOffset = map(elapsed, 0, smileRiseDuration, 0, smileRiseHeight);
  } else if (elapsed < smileRiseDuration + smileHoldDuration) {
    smileOffset = smileRiseHeight;
  } else if (elapsed < totalSmileTime) {
    int fallElapsed = elapsed - smileRiseDuration - smileHoldDuration;
    smileOffset = map(fallElapsed, 0, smileFallDuration, smileRiseHeight, 0);
  }

  drawTiltedSmile(84, 105, 55, 20, smileOffset, -0.5);
}


void drawBlinkLids(unsigned long now) {
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

void renderEye(unsigned long now) {
  display.clearDisplay();
  display.drawBitmap(0, 0, eye_bitmap, EYE_WIDTH, EYE_HEIGHT, SH110X_WHITE);

  switch (eye.mode) {
    case EYE_HAPPY:
      drawHappy(now - eye.startTime);
      break;
    case EYE_SAD:
      drawSad(now - eye.startTime);
      break;
    case EYE_NEUTRAL:
    default:
      break;
  }

  drawBlinkLids(now);
  display.display();
}

void updateEyeState(unsigned long now) {
  static unsigned long lastTrigger = 0;
  static bool nextSad = true;

  if (eye.mode == EYE_NEUTRAL && now - lastTrigger > random(6000, 10000)) {
    eye.mode = nextSad ? EYE_SAD : EYE_HAPPY;
    eye.animating = true;
    eye.startTime = now;
    lastTrigger = now;
    nextSad = !nextSad;
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

void setup() {
  Wire.begin(21, 22);
  Serial.begin(115200);

  if (!display.begin(0x3C, true)) {
    Serial.println("SH1107 not found");
    while (1);
  }

  display.clearDisplay();
  display.display();
}

void loop() {
  unsigned long now = millis();
  updateEyeState(now);
  renderEye(now);
  delay(30);
}
