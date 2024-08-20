# Triton/Triton Extreme PCG to VST Json (WIP)

**Important:** this repository is work in progress.

Dev workflow:
I was able to quickly start working on the project thanks to the Alchemist API (see dependencies section). They had already implemented PCG parsing tools, back in 2001.
Some of the offsets were missing (IFX, MFX, custom params, and Triton Extreme-related params), I retrieved those by analyzing PCG files saved from my own Korg Triton Extreme (changing parameters one by one and then comparing saved binaries in an hex editor).
Then I worked on mapping the PCG offsets with each json parameter. I extended the program so it can save formatted json patches that the VST can read.
I did my best to factorize code and not duplicate sections that were reused (for instance, each program has 2 blocks of params for each OSC, but the data is aligned the same way).

I'm using local "unit tests" to constantly compare .patch files that I manually converted from the factory presets, and .patch files generated from the factory PCG with this tool. This way I'm avoiding regressions.

Currently implemented:
- Program conversions
- Combi conversions
- All IFX/MFX specific parameters
- Triton Extreme specific params (Valve Force)

I'm working on:
- Global settings conversions
- User arpeggiator
- User drumkit

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

### Note about how the VST stores Combis
On a real Triton, a Combi references up to 8 Programs (called Timbers), and for a Combi to properly work, you need those Programs to exist on your keyboard.
This is not a requirement for the Combis on the VST: when you save a Combi, the parameters for the entirety of the 8 Timbers are stored inside the Combi .patch file.
Which means that as long as your PCG is valid (and that the Programs required for your Combis are there in the first place), it's absolutely safe to use my tool to export Combis on their own, without having to save the Programs separately.
Example: let's say you have a Combi A000 that uses Program B005, Program D105 and Program N015. You just need to call:
PCGToVST -PCG "TRITON1.PCG" -OutFolder "C:\Triton_Presets" -Combi A
And all the necessary parameters from Programs B005, Program D105 and Program N015 will be stored in the new file "C:\Triton_Presets\Combi\USER-A\000.patch"


## Dependencies
- [rapidjson](https://github.com/Tencent/rapidjson)
- [Alchemist Trinity to Triton Tools](https://sourceforge.net/projects/alchemist/)
