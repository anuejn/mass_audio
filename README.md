# `mass_audio`
![build badge](https://github.com/anuejn/mass_audio/actions/workflows/build.yml/badge.svg)

An experiment for distributing audio over a connectionless wifi mesh network in
highly dynamic network situations such as "critical mass" bike tours.


## Build it!
```sh
git clone --recursive https://github.com/anuejn/mass_audio  # mind the --recursive; this project makes heavy use of submodules
cd mass_audio

# you may need to install ncurses, gperf, flex and bison
. ./setup_env.sh
idf.py build

idf.py flash # flash the board (make sure you have the correct permissions)
idf.py monitor # look at the output (quit with ctrl-t ctrl-q)
```