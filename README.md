# Triton/Triton Extreme PCG to VST Json (WIP)

**Important:** this repository is work in progress and not currently promoted anywhere.

Dev workflow:
I was able to quickly start working on the project thanks to the Alchemist API (see dependencies section). They had already implemented PCG parsing tools, back in 2001.
Some of the offsets were missing (IFX, MFX, custom params, and Triton Extreme-related params), I retrieved those by analyzing PCG files saved from my own Korg Triton Extreme (changing parameters one by one and then comparing saved binaries in an hex editor).
Then I worked on mapping the PCG offsets with each json parameter. I extended the program so it can save formatted json patches that the VST can read.
I did my best to factorize code and not duplicate sections that were reused (for instance, each program has 2 blocks of params for each OSC, but the data is aligned the same way).

Currently implemented:
- Program conversions
- Combi conversions
- IFX and MFX (WIP: the order of specific parameters is different for each of the 101 available effects, both in the PCG and the json)
- Triton Extreme specific params (Valve Force)
- Unit test: currently, Prog A000 (BD Grand Concert) matches 100%

TODO:
- Global settings conversions
- User arpeggiator
- User drumkit
- Handle all effect custom parameters

## Usage
```
-PCG <Path> : path of the PCG file to open
-OutFolder <Path> : path of the destination folder for the output json patches
-Combi <Letters> : combis to export (max:4)
-Program <Letters> : programs to export (max:4)
[-unit_test] : performs unit test (optional)
```

Examples
```
    PCGToVST -PCG "TRITON1.PCG" -OutFolder "C:\Temp" -Program A B -Combi N
```

## Dependencies
- [Niels Lohmann's JSON for Modern C++](https://github.com/nlohmann/json)
- [Alchemist Trinity to Triton Tools](https://sourceforge.net/projects/alchemist/)