# `mass_audio`
![build badge](https://github.com/anuejn/mass_audio/actions/workflows/build.yml/badge.svg)

An experiment for distributing audio over a connectionless wifi mesh network in
highly dynamic network situations such as "critical mass" bike tours.


## Build it!
```sh
git clone --recursive https://github.com/anuejn/audio_mass  # mind the --recursive; this project makes heavy use of submodules

# you may need to install the esp-idf toolchain with ./esp-adf/esp-idf/install.sh
# as well as ncurses, gperf, flex and bison
. ./setup_env.sh
make -j $(nproc)

make flash # flash the board (make sure you have the correct permissions)
make monitor # look at the output (quit with ctrl-t ctrl-q)
```