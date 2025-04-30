# ESP32环境监测系统

[![PlatformIO](https://img.shields.io/badge/PlatformIO-IDE-orange.svg)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-C3-blue.svg)](https://www.espressif.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## 项目概述

这是一个基于ESP32的环境监测系统，能够实时监测温度、湿度、光照和气体浓度等环境参数，并通过OLED显示屏和WS2812 LED灯条提供直观的状态反馈。系统支持WiFi连接和远程数据上传，使用了贝玛物联平台(BEMFA)进行数据传输和远程控制。

## 功能特点

1. **多参数环境监测**：实时监测温度、湿度、光照和气体浓度
2. **智能状态显示**：
   - OLED屏幕显示所有传感器数据
   - WS2812 LED灯条以不同颜色和动画效果指示系统状态
     - 绿色：环境正常
     - 橙色：环境参数超出正常范围(流水灯效果，速度随温度变化)
     - 红色：系统异常
     - 蓝色：WiFi连接中(呼吸灯效果)
3. **WiFi配网功能**：
   - 支持自动连接已保存的WiFi
   - 通过配置按钮进入配网模式
   - 配网超时自动重启
4. **远程数据传输**：
   - 通过TCP连接上传数据到贝玛物联平台
   - 支持远程控制LED
   - 自动重连机制

## 硬件要求

### 主要组件

- ESP32-C3开发板 (AirM2M Core ESP32C3)
- DHT11温湿度传感器
- 光照传感器
- MQ135气体传感器
- SSD1306 OLED显示屏
- WS2812 RGB LED灯条(30个LED)
- 配置按钮
- 用户LED指示灯

### 引脚连接

| 功能 | ESP32引脚 |
|------|----------|
| 用户LED | 6 |
| 配置按钮 | 9 |
| I2C SDA | 4 |
| I2C SCL | 5 |
| DHT11 | 3 |
| 光照传感器 | 2 |
| 气体传感器 | 1 |
| WS2812 | 7 |

## 开发环境

项目使用PlatformIO开发环境，依赖以下库：

- WiFiManager (用于WiFi配置)
- Adafruit SSD1306 (OLED显示屏驱动)
- DHT sensor library (温湿度传感器驱动)
- Adafruit Unified Sensor (传感器统一接口)
- Adafruit NeoPixel (WS2812 LED控制)

## 快速开始

1. 克隆仓库：
   ```bash
   git clone https://github.com/Frisco11/ESP32-Environment-Monitor.git
   ```

2. 安装依赖：
   - 安装 [PlatformIO IDE](https://platformio.org/platformio-ide)
   - 打开项目并等待依赖自动安装

3. 硬件连接：
   - 按照引脚连接表连接各个传感器
   - 确保电源供应稳定

4. 编译上传：
   - 使用PlatformIO编译并上传代码到ESP32

5. 首次配置：
   - 按住配置按钮并上电，进入配网模式
   - 连接名为"ESP32_AP"的WiFi热点
   - 在弹出的配置页面中输入WiFi信息

## 注意事项

- 首次使用气体传感器需要预热一段时间才能获得稳定读数
- 确保WiFi信号稳定，避免频繁断开重连
- 使用贝玛物联平台前需要在代码中配置UID和Topic

## 贡献指南

欢迎提交问题和改进建议！提交PR时请：

1. Fork本仓库
2. 创建您的特性分支 (git checkout -b feature/AmazingFeature)
3. 提交您的改动 (git commit -m 'Add some AmazingFeature')
4. 推送到分支 (git push origin feature/AmazingFeature)
5. 创建一个Pull Request

## 许可证

本项目采用MIT许可证 - 详见 [LICENSE](LICENSE) 文件

## 联系方式

如有问题或建议，欢迎通过以下方式联系：

- 提交 [Issue](https://github.com/Frisco11/ESP32-Environment-Monitor/issues)
- 发送邮件至：[2637822378@qq.com](mailto:2637822378@qq.com)