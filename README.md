# Sprinkler Usurper

## Description
Add Wifi control to an ordinary residential sprinkler system using a Raspberry Pi Pico W. This gives you remote control of your sprinkler system.  Optionally read weather information from an Ecowitt weather station (rain, wind, temperature) to control scheduling.  It can also use a Govee Table Lamp or LED strip(s) to indicate operational status.  

## How it works
You can either add a web inteface to an existing "dumb" sprinkler controller or entirely implement the controller. If you keep the existing controller in place, then you only need one relay.  If you choose to implement the entire controller then you will need one relay for each irrigation zone plus a 24VAC power supply.

### Option 1: Add Wifi to an existing irrigation controller
A relay controlled by the Pico W is inserted into the common line that runs from the existing controller to the sprinkler valves.  An easy 2 minute job.  The existing irrigation controller is then configured to run the sprinklers every day, or even multiple times every day.   The Pico connects / disconnects the common line to control when the sprinklers actually run.
![usurper-system](https://github.com/user-attachments/assets/6ff725fb-f210-417a-a9da-73a56ac723e8)

### Option 2: Implement the entire irrigation controller
Relays controlled by the Pico W are used to switch 24VAC to each irrigation zone. A device like "Waveshare Industrial 8-Channel Relay Module for Raspberry Pi Pico" can be used to control up to 8 irrigation zones.
![Screenshot from 2024-10-11 20-26-41](https://github.com/user-attachments/assets/6641408e-83aa-46b1-a006-ec8aa4d0b83c)

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
- The Pico W will initially create a WiFi network with called **pluto**.  Connect to this WiFi network and then point your web browser to http://192.168.4.1
- Note that many web browsers automatically change the URL from http:// to https:// so if it is not connecting you might need to reenter the URL.
- Set the WiFi country, network and password then hit save and reboot.  The Pico will attempt to connect to the WiFi network.  If it fails then it will fall back to AP mode and you can once again connect to the pluto network and correct your mistakes.  
- Set the personality by navigating to Settings/Foibles/☺
  --Sprinkler Usurper for single relay
  --Sprinkler Controller for multiple relays
  --LED Controller for remote LED strip
- Refer to the Wiki for more detailed configuration

## Optional Hardware
I bought this stuff on Amazon and it is available elsewhere.  The relay module is especially convenient for this project. 
- Signal Relay Module for Raspberry Pi Pico, SPDT 2Amp / AC120V DC24V
- ECOWITT Wittboy Weather Station GW2001
- Govee Table Lamp H60511D1
- Addressable RGB LED strips
- Additional Pico W boards to control remote LED strips
- Waveshare Industrial 8-Channel Relay Module for Raspberry Pi Pico

## Licenses
- SPDX-License-Identifier: BSD-3-Clause
- SPDX-License-Identifier: MIT 
