# Sprinkler Usurper



## Getting started

To make it easy for you to get started with GitLab, here's a list of recommended next steps.

Already a pro? Just edit this README.md and make it your own. Want to make it easy? [Use the template at the bottom](#editing-this-readme)!

## Add your files

- [ ] [Create](https://docs.gitlab.com/ee/user/project/repository/web_editor.html#create-a-file) or [upload](https://docs.gitlab.com/ee/user/project/repository/web_editor.html#upload-a-file) files
- [ ] [Add files using the command line](https://docs.gitlab.com/ee/gitlab-basics/add-file.html#add-a-file-using-the-command-line) or push an existing Git repository with the following command:

```
cd existing_repo
git remote add origin http://spud.local/pico/usurper.git
git branch -M main
git push -uf origin main
```

## Integrate with your tools

- [ ] [Set up project integrations](http://spud.local/pico/usurper/-/settings/integrations)

## Collaborate with your team

- [ ] [Invite team members and collaborators](https://docs.gitlab.com/ee/user/project/members/)
- [ ] [Create a new merge request](https://docs.gitlab.com/ee/user/project/merge_requests/creating_merge_requests.html)
- [ ] [Automatically close issues from merge requests](https://docs.gitlab.com/ee/user/project/issues/managing_issues.html#closing-issues-automatically)
- [ ] [Enable merge request approvals](https://docs.gitlab.com/ee/user/project/merge_requests/approvals/)
- [ ] [Set auto-merge](https://docs.gitlab.com/ee/user/project/merge_requests/merge_when_pipeline_succeeds.html)

## Test and Deploy

Use the built-in continuous integration in GitLab.

- [ ] [Get started with GitLab CI/CD](https://docs.gitlab.com/ee/ci/quick_start/index.html)
- [ ] [Analyze your code for known vulnerabilities with Static Application Security Testing (SAST)](https://docs.gitlab.com/ee/user/application_security/sast/)
- [ ] [Deploy to Kubernetes, Amazon EC2, or Amazon ECS using Auto Deploy](https://docs.gitlab.com/ee/topics/autodevops/requirements.html)
- [ ] [Use pull-based deployments for improved Kubernetes management](https://docs.gitlab.com/ee/user/clusters/agent/)
- [ ] [Set up protected environments](https://docs.gitlab.com/ee/ci/environments/protected_environments.html)

***

# Editing this README

When you're ready to make this README your own, just edit this file and use the handy template below (or feel free to structure it however you want - this is just a starting point!). Thanks to [makeareadme.com](https://www.makeareadme.com/) for this template.

## Suggestions for a good README

Every project is different, so consider which of these sections apply to yours. The sections used in the template are suggestions for most open source projects. Also keep in mind that while a README can be too long and detailed, too long is better than too short. If you think your README is too long, consider utilizing another form of documentation rather than cutting out information.

## Name
Sprinkler Usurper

## Description
Add Wifi control to a residential sprinkler system using a Raspberry Pi Pico W. This gives you remote control of your sprinkler system.  Optionally read weather information from an Ecowitt weather station (rain, wind, temperature) to control scheduling.  It can also use a Govee Table Lamp or LED strip(s) to indicate operational status.

## Why add Wifi to a "dumb" sprinkler?
I left on a 3 week vacation during a drought.  3 days after I left, my city announced a ban on all irrigation and a $2000 fine!  That fine was cumulative per day!!!  Luckily my neighbor was able to turn off the water for me on that occasion.  I needed a solution for remote access for future similar problems.   In additona, I later discovered that when the wind was blowing stongly my lawn sprinklers would spray my carniverous plant bog -- that can kill them as they need rain water or distilled water.  So, by checking the wind speed the Pico W avoids running the sprinklers when the wind is too high. 

## How it works
A relay controlled by the Pico W is insertd into the common line that runs from the existing controller to the sprinkler valves.  A 2 minute job.  The existing controller is then configured to run everyday, or even multiple times every day.   The Pico connects / disconnects the common line to control when the sprinklers actually run.

## Installation

sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
git clone --recurse-submodules http://github/this_project 
cd usurper
mkdir build
cd build
cmake ..

Upon completion of a successful build the file usurper.uf2 should be created.  This may be loaded onto the Pico W in the usual manner.

## Initial Configuration
The Pico W will initially create a WiFi network with SSID = pluto.  Connect to this WiFi network and then point your web browser to http://192.168.4.1

Set the WiFi country, network and password then hit save and reboot.  The Pico will attempt to connect to the WiFi network.  If it fails then it will fall back to AP mode and you can once again connect to pluto and correct your mistakes.

## Optional Hardware
Signal Relay Module for Raspberry Pi Pico, SPDT 2Amp / AC120V DC24V (this uses GPIO3)
ECOWITT Wittboy Weather Station GW2001
Govee Table Lamp H60511D1
Additional Pico W to control remote LED strips (up to 6)

