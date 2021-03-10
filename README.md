# ESP32 Surveillance Camera

![license](https://img.shields.io/github/license/TDA-2030/surveillance-camera)

![build](https://img.shields.io/badge/build-passing-brightgreen)

I would like to make a surveillance camera with ESP32-CAM, but I have no idea to do it until see [ESP32-CAM-Video-Recorder](https://github.com/jameszah/ESP32-CAM-Video-Recorder). This project will not be the same as that one. I added a screen to display the image or video, and have to cut down the control signal(like button, motion sensor) caused by lacking gpio number.

Another [ext-board](https://oshwhub.com/TDA2030/ESP-CAM-EXT) is necessary by screen. 

## Funcitons

Tick the box if finished.

- [ ] Record video and save to sdcard
- [ ] Play video on screen
- [ ] Control camera by browser
- [ ] Upload image to cloud services

## Known issues

- Screen and sdcard cannot be used at the same time. But the ext-board is designed for this.
