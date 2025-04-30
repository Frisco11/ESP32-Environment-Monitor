#include <Arduino.h>
#include <WiFiManager.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// 硬件配置（修改LED定义）
#define USER_LED 6  // 替换原LED_BUILTIN
#define CONFIG_BTN 9
#define I2C_SDA 4
#define I2C_SCL 5
#define DHTPIN 3
#define DHTTYPE DHT11  // 定义DHT传感器类型
#define LIGHT_PIN 2
#define GAS_PIN 1

// WS2812配置
#define LED_PIN 7
#define LED_COUNT 30

// OLED显示配置
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// 外设对象
#define BEMFA_CONFIG \
const char* SERVER_HOST = "bemfa.com"; \
const uint16_t SERVER_PORT = 8344; \
const char* DEVICE_UID = "164b4b76e20e3c8d51236a57955b3951"; \
const char* TOPIC_LED = "light002"; \
const char* TOPIC_HUMI = "shidu004"; \
const char* TOPIC_TEMP = "wendu004"; \
const char* TOPIC_LIGHT = "guangxian004"; \
const char* TOPIC_GAS = "MQ135004"; 
BEMFA_CONFIG

// WS2812颜色定义
#define COLOR_NORMAL strip.Color(0, 255, 0)   // 绿色-正常
#define COLOR_WARNING strip.Color(255, 165, 0) // 橙色-警告
#define COLOR_ERROR strip.Color(255, 0, 0)   // 红色-错误
#define COLOR_WIFI_CONNECTING strip.Color(0, 0, 255) // 蓝色-WiFi连接中

// 流水灯配置
#define LED_FLOW_SPEED_MIN 10    // 最慢速度(毫秒)
#define LED_FLOW_SPEED_MAX 200   // 最快速度(毫秒)
#define LED_FLOW_SPEED_DEFAULT 50 // 默认速度(毫秒)

/******************** 设备配置 ********************/
/******************** 全局对象 ********************/
DHT dht(DHTPIN, DHTTYPE);
WiFiManager wifiManager;
WiFiClient TCPclient;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
unsigned long lastDataSend = 0;
const uint32_t DATA_INTERVAL = 10000;

// 流水灯相关变量
int ledPosition = 0;  // 流水灯位置跟踪
unsigned long lastLedUpdate = 0;
uint32_t ledFlowSpeed = LED_FLOW_SPEED_DEFAULT; // 流水灯速度(可调)
bool isEnvWarningState = false; // 环境警告状态
bool reverseFlow = false; // 流动方向

/******************** 协议生成器 ********************/
String bemfaSubscribeCmd() {
  String cmd;
  cmd.reserve(256); // 预分配内存
  cmd += "cmd=1&uid="; cmd += DEVICE_UID;
  cmd += "&topic=";   cmd += TOPIC_LED;   cmd += "\r\n";
  cmd += "cmd=1&uid="; cmd += DEVICE_UID;
  cmd += "&topic=";   cmd += TOPIC_HUMI;  cmd += "\r\n";
  cmd += "cmd=1&uid="; cmd += DEVICE_UID;
  cmd += "&topic=";   cmd += TOPIC_TEMP;  cmd += "\r\n";
  cmd += "cmd=1&uid="; cmd += DEVICE_UID;
  cmd += "&topic=";   cmd += TOPIC_LIGHT; cmd += "\r\n";
  cmd += "cmd=1&uid="; cmd += DEVICE_UID;
  cmd += "&topic=";   cmd += TOPIC_GAS;   cmd += "\r\n";
  return cmd;
}

String bemfaDataCmd(const char* topic, float value) {
  String cmd;
  cmd.reserve(64);
  cmd += "cmd=2&uid="; cmd += DEVICE_UID;
  cmd += "&topic=";    cmd += topic;
  cmd += "&msg=";      cmd += String(value,1);
  cmd += "\r\n";
  return cmd;
}

String bemfaDataCmd(const char* topic, int value) {
  String cmd;
  cmd.reserve(64);
  cmd += "cmd=2&uid="; cmd += DEVICE_UID;
  cmd += "&topic=";    cmd += topic;
  cmd += "&msg=";      cmd += value;
  cmd += "\r\n";
  return cmd;
}

/******************** 函数声明 ********************/
void updateDisplay(float h, float t, int l, int g);
void sendSensorData(float h, float t, int l, int g);
void handleNetwork();
void handleSensors();
void handleLEDs();
void updateLEDStatus(float h, float t, int l, int g);
void setLEDFlowSpeed(uint32_t speed);
void breathingLED(uint32_t color, int delayMs);



/******************** 初始化 ********************/
void setup() {
  Serial.begin(115200);
  
  // WiFi配置（最优先）
  pinMode(CONFIG_BTN, INPUT_PULLUP);
  wifiManager.setDebugOutput(true);
  wifiManager.setConfigPortalTimeout(180); // 3分钟超时
  
  // 如果按下配置按钮，进入配置模式
  if(digitalRead(CONFIG_BTN) == LOW) {
    Serial.println("进入配网模式，蓝色呼吸灯指示中...");
    // 配网模式下显示蓝色呼吸灯
    while(!wifiManager.startConfigPortal("ESP32_AP")) {
      breathingLED(COLOR_WIFI_CONNECTING, 30);
      // 检查是否超时
      if(!wifiManager.getConfigPortalActive()) {
        Serial.println("配网失败，即将重启...");
        delay(3000);
        ESP.restart();
      }
    }
  } else {
    // 尝试自动连接WiFi
    Serial.println("尝试自动连接WiFi，蓝色呼吸灯指示中...");
    while(!wifiManager.autoConnect("ESP32_AP")) {
      breathingLED(COLOR_WIFI_CONNECTING, 30);
      // 检查是否超时
      if(!wifiManager.getConfigPortalActive()) {
        Serial.println("自动连接失败，即将重启...");
        delay(3000);
        ESP.restart();
      }
    }
  }
  
  Serial.println("\nWiFi已连接: " + WiFi.localIP().toString());
  
  // TCP连接
  if (TCPclient.connect(SERVER_HOST, SERVER_PORT)) {
    TCPclient.print(bemfaSubscribeCmd());
    Serial.println("Subscribed topics");
  }
  
  // 初始化其他外设
  pinMode(USER_LED, OUTPUT);
  digitalWrite(USER_LED, HIGH);
  
  // I2C和OLED初始化
  Wire.begin(I2C_SDA, I2C_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED显示器初始化失败！");
    delay(3000);
    ESP.restart();
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  
  // 传感器初始化
  dht.begin();
  delay(2000);  // 等待传感器初始化完成
  
  // 检查传感器是否正常工作
  if (isnan(dht.readHumidity()) || isnan(dht.readTemperature())) {
    Serial.println("DHT传感器初始化失败！");
    delay(3000);
    ESP.restart();
  }
  analogReadResolution(12);
  
  // 初始化WS2812
  strip.begin();
  strip.setBrightness(50); // 设置亮度为50%
  for(int i=0; i<LED_COUNT; i++) {
    strip.setPixelColor(i, COLOR_WIFI_CONNECTING);
  }
  strip.show();
}



/******************** 网络处理 ********************/
void handleNetwork() {
  // 维持TCP连接
  if (!TCPclient.connected()) {
    if (millis() % 5000 < 100) { // 每5秒尝试重连
      if (TCPclient.connect(SERVER_HOST, SERVER_PORT)) {
        TCPclient.print(bemfaSubscribeCmd());
      }
    }
    return;
  }

  // 处理接收数据
  while (TCPclient.available()) {
    String response = TCPclient.readStringUntil('\n');
    if (response.indexOf(TOPIC_LED) != -1) {
      digitalWrite(USER_LED, response.indexOf("on") != -1 ? LOW : HIGH);
    }
  }
}

/******************** 传感器处理 ********************/
void handleSensors() {
  static uint32_t lastRead = 0;
  if (millis() - lastRead < 2000) return;
  
  // 读取传感器
  float humidity = dht.readHumidity();
  float temp = dht.readTemperature();
  int light = analogRead(LIGHT_PIN);
  int gas = analogRead(GAS_PIN);
  lastRead = millis();

  // 数据校验
  if (isnan(humidity) || isnan(temp)) {
    Serial.println("DHT11读取失败，跳过本次数据发送");
    return;
  }
  
  // 打印传感器数据用于调试
  Serial.printf("温度: %.1f°C, 湿度: %.1f%%\n", temp, humidity);

  // 定时发送
  if (millis() - lastDataSend > DATA_INTERVAL) {
    sendSensorData(humidity, temp, light, gas);
    lastDataSend = millis();
  }
}

/******************** 主循环 ********************/
void loop() {
  handleNetwork();
  handleSensors();
  handleLEDs(); // 独立处理LED，不依赖传感器数据发送周期
}

void updateDisplay(float h, float t, int l, int g) {
  display.clearDisplay();
  display.setCursor(0,0);
  
  // 显示温度
  display.print("Temperature: ");
  display.print(t, 1);
  display.println(" C");
  
  // 显示湿度
  display.print("Humidity: ");
  display.print(h, 1);
  display.println(" %");
  
  // 显示光照
  display.print("Light: ");
  display.print(map(l, 0, 4095, 0, 100));
  display.println(" %");
  
  // 显示气体浓度
  display.print("Gas: ");
  display.println(g);
  
  display.display();
}

// 设置流水灯速度
void setLEDFlowSpeed(uint32_t speed) {
  // 限制速度范围
  if (speed < LED_FLOW_SPEED_MIN) speed = LED_FLOW_SPEED_MIN;
  if (speed > LED_FLOW_SPEED_MAX) speed = LED_FLOW_SPEED_MAX;
  ledFlowSpeed = speed;
  Serial.printf("流水灯速度已设置为: %d ms\n", ledFlowSpeed);
}

// 呼吸灯效果函数
void breathingLED(uint32_t color, int delayMs) {
  static int brightness = 0;
  static bool increasing = true;
  
  // 调整亮度方向
  if (increasing) {
    brightness += 5;
    if (brightness >= 100) {
      brightness = 100;
      increasing = false;
    }
  } else {
    brightness -= 5;
    if (brightness <= 5) {
      brightness = 5;
      increasing = true;
    }
  }
  
  // 设置所有LED为相同颜色和亮度
  strip.setBrightness(brightness);
  for(int i=0; i<LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
  
  // 控制呼吸速度
  delay(delayMs);
}

// 独立处理LED状态更新
void handleLEDs() {
  // 只有在达到更新间隔时才更新LED
  if (millis() - lastLedUpdate < ledFlowSpeed) return;
  
  if (isEnvWarningState) {
    // 警告状态 - 流水灯效果
    strip.clear(); // 先清除所有LED
    
    // 设置8个LED形成更宽的流水效果
    for(int i = 0; i < 8; i++) {
      int pos = (ledPosition + i) % LED_COUNT;
      if (pos < 0) pos += LED_COUNT; // 处理负数位置
      
      // 更平滑的亮度渐变效果
      uint8_t brightness = 255 * (8 - i) / 8;
      strip.setPixelColor(pos, strip.Color(brightness, brightness/3, 0));
      
      // 添加尾迹效果
      if(i > 4) {
        brightness = brightness / 2;
        strip.setPixelColor(pos, strip.Color(brightness, brightness/4, 0));
      }
    }
    
    // 更新位置，根据流动方向移动LED
    if (reverseFlow) {
      ledPosition = (ledPosition - 1) % LED_COUNT;
      if (ledPosition < 0) ledPosition += LED_COUNT; // 处理负数位置
    } else {
      ledPosition = (ledPosition + 1) % LED_COUNT;
    }
    
    // 每10秒改变流动方向
    if(millis() % 10000 < ledFlowSpeed) {
      reverseFlow = !reverseFlow;
      Serial.println(reverseFlow ? "流水灯方向: 反向" : "流水灯方向: 正向");
    }
  }
  
  strip.show();
  lastLedUpdate = millis();
}

// 根据传感器数据更新LED状态
void updateLEDStatus(float h, float t, int l, int g) {
  // 系统状态检查
  bool isSystemOk = TCPclient.connected() && !isnan(h) && !isnan(t);
  
  // 环境状态检查
  bool isEnvWarning = (t > 30.0 || t < 10.0 || h > 80.0 || h < 20.0 || g > 1000);
  isEnvWarningState = isEnvWarning; // 更新全局状态变量
  
  // 设置LED颜色
  uint32_t statusColor;
  if (!isSystemOk) {
    statusColor = COLOR_ERROR;
  } else if (isEnvWarning) {
    statusColor = COLOR_WARNING;
    // 根据环境参数调整流水灯速度
    uint32_t newSpeed = map(t > 30 ? t : 30, 30, 40, LED_FLOW_SPEED_DEFAULT, LED_FLOW_SPEED_MIN);
    setLEDFlowSpeed(newSpeed);
  } else {
    statusColor = COLOR_NORMAL;
    // 正常状态 - 所有LED为绿色
    for(int i=0; i<LED_COUNT; i++) {
      strip.setPixelColor(i, statusColor);
    }
    strip.show();
  }
  
  // 调试信息
  Serial.printf("系统状态: %s, 环境状态: %s\n", 
                isSystemOk ? "正常" : "异常", 
                isEnvWarning ? "警告" : "正常");
}

void sendSensorData(float h, float t, int l, int g) {
  if (!TCPclient.connected()) return;
  
  Serial.println("Updating LED status..."); // 添加调试信息
  updateLEDStatus(h, t, l, g);  // 确保在发送数据前更新LED状态
  
  String payload;
  payload.reserve(256);
  payload += bemfaDataCmd(TOPIC_HUMI, h);
  payload += bemfaDataCmd(TOPIC_TEMP, t);
  payload += bemfaDataCmd(TOPIC_LIGHT, static_cast<int>(map(l, 0, 4095, 0, 100))); // 显式转换为int类型，避免函数重载歧义
  payload += bemfaDataCmd(TOPIC_GAS, g);
  
  TCPclient.print(payload);
  Serial.println("Data sent: " + payload);
  
  // 更新OLED显示
  updateDisplay(h, t, l, g);
}
