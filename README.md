# Sprinkler Usurper

## Description
Add Wifi control to an ordinary residential sprinkler system using a Raspberry Pi Pico W. This gives you remote control of your sprinkler system.  Optionally read weather information from an Ecowitt weather station (rain, wind, temperature) to control scheduling.  It can also use a Govee Table Lamp or LED strip(s) to indicate operational status.

## How it works
A relay controlled by the Pico W is inserted into the common line that runs from the existing controller to the sprinkler valves.  An easy 2 minute job.  The existing irrigation controller is then configured to run the sprinklers every day, or even multiple times every day.   The Pico connects / disconnects the common line to control when the sprinklers actually run.

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

## Configuration Screens
### Schedule
![schedule](https://github.com/user-attachments/assets/0682428a-7491-4e82-b969-fff4b6c39950)
### Network
![network](https://github.com/user-attachments/assets/19809422-0752-45fb-92e2-891b4f614d1d)
### Time
![time](https://github.com/user-attachments/assets/b8215030-1da2-4547-8c82-3486c7bf5ef7)
### Weather
![weather](https://github.com/user-attachments/assets/b73436c7-60dc-419b-8058-5618ca926965)
### Mood Light
![moodlight](https://github.com/user-attachments/assets/2aae15ea-5ef0-47c6-b069-1d75b27b8df9)
### LED Strip
![ledstrip](https://github.com/user-attachments/assets/a59f18f3-6d6d-49de-9398-d42ca36b78b4)
### Syslog
![syslog](https://github.com/user-attachments/assets/cf595d06-1d2e-4dd3-8308-c8a0ba1b9baa)
