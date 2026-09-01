# Godwise 生日时钟

基于 [Clockwise](https://github.com/jnthas/clockwise) 定制的 **64×64 LED 肖像时钟**（生日礼物版）。

手机打开 `http://clockwise.local/poke` → **请出去**。

---

## 请出去 → 数字钟 + 沙漏

人物约 **3 秒** 慢慢滑出 → 圆表盘旋转缩小消失 → 原背景上留下 `HH:MM`。  
沙漏跟着秒走，**每过一分钟换一张背景**。点「请回来」再滑回来。

![请出去](docs/demo/03_slide_digital.gif)

---

## 烧录

```bash
cd firmware
set FW_NAME=GodwisePortrait
pio run -e esp32dev -t upload --upload-port COMx
```

Fork from [jnthas/clockwise](https://github.com/jnthas/clockwise)
