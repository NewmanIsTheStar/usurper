# Sprinkler Usurper

## Description
Add Wifi control to an ordinary residential sprinkler system using a Raspberry Pi Pico W. This gives you remote control of your sprinkler system.  Optionally read weather information from an Ecowitt weather station (rain, wind, temperature) to control scheduling.  It can also use a Govee Table Lamp or LED strip(s) to indicate operational status.

## How it works
A relay controlled by the Pico W is inserted into the common line that runs from the existing controller to the sprinkler valves.  An easy 2 minute job.  The existing irrigation controller is then configured to run the sprinklers every day, or even multiple times every day.   The Pico connects / disconnects the common line to control when the sprinklers actually run.
![usurper-system](https://github.com/user-attachments/assets/6ff725fb-f210-417a-a9da-73a56ac723e8)


## Installation on Ubuntu Linux
```
sudo apt install git build-essential cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
git clone --recurse-submodules https://github.com/NewmanIsTheStar/usurper.git 
cd usurper
mkdir build
cd build
cmake ..
make
```
Upon completion of a successful build the file usurper.uf2 should be created.  This may be loaded onto the Pico W in the usual manner.

## Initial Configuration
- The Pico W will initially create a WiFi network with SSID = pluto.  Connect to this WiFi network and then point your web browser to http://192.168.4.1
- Set the WiFi country, network and password then hit save and reboot.  The Pico will attempt to connect to the WiFi network.  If it fails then it will fall back to AP mode and you can once again connect to the pluto network and correct your mistakes.

## Optional Hardware
I bought this stuff on Amazon and it is available elsewhere.  The relay module is especially convenient for this project. 
- Signal Relay Module for Raspberry Pi Pico, SPDT 2Amp / AC120V DC24V
- ECOWITT Wittboy Weather Station GW2001
- Govee Table Lamp H60511D1
- Addressable RGB LED strips
- Additional Pico W boards to control remote LED strips

## Why add Wifi to a "dumb" sprinkler?
I went on a 3 week vacation during a drought.  3 days after I left, my city announced a ban on all irrigation and a $2000 fine!  That fine was cumulative per day!!!  Luckily my neighbor was able to turn off the water for me on that occasion.  I needed a solution for remote access. In additon, I later discovered that when the wind was blowing strongly my lawn sprinklers would spray onto my carniverous plant bog -- that can kill them as they need rain water or distilled water.  So, by checking the wind speed the Pico W avoids running the sprinklers when the wind is too high. 

## Licenses
- SPDX-License-Identifier: BSD-3-Clause
- SPDX-License-Identifier: MIT 

## Screenshots

![schedule](https://github.com/user-attachments/assets/0682428a-7491-4e82-b969-fff4b6c39950)

![network](https://github.com/user-attachments/assets/de2f43b7-ac39-487d-975e-9b18a4d62cdb)

![time](https://github.com/user-attachments/assets/db8c4fc2-173f-42f5-a087-f842b5bf5d0c)

![relay](https://github.com/user-attachments/assets/5dcc214c-c489-49dc-a0e0-8403f558d47d)

![weather](https://github.com/user-attachments/assets/c6e6339f-38b1-43f3-ae55-52f9e34f4af6)

![moodlight](https://github.com/user-attachments/assets/af4e9aa5-c000-47bc-941d-0be7298ba6c5)

![ledstrip](https://github.com/user-attachments/assets/a92cf77c-305f-4e33-8d1b-388441cda62a)

![syslog](https://github.com/user-attachments/assets/f7dfe161-d2d1-4564-a7a5-84dbc8a9fbcd)


