#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Adafruit_NeoPixel.h>
#include <ButtonDebounce.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

#define BTN_R_PRESSED 0
#define BTN_R_RELEASED 1

#define BTN_L_PRESSED 0
#define BTN_L_RELEASED 1

int ledPin1 = 2;
int ledPin2 = 5;
int totalLEDs1 = 1;
int totalLEDs2 = 16;
int ledFadeTime = 10;

int buttonPinR = 6;
int buttonPinL = 7;

volatile bool fade = true;
volatile bool flicker = false;
volatile bool tricolore = false;
volatile bool rainbow = false;

float threshold = 0.15;

void buttonChangedR(const int state);
void buttonChangedL(const int state);

void fadeTask(void *parameter);
void flickerTask(void *parameter);
void tricoloreTask(void *parameter);
void rainbowTask(void *parameter);
void accelerometerTask(void *parameter);
void rgbFadeInAndOut(uint8_t red, uint8_t green, uint8_t blue, uint8_t wait);

Adafruit_NeoPixel strip = Adafruit_NeoPixel(totalLEDs2, ledPin2, NEO_GRBW + NEO_KHZ800);

ButtonDebounce buttonL(buttonPinL, 250);
ButtonDebounce buttonR(buttonPinR, 250);

void setup()
{
  buttonL.setCallback(buttonChangedL);
  buttonR.setCallback(buttonChangedR);

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  xTaskCreate(fadeTask, "FadeTask", 10000, NULL, 2, NULL);
  xTaskCreate(flickerTask, "FlickerTask", 10000, NULL, 3, NULL);
  xTaskCreate(tricoloreTask, "TriColoreTask", 10000, NULL, 4, NULL);
  xTaskCreate(rainbowTask, "RainbowTask", 10000, NULL, 4, NULL);
  xTaskCreate(accelerometerTask, "AccelerometerTask", 10000, NULL, 1, NULL);
}

void loop()
{
  buttonR.update();
  buttonL.update();
  vTaskDelay(pdMS_TO_TICKS(20)); // Delay for 20 milliseconds
}

void fadeTask(void *parameter)
{
  while (1)
  {
    if (fade && !tricolore && !rainbow)
    {
      rgbFadeInAndOut(0, 0, 255, ledFadeTime); // Blue
      rgbFadeInAndOut(0, 255, 255, ledFadeTime);
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Delay for 10 milliseconds
  }
}

void flickerTask(void *parameter)
{
  TickType_t flickerDuration = pdMS_TO_TICKS(1000); // Duration of flicker in milliseconds
  TickType_t startTime = 0;

  while (1)
  {
    if (flicker && !tricolore && !rainbow)
    {

      for (uint8_t i = 0; i < strip.numPixels(); i++)
      {
        strip.setPixelColor(i, 0, 0, 0);
      }
      strip.show();
      vTaskDelay(pdMS_TO_TICKS(50)); // Delay for 10 milliseconds

      if (startTime == 0)
      {
        startTime = xTaskGetTickCount(); // Record the start time
      }

      // Flicker the LED strip red
      for (uint8_t i = 0; i < strip.numPixels(); i++)
      {
        strip.setPixelColor(i, 255, 0, 0);
      }
      strip.show();

      TickType_t currentTime = xTaskGetTickCount();     // Get the current time
      TickType_t elapsedTime = currentTime - startTime; // Calculate the elapsed time

      if (elapsedTime >= flickerDuration)
      {
        flicker = false; // Duration reached, reset the flicker flag
        fade = true;
        startTime = 0; // Reset the start time
      }
    }
    else
    {
      startTime = 0; // Reset the start time when flicker is not active
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // Delay for 10 milliseconds
  }
}

void tricoloreTask(void *parameter)
{
  while (1)
  {
    if (tricolore)
    {
      for (int i = 0; i < 16; i++)
      {
        if (i < 6)
          strip.setPixelColor(i, 0, 0, 250);
        if (i > 5 && i < 11)
          strip.setPixelColor(i, 0, 0, 0, 127);
        if (i > 10)
          strip.setPixelColor(i, 250, 0, 0);
      }
      strip.show();
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // Delay for 10 milliseconds
  }
}

void rainbowTask(void *parameter)
{
  while (1)
  {
    if (rainbow)
    {
      for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256)
      {

        if (!fade)
          strip.rainbow(firstPixelHue);

        if (!fade)
          strip.show();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // Delay for 10 milliseconds
  }
}

void accelerometerTask(void *parameter)
{
  Adafruit_MPU6050 mpu;
  TwoWire MPU_bus = TwoWire(0);

  MPU_bus.begin(19, 18);

  float prevAccX = 0.0;
  float prevAccY = 0.0;
  float prevAccZ = 0.0;

  if (!mpu.begin(0x68, &MPU_bus))
  {
    Serial.println("Failed to find MPU6050 chip");
    while (1)
    {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_94_HZ);

  sensors_event_t a, g, temp;

  while (1)
  {
    mpu.getEvent(&a, &g, &temp);

    if (fabs(a.acceleration.x - prevAccX) > threshold || fabs(a.acceleration.y - prevAccY) > threshold || fabs(a.acceleration.z - prevAccZ) > threshold)
    {
      fade = false;
      flicker = true;
    }

    prevAccX = a.acceleration.x;
    prevAccY = a.acceleration.y;
    prevAccZ = a.acceleration.z;

    vTaskDelay(pdMS_TO_TICKS(50)); // Delay for 50 milliseconds
  }
}

void rgbFadeInAndOut(uint8_t red, uint8_t green, uint8_t blue, uint8_t wait)
{
  for (uint8_t b = 10; b < 245; b++)
  {
    for (uint8_t i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, red * b / 255, green * b / 255, blue * b / 255);
    }
    if (fade && !tricolore)
      strip.show();
    vTaskDelay(pdMS_TO_TICKS(wait)); // Delay for 50 milliseconds
  }

  for (uint8_t b = 245; b > 10; b--)
  {
    for (uint8_t i = 0; i < strip.numPixels(); i++)
    {
      strip.setPixelColor(i, red * b / 255, green * b / 255, blue * b / 255);
    }
    if (fade && !tricolore)
      strip.show();
    vTaskDelay(pdMS_TO_TICKS(wait)); // Delay for 50 milliseconds
  }
}

void buttonChangedR(const int state)
{
  if (state == BTN_R_PRESSED)
  {
    tricolore = true;
    rainbow = false;
    fade = false;
  }
  if (state == BTN_R_RELEASED)
  {
    tricolore = false;
    fade = true;
  }
}

void buttonChangedL(const int state)
{
  if (state == BTN_L_PRESSED)
  {
    rainbow = true;
    tricolore = false;
    fade = false;
  }
  if (state == BTN_L_RELEASED)
  {
    rainbow = false;
    fade = true;
  }
}
