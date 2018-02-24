
#include <Adafruit_GFX.h>    // Core graphics library
#include <SPI.h>
#include <Wire.h>      // this is needed even tho we aren't using it
#include <Adafruit_ILI9341.h>
#include <Adafruit_STMPE610.h>

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 150
#define TS_MINY 130
#define TS_MAXX 3800
#define TS_MAXY 4000

// The STMPE610 uses hardware SPI on the shield, and #8
#define STMPE_CS 8
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);

// The display also uses hardware SPI, plus #9 & #10
#define TFT_CS 10
#define TFT_DC 9
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);


#define TFT_BRIGHTNESS_CONTROL 3
#define MIN_TFT_BRIGHTNESS 10

#define PARTICPANTS_Y_OFFSET 70
#define PARTICIPANTS_HEIGHT 50

int tft_brightness;
unsigned long last_activity_millis = 0;
unsigned long last_day_tick_millis = 0;
int days_left = 14;
bool sleeping = false;

// Configuration
const unsigned long seconds_until_sleep = 30;
const unsigned long day_in_millis = 24ul * 60 * 60 * 1000;

const int rotation_days = 14;
const int participant_count = 3;
const char *participants[] = { "Ben", "Joan", "Maya" };
const char *tasks[] = { "Grosses Bad", "Kleines Bad + WZ", "Kueche" };
bool done[] = { false, false, false };
int rotation_offset = 0;

void set_tft_brightness(int brightness) {
  analogWrite(TFT_BRIGHTNESS_CONTROL, brightness);
  tft_brightness = brightness;
}

void mod_tft_brightness(int delta) {
  if (delta < 0 && tft_brightness + delta < MIN_TFT_BRIGHTNESS) set_tft_brightness(MIN_TFT_BRIGHTNESS);
  else if (delta > 0 && 255 - tft_brightness < delta) set_tft_brightness(255);
  else set_tft_brightness(tft_brightness + delta);
}

bool all_done() {
  for (int i = 0; i<participant_count; i++) {
    if (!done[i]) {
      return false;
    }
  }
  return true;
}

bool overdue() {
  return days_left < 0;
}

void setup() {
  set_tft_brightness(0);
  tft.begin();
  tft.setRotation(2);
  tft.fillScreen(ILI9341_BLACK);
  if (!ts.begin()) {}

  render();
  set_tft_brightness(255);
}

void render() {
  tft.fillScreen(ILI9341_BLACK);
  render_title();
  render_time();
  render_participants();
  render_bottom_bar();
}

void render_title() {
  tft.setTextColor(ILI9341_DARKGREY);
  tft.setTextSize(5);
  tft.setCursor(0, 0);
  tft.print("Putzplan");
}

void render_time() {
  tft.setTextColor(overdue() ? ILI9341_RED : ILI9341_DARKGREY);
  tft.setTextSize(2);
  tft.setCursor(0, 40);
  tft.print(days_left);
  tft.print(" days left");
}

void render_participants() {
  for (int i = 0; i<participant_count; i++) {
    render_participant(i);
  }
}

void render_participant(int index) {
  tft.setTextColor( done[index] ? ILI9341_DARKGREEN : (overdue() ? ILI9341_RED : ILI9341_LIGHTGREY));
  tft.setTextSize(3);
  
  tft.setCursor(5, index*PARTICIPANTS_HEIGHT + PARTICPANTS_Y_OFFSET);
  tft.println(participants[index]);
  tft.setTextSize(2);
  tft.println(tasks[(index + rotation_offset) % participant_count]);
}

void render_bottom_bar() {
  
  tft.setTextColor(ILI9341_DARKGREY);
  tft.setTextSize(3);
  tft.setCursor(25, 300);
  tft.print('-');
  
  tft.setCursor(85, 300);
  tft.print('+');
  
  tft.setTextSize(2);
  tft.setCursor(205, 300);
  tft.print('>');
  
  if (all_done()) {
    tft.setTextColor(ILI9341_GREEN);
    tft.setCursor(130, 306);
    tft.setTextSize(1);
    tft.print("rotieren");
  }
}

void rotate() {
  ++rotation_offset %= participant_count;
  days_left = rotation_days;
  for (int i = 0; i<participant_count; i++) {
    done[i] = false;
  }
  render();
  clearTouchBuffer();
}

void sleep() {
  sleeping = true;
  for (int i = tft_brightness; i>0; i--) {
    set_tft_brightness(i);
    delay(1);
  }
  clearTouchBuffer();
}

void wakeup() {
  sleeping = false;
  render();
  set_tft_brightness(255);
  clearTouchBuffer();
}

void clearTouchBuffer() {
  while (!ts.bufferEmpty()) ts.getPoint();
}

void touch_events() {
  static uint8_t brightness = 255;
  while (! ts.bufferEmpty()) {
    last_activity_millis = millis();
    // Retrieve a point  
    TS_Point p = ts.getPoint();

    if (sleeping) {
      wakeup();
      return;
    }
   
    // Scale from ~0->4000 to tft.width using the calibration #'s
    int16_t x = map(p.x, TS_MAXX, TS_MINX, 0, tft.width());
    int16_t y = map(p.y, TS_MAXY, TS_MINY, 0, tft.height());

    if(y > 280) // bottom bar
    { 
      if (x < 60) mod_tft_brightness(-20);
      else if (x < 120) mod_tft_brightness(+20);
      else if (x < 180) { if (all_done()) rotate(); }
    }
    else if(y > PARTICPANTS_Y_OFFSET) 
    {
      int participantIndex = (y - PARTICPANTS_Y_OFFSET) / PARTICIPANTS_HEIGHT;
      if (participantIndex >= 0 and participantIndex < participant_count) {
        done[participantIndex] = !done[participantIndex];
        render();
        clearTouchBuffer();
      }
    }

    //clearTouchBuffer();
  }
}

void loop() {
  touch_events();
  if (!sleeping && millis() - last_activity_millis > seconds_until_sleep * 1000) {
    sleep();
  }
  if (millis() - last_day_tick_millis > day_in_millis) {
    last_day_tick_millis = millis();
    days_left -= 1;
    render();
    clearTouchBuffer();
  }
}

