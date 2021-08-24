# AVI Player

## 使用方法
1. 准备硬件
    - 程序配套使用的是 [ESP32-LCDKit](https://blog.csdn.net/espressif/article/details/101547122) 开发板，**需使用带 PSRAM 的 ESP32-DevKitC**
    - 插入 spi 接口的屏幕到开发板
    - 准备 SD 卡用来存放视频文件

2. 准备视频文件
    ``` shell
    ffmpeg -i 原始视频.mp4 -vcodec mjpeg -vf scale=240:240 -r 14 -acodec pcm_s16le -ar 32000 tom-240.avi
    ```
    将生成的 `tom-240.avi` 文件放到 SD 卡内

3. 烧录固件
    ``` shell
    idf.py -b 921600 -p /dev/ttyUSB0 flash monitor
    ```
4. 插入 SD 卡，自动播放 `tom-240.avi` 视频，音频输出为单声道，在开发板的 J6 接口接一只喇叭即可。

![demo](demo.gif)
