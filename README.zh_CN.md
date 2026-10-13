# AI Passport 嘉佰达电池监控器

<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

这是为 FoloToy AI Passport 开发的离线、只读嘉佰达 JBD BMS 监控应用。
当前 `v0.3` 固件可以不经过手机，直接通过 BLE 连接国轩黄金锂电的电池 B。

## 当前目标硬件

- 主机：FoloToy AI Passport，ESP32-C3 rev 1.1，8 MB Flash，无 PSRAM
- 屏幕：240×320
- ESP-IDF：5.5.3
- BMS 蓝牙名称：`03HMB0GL0200CFG180600673`
- 实测 BMS 地址：`20:25:12:29:25:63`
- GATT 服务：`0xFF00`
- 通知特征：`0xFF01`
- 无响应写特征：`0xFF02`

## 已实现功能

- 自动扫描、连接、发现 GATT、订阅通知、发送经过验证的只读查询，并在断线后重连。
- 显示 SOC、总电压、电流、功率、充电/放电/静置状态、剩余/额定容量、RSSI、
  单体数量、最高/最低单体电压和压差。
- 右上角独立显示 AI Passport 自身电量，不遮挡标题。
- 使用覆盖界面所需汉字的 LVGL 中文字体，并针对 240×320 屏幕重新排版。
- JBD 数据帧、校验和解析及定点数显示格式与 BLE/LVGL 解耦，并提供抓包回归测试。

## 安全范围

程序只发送从 HCI 抓包中确认过的 `0x03`（基本状态）和 `0x04`（单体电压）查询。
不会修改保护参数，不会控制 MOS，不会复位、校准或恢复出厂设置。

## 构建

激活 ESP-IDF 5.5.3 后运行：

```sh
idf.py build
idf.py merge-bin -o build/FoloToy-AI-Passport-full.bin
python tools/verify_firmware.py build
```

只能把合并固件写入 `0x0`。不要把仅包含应用的 bin 写到 `0x0`。

```sh
esptool.py --chip esp32c3 --port COM3 write_flash 0x0 build/FoloToy-AI-Passport-full.bin
```

除非单独明确需要，否则不要执行整片擦除。

## 已验证版本

可从 [v0.3 Release](https://github.com/qq305514253/ai-passport-battery-monitor/releases/tag/v0.3)
下载完整合并固件、交付说明和调试归档。

- 完整固件 SHA-256：
  `C8E07A0BB8F00D1BF3D57C2E02D15199DE8BF113BF17F2116C7631C40FF871BC`
- 真机已验证：ESP32-C3 启动、屏幕/LVGL、按键、本机电量、BLE 扫描、直接连接电池 B、
  SOC、24 节单体数据和 RSSI。

## 当前限制

- `v0.3` 是只连接电池 B 的单电池验证版本。
- 电池 A 和两块电池同时监控尚未实现。
- 嘉佰达蓝牙模块可能只允许一个中央设备连接，因此原厂手机 App 与 AI Passport
  可能无法同时连接同一块电池。

本项目基于官方 [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport) 仓库开发。
