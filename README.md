# GC Nano LCD Jewel

Custom LCD "jewel" insert for the [GC Nano](https://bitbuilt.net/forums/threads/gc-nano-the-worlds-smallest-gamecube.5697/), using an ESP32-C3 and a lensed 0.96" round LCD.

<img src="images/photo.jpg" alt="GC Nano LCD Jewel animation demo" width="60%" />

## Features

- Fits in an unmodified GC Nano shell
- Displays a static or animated GIF (240x198, < 1920KB)
- WiFi access point and web interface for uploading new GIFs
- OTA firmware updates so you don't have to open the case again

## Bill of Materials

- ESP32-C3 Super Mini (or similar) - [AliExpress](https://www.aliexpress.us/item/3256808827955225.html)
- 0.96" 240x198 Round LCD - [AliExpress](https://www.aliexpress.us/item/3256806072157291.html), [docs](https://www.lcdwiki.com/0.96inch_IPS_ST7789_Module)
- Some wire
- 3D printed insert (see [`hardware/` folder](hardware/))

## Assembly

The LCD should be wired to the following pins on the ESP32-C3:

| LCD Pin | ESP32-C3 Pin | Function         |
|---------|--------------|------------------|
| GND     | GND          | Ground           |
| VCC     | 3.3V         | Power            |
| SCL     | GPIO4        | SPI Clock        |
| SDA     | GPIO6        | SPI MOSI         |
| CS      | GPIO7        | SPI Chip Select  |
| DC      | GPIO2        | Data/Command     |
| RST     | GPIO1        | Reset            |

You'll also need to connect GND and 5V on the ESP32-C3 to the corresponding pads on the GC Nano top board.

The screen should press-fit from the top into the 3D printed insert, which can then be glued into the GC Nano shell. The ESP32-C3 can be mounted on the back of the insert with some double-sided tape.

Flash the firmware before closing everything up.

## Uploading GIFs

- Connect to the "GCNANO-XXXXXX" WiFi network (password: "bitbuilt")
- Open a web browser and navigate to <http://gcnano.local>
- Upload the GIF!

## Pre-made GIFs

There are a couple of pre-made GIFs in the [`gifs/` folder](gifs/). If you want to make your own, make sure to resize it to 240x198 and keep the file size under 1920KB. Please send me your GIFs and I'll add them to the collection!

<table>
  <tr>
    <td><img src="gifs/gcnano.gif" width="240" height="198" /></td>
    <td><img src="gifs/badapple.gif" width="240" height="198" /></td>
  </tr>
</table>