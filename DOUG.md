# Doug Notes
- Configure requried before make. To only build scumm engine can do `./configure --disable-all-engines --enable-engine=scumm,scumm_7_8,he`
- I have 8 cpu cores so can do `make -j10` to build it
- `make clean`
- That creates the `./scummvm`