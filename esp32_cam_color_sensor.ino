/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-take-photo-save-microsd-card
  
  IMPORTANT!!! 
   - Select Board "AI Thinker ESP32-CAM"
   - GPIO 0 must be connected to GND to upload a sketch
   - After connecting GPIO 0 to GND, press the ESP32-CAM on-board RESET button to put your board in flashing mode
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
//#include "driver/sdmmc_host.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include "img_converters.h"    // frame type conversions
#include <map>

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define JPEG_QUALITY      10

// Variable holding picture
camera_fb_t * fb;
//LED setup 
int freq = 5000;
int ledCHannel = 4;
int res = 8;
const int ledPin = 4;
int dutyCycle = 4;

void take_photo(bool save);
void analyse_photo();

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
  Serial.begin(115200);
  //Serial.setDebugOutput(true);
  //Serial.println();
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  // Picture format
  config.pixel_format = PIXFORMAT_RGB888; 
  
  if(psramFound()){
    Serial.print("Psram found\n");
    // Frame size
    config.frame_size = FRAMESIZE_240X240;
//    config.frame_size = FRAMESIZE_VGA;
//    config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = JPEG_QUALITY;
    config.fb_count = 2;
  } else {
    Serial.print("Psram not found\n");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  // Camera settings
  sensor_t * s = esp_camera_sensor_get();
  s->set_exposure_ctrl(s, 0);
  s->set_colorbar(s, 0);

  //Serial.println("Starting SD Card");
  if(!SD_MMC.begin("/cdcard", true)){
    Serial.println("SD Card Mount Failed");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  }
  
  // Variable holding picture
  fb = NULL;

  ledcSetup(ledCHannel, freq, res);
  ledcAttachPin(ledPin, ledCHannel);
  ledcWrite(ledCHannel, dutyCycle);

  take_photo(true);

//  // Turn on the white led
//  pinMode(4, OUTPUT);
//  digitalWrite(4, HIGH);
//  rtc_gpio_hold_en(GPIO_NUM_4);
}

std::vector<int> select_pixels(camera_fb_t * const fb) {
  std::vector<int> selected_pixels = {};
  const char *data = (const char *)fb->buf;
  size_t size = fb->len;

  const int treshold = 150;
//  selected_pixels.push_back(1);

  for (int i = 0; i < size - 2; i += 3) {
    if ((int)data[i] >= treshold && (int)data[i + 1] >= treshold && (int)data[i + 2] >= treshold) {
      selected_pixels.push_back(i);
    }
  }

  return selected_pixels;
}

void highlight_pixels(camera_fb_t * fb, const std::vector<int> &selected_pixels) {
  char *data = (char *)fb->buf;
  for (int pixel_id : selected_pixels) {
//    Serial.print((int)data[pixel_id]);
    (fb->buf)[pixel_id + 2] = (char)(200);
//    Serial.print(" ");
//    Serial.print((int)data[pixel_id]);
//    Serial.print("\n"); 
  }
}

void take_photo(bool save) {
  // Take Picture with Camera
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // Saving photo into SD card
  if (save) {   
    // Path where new picture will be saved in SD Card
    String path = "/last_picture.jpg";

    // Highlight bright pixels
    auto selected_pixels = select_pixels(fb);
    highlight_pixels(fb, selected_pixels);

    // Convert frame
    uint8_t * buf;
    size_t buf_len;
    if (!frame2jpg(fb, JPEG_QUALITY, &buf, &buf_len)) {
      Serial.print("Conversion to JPG failed\n");
    } else {
      Serial.print("Conversion to JPG successful\n");
      delete fb->buf;
      fb->buf = buf;
      fb->len = buf_len;
    }
  
    fs::FS &fs = SD_MMC; 
    Serial.printf("Picture file name: %s\n", path.c_str());
    
    File file = fs.open(path.c_str(), FILE_WRITE);
    if(!file){
      Serial.println("Failed to open file in writing mode");
    } 
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.printf("Saved file to path: %s\n", path.c_str());
    }
    file.close();
  }

  // Prepare fb for next picture?
  esp_camera_fb_return(fb); 
}

struct Color {
  Color(int new_r, int new_g, int new_b) {
    r = new_r;
    g = new_g;
    b = new_b;
  }
  int r;
  int g;
  int b;
  bool operator<(const Color &other) const {
    return this->r < other.r;
  }
  bool operator!=(const Color &other) const {
    return (this->r != other.r || this->g != other.g || this->b != other.b);
  }
};

//bool colors_similar(const Color &model, const Color &other) {
//  const int epsilon = 10;
//  return abs(model.r - other.r) <= epsilon && abs(model.g - other.g) <= epsilon && abs(model.b - other.b) <= epsilon;
//}

Color simplify(const Color &color, int number_of_colors, int max_color_value=256) {
  int step = max_color_value / number_of_colors;
  return Color((color.r / step) * step, (color.g / step) * step, (color.b / step) * step);
}

void display_dominant_color(const char *data, const size_t size) {
  std::map<Color, int> classified_colors;
  
  // Iterating over pixels
  for (int i = 0; i < size; i += 3) {
    // check if this pixel's color has already been classified
    Color current_color = Color((int)data[i + 2], (int)data[i + 1], (int)data[i]);
    Color simplified_color = simplify(current_color, 8);
    try {
      classified_colors.at(simplified_color) += 1;  
    }
    catch (const std::out_of_range &e) {
      classified_colors[simplified_color] = 1;
    }
  }

  Color max_color = {0, 0, 0};
  int max_occurences = 0;
//  Color max_color_2 = {0, 0, 0};
//  int max_occurences_2 = 0;
  // Search for dominant color
  for (auto &color : classified_colors) {
    if (color.second > max_occurences && color.first != Color(0, 0, 0)) {
      max_color = color.first;
      max_occurences = color.second;
    }
  }

  Serial.printf("Dominant color: %dR %dG %dB with %d occurences\n", max_color.r, max_color.g, max_color.b, max_occurences);
}

void display_average(const char *data, const size_t size) {
  int red_sum = 0;
  int green_sum = 0;
  int blue_sum = 0;
  
  // Average
  for (int i = 0; i < size; ++i) {
    if (i % 3 == 0) {
      blue_sum += (int)data[i];
    }
    else if (i % 3 == 1) {
      green_sum += (int)data[i];
    }
    else {
      red_sum += (int)data[i];
    }
  }

  int red_average = red_sum * 3 / size;
  int green_average = green_sum * 3 / size;
  int blue_average = blue_sum * 3 / size;
  
  Serial.printf("R%d G%d B%d\n", red_average, green_average, blue_average);
}

void analyse_photo() {
  // Retreive information from fb object
  const char *data = (const char *)fb->buf;
  size_t size = fb->len;
  int width = 3 * fb->width;
  int height = 3 * fb->height;

  display_dominant_color(data, size);  
}

void loop() {
//  dutyCycle += 1;
//  if (dutyCycle > 40) {
//    dutyCycle = 1;
//  }
//  ledcWrite(ledCHannel, dutyCycle);
  take_photo(false);
//  analyse_photo();
  delay(750);
}
