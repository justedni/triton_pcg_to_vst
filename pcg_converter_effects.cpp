#include "pcg_converter.h"

std::vector<SubParam> PCG_Converter::program_ifx_offsets = {
	{ 1, 16 }, { 2, 40 }, { 3, 64 }, { 4, 88 }, { 5, 112 }
};

std::vector<TritonStruct> PCG_Converter::program_ifx_conversions = {
	{ "IFX1 Effect Type"							, "effect_type", 16, 0, 7 },
	{ "IFX1 Control Channel (Combi only?)"			, "control_channel", 17, 0, 5 },
	{ "IFX1 On/Off"									, "onoff", 17, 6, 6 },
	{ "IFX1 Chain"									, "chain", 17, 7, 7 },
	{ "IFX1 Pan"									, "pan", 20, 0, 7 },
	{ "IFX1 Routing"								, "bus_select", 21, 0, 7 },
	{ "IFX1 Send 1"									, "send_1_level", 22, 0, 7 },
	{ "IFX1 Send 2"									, "send_2_level", 23, 0, 7 },
};

std::map<int, std::vector<TritonStruct>> PCG_Converter::effect_conversions;

void PCG_Converter::initEffectConversions()
{
	if (!effect_conversions.empty())
		return;

	effect_conversions[1] = { // St. Amp Simulation
		{ "Amplifier Type"			, "0_24", 0, 0, 1, EVarType::Unsigned },
		{ "Wet/Dry"					, "1_24", 1, 0, 6, EVarType::Unsigned },
		{ "Source"					, "2_24", 2, 0, 4, EVarType::Unsigned },
		{ "Amt"						, "3_24", 3, 0, 7, EVarType::Signed } };

	effect_conversions[2] = { // Stereo Compressor
		{ "Envelope Select"			, "0_24", 0, 0, 0, EVarType::Unsigned },
		{ "Sensitivity"				, "1_24", 1, 0, 7, EVarType::Unsigned },
		{ "Attack"					, "2_24", 2, 0, 7, EVarType::Unsigned },
		{ "EQ Trim"					, "3_24", 3, 0, 7, EVarType::Unsigned },
		{ "Pre LEQ Gain"			, "4_24", 4, 0, 5, EVarType::Signed },
		{ "Pre HEQ Gain"			, "5_24", 5, 0, 5, EVarType::Signed },
		{ "Output Level"			, "6_24", 6, 0, 7, EVarType::Unsigned },
		{ "Source"					, "7_24", 7, 0, 7, EVarType::Unsigned },
		{ "Amount"					, "8_24", 8, 0, 7, EVarType::Signed },
		{ "Wet/Dry"					, "9_24", 9, 0, 6, EVarType::Unsigned },
		{ "Source"					, "10_24", 10, 0, 4, EVarType::Unsigned },
		{ "Amt"						, "11_24", 11, 0, 7, EVarType::Signed } };

	effect_conversions[3] = { // Stereo Limiter
		{ "Envelope Select"			, "0_24",  0,  0, 1, EVarType::Unsigned },
		{ "Ratio"					, "1_24",  1,  0, 5, EVarType::Unsigned },
		{ "Threshold"				, "2_24",  2,  0, 7, EVarType::Signed },
		{ "Attack"					, "3_24",  3,  0, 6, EVarType::Unsigned },
		{ "Release"					, "4_24",  4,  0, 6, EVarType::Unsigned },
		{ "Gain Adjust"				, "5_24",  5,  0, 5, EVarType::Signed },
		{ "Source"					, "6_24",  6,  0, 7, EVarType::Unsigned },
		{ "Source Amount"			, "7_24",  7,  0, 6, EVarType::Signed },
		{ "Side PEQ Insert"			, "8_24",  0,  2, 2, EVarType::Unsigned },
		{ "Trigger Monitor"			, "9_24",  0,  3, 3, EVarType::Unsigned },
		{ "Side PEQ Cutoff"			, "10_24", 8,  0, 7, EVarType::Unsigned },
		{ "Q"						, "11_24", 9,  0, 5, EVarType::Unsigned },
		{ "Gain"					, "12_24", 10, 0, 7, EVarType::Signed },
		{ "Wet/Dry"					, "13_24", 11, 0, 6, EVarType::Unsigned },
		{ "Source"					, "14_24", 12, 0, 4, EVarType::Unsigned },
		{ "Amt"						, "15_24", 13, 0, 7, EVarType::Signed } };

	effect_conversions[4] = { // Multiband Limiter
		{ "Ratio"					, "0_24", 0, 0, 7, EVarType::Unsigned },
		{ "Threshold"				, "1_24", 1, 0, 7, EVarType::Signed },
		{ "Attack"					, "2_24", 2, 0, 7, EVarType::Unsigned },
		{ "Release"					, "3_24", 3, 0, 7, EVarType::Unsigned },
		{ "Low Offset"				, "4_24", 5, 0, 7, EVarType::Signed },
		{ "Mid Offset"				, "5_24", 6, 0, 7, EVarType::Signed },
		{ "High OFfset"				, "6_24", 7, 0, 7, EVarType::Signed },
		{ "Gain Adjust"				, "7_24", 4, 0, 7, EVarType::Signed },
		{ "Source"					, "8_24", 11, 0, 7, EVarType::Unsigned },
		{ "Amount"					, "9_24", 12, 0, 7, EVarType::Signed },
		{ "Wet/Dry"					, "10_24", 8, 0, 6, EVarType::Unsigned  },
		{ "Source"					, "11_24", 9, 0, 4, EVarType::Unsigned  },
		{ "Amt"						, "12_24", 10, 0, 7, EVarType::Signed } };

	// TODO [05] : no example in IFXs

	effect_conversions[6] = { // OD/Hi.Gain Wah
		{ "Wah"						, "0_24", 11 , 0, 0, EVarType::Unsigned},
		{ "WahSource"				, "1_24", 11 , 1, 5, EVarType::Unsigned},
		{ "WahSw"					, "2_24", 3 , 0, 0, EVarType::Unsigned},
		{ "Wah Sweep Range"			, "3_24", 10 , 3, 7, EVarType::Signed},
		{ "Wah Sweep Src"			, "4_24", 10 , 0, 2, EVarType::Unsigned, 11, 6, 7},
		{ "Drive Mode"				, "5_24", 12 , 0, 0, EVarType::Unsigned},
		{ "Drive"					, "6_24", 13 , 0, 6, EVarType::Unsigned},
		{ "Pre Low-cut"				, "7_24", 3 , 1, 4, EVarType::Unsigned},
		{ "Output Level"			, "8_24", 2 , 0, 4, EVarType::Unsigned, 3, 7, 7 },
		{ "OutputSource"			, "9_24", 9 , 0, 4, EVarType::Unsigned},
		{ "OutputAmount"			, "10_24", 14 , 0, 6, EVarType::Signed},
		{ "Low CutOff Hz"			, "11_24", 1 , 0, 3, EVarType::Unsigned, 2, 6, 7 },
		{ "Low CutOff Gain"			, "12_24", 0 , 0, 1, EVarType::Signed, 1, 4, 7 },
		{ "Mid1 Cutoff Hz"			, "13_24", 0 , 2, 7, EVarType::Unsigned},
		{ "Mid1 Cutoff Q"			, "14_24", 7 , 0, 5, EVarType::Unsigned},
		{ "Mid1 Cutoff Gain"		, "15_24", 6 , 0, 3, EVarType::Signed, 7, 6, 7 },
		{ "Mid2 Cutoff Hz"			, "16_24", 5 , 0, 1, EVarType::Unsigned, 6, 4, 7 },
		{ "Mid2 Cutoff Q"			, "17_24", 5 , 2, 7, EVarType::Unsigned},
		{ "Mid2 Cutoff Gain"		, "18_24", 8 , 0, 2, EVarType::Signed, 9, 5, 7 },
		{ "Direct Mix"				, "19_24", 12 , 1, 6, EVarType::Unsigned},
		{ "Speaker Simulation"		, "20_24", 4 , 0, 0, EVarType::Unsigned},
		{ "WetDry"					, "21_24", 4 , 1, 7, EVarType::Unsigned},
		{ "WetDrySource"			, "22_24", 8 , 3, 7, EVarType::Unsigned},
		{ "WetDryAmount"			, "23_24", 15 , 0, 7, EVarType::Signed} };

	effect_conversions[7] = { // St. Parametric 4EQ
		{ "Trim"					, "0_24",  3 ,  0, 6, EVarType::Unsigned},
		{ "Band1 Type"				, "1_24",  9 ,  0, 0, EVarType::Unsigned},
		{ "Band4 Type"				, "2_24",  10,  0, 0, EVarType::Unsigned},
		{ "Band2 DynGain"			, "3_24",  2 ,  0, 3, EVarType::Unsigned, 3, 7, 7 },
		{ "Band2 Amount"			, "4_24",  1 ,  0, 1, EVarType::Signed, 2, 4, 7 },
		{ "Band1 Cutoff"			, "5_24",  7 ,  0, 5, EVarType::Unsigned},
		{ "Band1 Q"					, "6_24",  0 ,  0, 0, EVarType::Unsigned, 1, 2, 7 },
		{ "Band1 Gain"				, "7_24",  0 ,  1, 7, EVarType::Signed},
		{ "Band2 Cutoff"			, "8_24",  8 ,  0, 7, EVarType::Unsigned},
		{ "Band2 Q"					, "9_24",  6 ,  0, 4, EVarType::Unsigned, 7, 6, 7 },
		{ "Band2 Gain"				, "10_24", 5 ,  0, 3, EVarType::Signed, 6, 5, 7 },
		{ "Band3 Cutoff"			, "11_24", 4 ,  0, 2, EVarType::Unsigned, 5, 4, 7 },
		{ "Band3 Q"					, "12_24", 9 ,  1, 7, EVarType::Unsigned},
		{ "Band3 Gain"				, "13_24", 10 , 1, 7, EVarType::Signed},
		{ "Band4 Cutoff"			, "14_24", 11 , 0, 7, EVarType::Unsigned},
		{ "Band4 Q"					, "15_24", 12 , 0, 6, EVarType::Unsigned},
		{ "Band4 Gain"				, "16_24", 13 , 0, 6, EVarType::Signed},
		{ "WetDry"					, "17_24", 14 , 0, 6, EVarType::Unsigned},
		{ "Source"					, "18_24", 4  , 3, 7, EVarType::Unsigned},
		{ "Amount"					, "19_24", 15 , 0, 7, EVarType::Signed} };

	effect_conversions[8] = { // St. Graphic 7EQ
		 { "Type"					, "0_24", 0, 0, 7, EVarType::Unsigned },
		 { "Trim"					, "1_24", 1, 0, 7, EVarType::Unsigned },
		 { "Bank1 dB"				, "2_24", 2, 0, 7, EVarType::Signed },
		 { "Bank2 dB"				, "3_24", 3, 0, 7, EVarType::Signed },
		 { "Bank3 dB"				, "4_24", 4, 0, 7, EVarType::Signed },
		 { "Bank4 dB"				, "5_24", 5, 0, 7, EVarType::Signed },
		 { "Bank5 dB"				, "6_24", 6, 0, 7, EVarType::Signed },
		 { "Bank6 dB"				, "7_24", 7, 0, 7, EVarType::Signed },
		 { "Bank7 dB"				, "8_24", 8, 0, 7, EVarType::Signed },
		 { "Wet/Dry"				, "9_24", 9, 0, 7, EVarType::Unsigned },
		 { "Source"					, "10_24", 10, 0, 4, EVarType::Unsigned },
		 { "Amt"					, "11_24", 11, 0, 7, EVarType::Signed } };

	effect_conversions[13] = { // Talking Modulator
		{ "SweepMode"				, "0_24",  0 , 0, 0, EVarType::Unsigned},
		{ "ManualVoiceControl"		, "1_24",  0 , 1, 7, EVarType::Unsigned},
		{ "ManualVoiceSource"		, "2_24",  1 , 0, 4, EVarType::Unsigned},
		{ "VoiceTop"				, "11_24", 1 , 5, 7, EVarType::Unsigned},
		{ "VoiceCenter"				, "12_24", 2 , 0, 2, EVarType::Unsigned},
		{ "VoiceBottom"				, "13_24", 2 , 3, 5, EVarType::Unsigned},
		{ "LFOFrequency"			, "3_24",  3 , 0, 6, EVarType::Unsigned},
		{ "LFOSource"				, "4_24",  4 , 0, 4, EVarType::Unsigned},
		{ "LFOAmount"				, "5_24",  5 , 0, 7, EVarType::Signed},
		{ "BPMMidiSync"				, "6_24",  13 ,0, 0, EVarType::Unsigned},
		{ "BPMVal"					, "7_24",  14, 0, 7, EVarType::Signed},
		{ "BaseNote"				, "8_24",  15, 0, 2, EVarType::Unsigned},
		{ "Times"					, "9_24",  15, 3, 6, EVarType::Unsigned},
		// 10_24 is unused
		{ "FormantShift"			, "14_24", 6 , 0, 7, EVarType::Signed},
		{ "Resonance"				, "15_24", 7 , 0, 6, EVarType::Unsigned},
		{ "WetDry"					, "16_24", 13, 1, 7, EVarType::Unsigned},
		{ "WetDrySource"			, "17_24", 8 , 0, 4, EVarType::Unsigned},
		{ "WetDryAmount"			, "18_24", 9 , 0, 7, EVarType::Signed} };

	effect_conversions[14] = { // Stereo Decimator
		{ "PreLPF"					, "0_24",  4 , 0, 0, EVarType::Unsigned},
		{ "HighDamp"				, "4_24",  4 , 1, 7, EVarType::Unsigned},
		{ "SamplingFreq"			, "1_24",  10, 0, 0, EVarType::Unsigned, 11, 0, 7},
		{ "Source1"					, "2_24",  1 , 0, 4, EVarType::Unsigned},
		{ "Amount1"					, "3_24",  12, 0, 0, EVarType::Signed, 13, 0, 7},
		{ "LFOFreq"					, "12_24",  8 , 0, 6, EVarType::Unsigned},
		{ "Source2"					, "13_24",  2 , 0, 1, EVarType::Unsigned, 3, 5, 7},
		{ "Amount2"					, "14_24",  9,  0, 7, EVarType::Signed},
		{ "Depth"					, "15_24",  12, 1, 7, EVarType::Unsigned},
		{ "Source3"					, "16_24",  2 , 2, 6, EVarType::Unsigned},
		{ "Amount3"					, "17_24", 15, 0, 7, EVarType::Signed},
		{ "Resolution"				, "5_24", 0 , 0, 1, EVarType::Unsigned, 1, 5, 7},
		{ "OutputLevel"				, "6_24", 5 , 0, 6, EVarType::Unsigned},
		{ "Source4"					, "7_24", 0 , 3, 7, EVarType::Unsigned},
		{ "Amount4"					, "8_24", 6 , 0, 7, EVarType::Signed},
		{ "WetDry"					, "9_24", 10, 1, 7, EVarType::Unsigned},
		{ "WetDrySource"			, "10_24", 3 , 0, 4, EVarType::Unsigned},
		{ "WetDryAmount"			, "11_24", 7 , 0, 7, EVarType::Signed}
	};

	effect_conversions[16] = { // Stereo Chorus
		{ "LFO Waveform"			, "0_24",  12, 0, 0, EVarType::Unsigned },
		{ "LFO Phase"				, "1_24",  12, 1, 6, EVarType::Signed },
		{ "LFO Frequency"			, "2_24",  0,  0, 7, EVarType::Unsigned },
		{ "Src"						, "3_24",  11, 0, 4, EVarType::Unsigned },
		{ "Amt"						, "4_24",  1,  0, 7, EVarType::Signed },
		{ "BPM/Midi Sync"			, "5_24",  13, 0, 0, EVarType::Unsigned },
		{ "BPM"						, "6_24",  14, 0, 7, EVarType::Unsigned },
		{ "Base Note"				, "7_24",  15, 0, 2, EVarType::Unsigned },
		{ "Times"					, "8_24",  15, 3, 7, EVarType::Unsigned },
		{ "L Pre Delay"				, "10_24", 2,  0, 7, EVarType::Unsigned },
		{ "R Pre Delay"				, "11_24", 3,  0, 7, EVarType::Unsigned },
		{ "Depth"					, "12_24", 4,  0, 7, EVarType::Unsigned },
		{ "Src"						, "13_24", 10, 0, 1, EVarType::Unsigned, 11, 5, 7 },
		{ "Amt"						, "14_24", 5,  0, 7, EVarType::Signed },
		{ "EQ Trim"					, "15_24", 13, 1, 7, EVarType::Unsigned},
		{ "Pre LEQ Gain"			, "16_24", 10, 2, 7, EVarType::Signed },
		{ "Pre HEQ Gain"			, "17_24", 6,  0, 7, EVarType::Signed },
		{ "Wet/Dry"					, "18_24", 7,  0, 7, EVarType::Unsigned },
		{ "Source"					, "19_24", 8,  0, 4, EVarType::Unsigned },
		{ "Amt"						, "20_24", 9,  0, 7, EVarType::Signed} };

	effect_conversions[17] = { // St. HarmonicChorus
		{ "LFOWaveform"				, "0_24", 3 , 0, 0, EVarType::Unsigned},
		{ "LFOPhase"				, "1_24", 3 , 1, 5, EVarType::Signed},
		{ "LFOFreqVal"				, "2_24", 8 , 0, 7, EVarType::Unsigned},
		{ "LFOFreqSource"			, "3_24", 2 , 0, 2, EVarType::Unsigned, 3, 6, 7},
		{ "LFOFreqAmount"			, "4_24", 9 , 0, 7, EVarType::Signed},
		{ "BPMMidiSync"				, "5_24", 13 , 0, 0, EVarType::Unsigned},
		{ "BPMVal"					, "6_24", 14 , 0, 7, EVarType::Unsigned},
		{ "BaseNote"				, "7_24", 15 , 0, 2, EVarType::Unsigned},
		{ "Times"					, "8_24", 15 , 3, 6, EVarType::Unsigned},
		{ "PreDelay"				, "10_24", 10 , 0, 7, EVarType::Unsigned},
		{ "DepthVal"				, "11_24", 1 , 0, 1, EVarType::Unsigned, 2, 3, 7},
		{ "DepthSource"				, "12_24", 7 , 0, 4, EVarType::Unsigned},
		{ "DepthAmount"				, "13_24", 11 , 0, 7, EVarType::Signed},
		{ "HighLowSplitPoint"		, "14_24", 0 , 0, 0, EVarType::Unsigned, 1, 2, 7},
		{ "Feedback"				, "15_24", 12 , 0, 7, EVarType::Signed},
		{ "HighDamp"				, "16_24", 0 , 1, 7, EVarType::Unsigned},
		{ "LowLevel"				, "17_24", 6 , 0, 3, EVarType::Unsigned, 7, 5, 7},
		{ "HighLevel"				, "18_24", 5 , 0, 2, EVarType::Unsigned, 6, 4, 7},
		{ "WetDry"					, "19_24", 13 , 1, 7, EVarType::Unsigned},
		{ "WetDrySource"			, "20_24", 5 , 3, 7, EVarType::Unsigned},
		{ "WetDryAmount"			, "21_24", 4 , 0, 7, EVarType::Signed} };

	effect_conversions[18] = { // Multitap Cho/Delay
		{ "LFO Frequency"			, "0_24",  12, 0, 2, EVarType::Unsigned, 13, 5, 7 },
		{ "Tap1Val"					, "1_24",  14, 0, 5, EVarType::Unsigned, 15, 7, 7 },
		{ "Tap1Depth"				, "2_24",  10, 0, 1, EVarType::Unsigned, 11, 5, 7 },
		{ "Tap1Level"				, "3_24",  8 , 3, 7, EVarType::Unsigned},
		{ "Tap1Pan"					, "4_24",  3 , 4, 7, EVarType::Signed},
		{ "Tap2Val"					, "5_24",  8 , 0, 2, EVarType::Unsigned, 9, 4, 7 },
		{ "Tap2Depth"				, "6_24",  9 , 0, 3, EVarType::Unsigned, 10, 7, 7 },
		{ "Tap2Level"				, "7_24",  6 , 0, 1, EVarType::Unsigned, 7, 5, 7 },
		{ "Tap2Pan"					, "8_24",  4 , 4, 7, EVarType::Signed},
		{ "Tap3Val"					, "9_24",  13, 0, 4, EVarType::Unsigned, 14, 6, 7 },
		{ "Tap3Depth"				, "10_24", 10, 2, 6, EVarType::Unsigned},
		{ "Tap3Level"				, "11_24", 7 , 0, 4, EVarType::Unsigned},
		{ "Tap3Pan"					, "12_24", 4 , 0, 3, EVarType::Signed},
		{ "Tap4Val"					, "13_24", 15, 0, 6, EVarType::Unsigned},
		{ "Tap4Depth"				, "14_24", 11, 0, 4, EVarType::Unsigned},
		{ "Tap4Level"				, "15_24", 12, 3, 7, EVarType::Unsigned},
		{ "Tap4Pan"					, "16_24", 3 , 0, 3, EVarType::Signed},
		{ "Tap1Feedback"			, "17_24", 0 , 0, 7, EVarType::Signed},
		{ "Tap1Amount"				, "18_24", 1 , 0, 7, EVarType::Signed},
		{ "WetDryVal"				, "19_24", 5 , 0, 5, EVarType::Unsigned, 6, 7, 7 },
		{ "Source"					, "20_24", 6 , 2, 6, EVarType::Unsigned},
		{ "WetDryAmount"			, "21_24", 2 , 0, 7, EVarType::Signed} };

	effect_conversions[19] = { 	// Ensemble
		 { "Speed"					, "0_24", 0, 0, 6, EVarType::Unsigned },
		 { "Source"					, "1_24", 1, 0, 4, EVarType::Unsigned },
		 { "Amount"					, "2_24", 2, 0, 7, EVarType::Signed },
		 { "Depth"					, "3_24", 3, 0, 6, EVarType::Unsigned },
		 { "Source"					, "4_24", 4, 0, 4, EVarType::Unsigned },
		 { "Amount"					, "5_24", 5, 0, 7, EVarType::Signed },
		 { "Shimmer"				, "6_24", 6, 0, 6, EVarType::Unsigned },
		 { "Wet/Dry"				, "7_24", 7, 0, 7, EVarType::Unsigned },
		 { "Source"					, "8_24", 8, 0, 4, EVarType::Unsigned },
		 { "Amt"					, "9_24", 9, 0, 7, EVarType::Signed } };

	effect_conversions[23] = { // Stereo Phaser
		{ "LFO Waveform"			, "0_24",  12, 0, 0, EVarType::Unsigned},
		{ "LFO Shape"				, "1_24",  0 , 0, 7, EVarType::Signed},
		{ "LFO Phase"				, "2_24",  12, 1, 5, EVarType::Signed},
		{ "LFO Frequency"			, "3_24",  1 , 0, 6, EVarType::Unsigned},
		{ "LFO Source"				, "4_24",  2 , 0, 5, EVarType::Unsigned},
		{ "LFO Amount"				, "5_24",  3 , 0, 7, EVarType::Signed},
		{ "BPM Midi Sync"			, "6_24",  13, 0, 0, EVarType::Unsigned},
		{ "BPM Val"					, "7_24",  14, 0, 7, EVarType::Signed},
		{ "Base Note"				, "8_24",  15, 0, 2, EVarType::Unsigned},
		{ "Times"					, "9_24",  15, 3, 6, EVarType::Unsigned},
		// 10_24 is unused for some reason
		{ "Manual"					, "11_24", 4 , 0, 6, EVarType::Unsigned},
		{ "Depth Val"				, "12_24", 5 , 0, 6, EVarType::Unsigned},
		{ "Depth Source"			, "13_24", 6 , 0, 4, EVarType::Unsigned},
		{ "Depth Amount"			, "14_24", 7 , 0, 7, EVarType::Signed},
		{ "Resonance"				, "15_24", 8 , 0, 7, EVarType::Signed},
		{ "High Damp"				, "16_24", 13, 1, 7, EVarType::Unsigned},
		{ "WetDry"					, "17_24", 9 , 0, 6, EVarType::Unsigned},
		{ "WetDrySource"			, "18_24", 10, 0, 4, EVarType::Unsigned},
		{ "WetDryAmount"			, "19_24", 11, 0, 7, EVarType::Signed} };

	// [34] Stereo Auto Pan  		// TODO IN PROGRESS
	effect_conversions[34] = {
		{ "LFO Waveform"			, "0_24",  12, 0, 0, EVarType::Unsigned},
		{ "LFO Shape"				, "1_24",  0 , 0, 7, EVarType::Signed},
		{ "LFO Phase"				, "2_24",  12, 1, 5, EVarType::Signed},
		{ "LFO Frequency"			, "3_24",  1 , 0, 6, EVarType::Unsigned},
		{ "LFO Source"				, "4_24",  9 , 3, 6, EVarType::Unsigned},
		{ "LFO Amount"				, "5_24",  2 , 0, 7, EVarType::Signed},
		{ "BPM Midi Sync"			, "6_24",  13, 0, 0, EVarType::Unsigned},
		{ "BPM Val"					, "7_24",  14, 0, 7, EVarType::Signed},
		{ "Base Note"				, "8_24",  15, 0, 2, EVarType::Unsigned},
		{ "Times"					, "9_24",  15, 3, 6, EVarType::Unsigned},
		// 10_24 is unused for some reason
		{ "Depth Val"				, "11_24", 3 , 0, 6, EVarType::Unsigned},
		{ "Depth Source"			, "12_24", 4 , 0, 4, EVarType::Unsigned},
		{ "Depth Amount"			, "13_24", 5 , 0, 7, EVarType::Signed},
		{ "WetDry"					, "14_24", 6,  0, 6, EVarType::Unsigned},
		{ "WetDrySource"			, "15_24", 7,  0, 4, EVarType::Unsigned},
		{ "WetDryAmount"			, "16_24", 8,  0, 7, EVarType::Signed} };

	effect_conversions[36] = { // St. Ring Modulator
		{ "PreLPF"					, "0_24", 6 , 0, 6, EVarType::Unsigned},
		{ "OscMode"					, "1_24", 6 , 7, 7, EVarType::Unsigned},
		{ "FixedFreqVal"			, "2_24", 7 , 0, 7, EVarType::Unsigned},
		{ "FixedFreqSource"			, "3_24", 3 , 0, 4, EVarType::Unsigned},
		{ "FixedFreqAmount"			, "4_24", 2 , 0, 4, EVarType::Signed, 3, 5, 7},
		{ "NoteOffset"				, "5_24", 1 , 0, 3, EVarType::Signed, 2, 5, 7},
		{ "Unknown Param"			, "6_24", 0 , 0, 0, EVarType::Unsigned, 1, 4, 7}, // Ghost param that is not in the GUI
		{ "NoteFine"				, "7_24", 8 , 0, 7, EVarType::Signed},
		{ "LFOFreqVal"				, "8_24", 9 , 0, 7, EVarType::Unsigned},
		{ "LFOFreqSource"			, "9_24", 5 , 0, 4, EVarType::Unsigned},
		{ "LFOFreqAmount"			, "10_24", 10 , 0, 7, EVarType::Signed},
		{ "BPMMidiSync"				, "11_24", 13 , 0, 0, EVarType::Unsigned},
		{ "BPMVal"					, "12_24", 14 , 0, 7, EVarType::Unsigned},
		{ "BaseNote"				, "13_24", 15 , 0, 2, EVarType::Unsigned},
		{ "Times"					, "14_24", 15 , 3, 6, EVarType::Unsigned},
		{ "LFODepthVal"				, "16_24", 13 , 1, 7, EVarType::Unsigned},
		{ "LFODepthSource"			, "17_24", 4 , 0, 1, EVarType::Unsigned, 5, 5, 7},
		{ "LFODepthAmount"			, "18_24", 11 , 0, 7, EVarType::Signed},
		{ "WetDry"					, "19_24", 0 , 1, 7, EVarType::Unsigned},
		{ "WetDrySource"			, "20_24", 4 , 2, 6, EVarType::Unsigned},
		{ "WetDryAmount"			, "21_24", 12 , 0, 7, EVarType::Signed},
	};

	effect_conversions[41] = { // Early Reflections
		{ "Type"					, "0_24",  0, 0, 1, EVarType::Unsigned},
		{ "ER Time"					, "1_24",  1, 0, 7, EVarType::Unsigned},
		{ "Pre Delay"				, "2_24",  2, 0, 7, EVarType::Unsigned},
		{ "EQ Trim"					, "3_24",  3, 0, 6, EVarType::Unsigned},
		{ "Pre LEQ Gain"			, "4_24",  4, 0, 7, EVarType::Signed},
		{ "Pre HEQ Gain"			, "5_24",  5, 0, 7, EVarType::Signed},
		{ "WetDry"					, "6_24",  6, 0, 6, EVarType::Unsigned},
		{ "WetDrySource"			, "7_24",  7, 0, 4, EVarType::Unsigned},
		{ "WetDryAmount"			, "8_24",  8, 0, 7, EVarType::Signed} };

	
	effect_conversions[43] = { // L/C/R Delay
		{ "L Delay Time"			, "0_24",  2,  0, 3, EVarType::Unsigned, 3, 0, 7 }, // 12-bit val
		{ "L Level"					, "1_24",  7,  0, 5, EVarType::Unsigned},
		{ "C Delay Time"			, "2_24",  1,  0, 7, EVarType::Unsigned, 2, 4, 7 }, // 12-bit val
		{ "C Level"					, "3_24",  6,  0, 3, EVarType::Unsigned, 7, 6, 7 },
		{ "R Delay Time"			, "4_24",  5,  0, 7, EVarType::Unsigned, 6, 4, 7 }, // 12-bit val
		{ "R Level"					, "5_24",  11, 0, 5, EVarType::Unsigned},
		{ "Feedback C Delay"		, "6_24",  0 , 0, 7, EVarType::Signed},
		{ "FeedbackSource"			, "7_24",  10, 0, 2, EVarType::Unsigned, 11, 6, 7 },
		{ "Amount"					, "8_24",  4 , 0, 7, EVarType::Signed},
		{ "High Damp"				, "9_24",  9 , 0, 1, EVarType::Unsigned, 10, 3, 7 },
		{ "Low Damp"				, "10_24", 15, 0, 6, EVarType::Unsigned},
		{ "Input Level Dmod"		, "11_24", 8 , 0, 7, EVarType::Signed},
		{ "InputLevelSource "		, "12_24", 14, 0, 3, EVarType::Unsigned, 15, 7, 7 },
		{ "Spread"					, "13_24", 9 , 2, 7, EVarType::Unsigned},
		{ "WetDry"					, "14_24", 13, 0, 2, EVarType::Unsigned, 14, 4, 7 },
		{ "WetDrySource"			, "15_24", 13, 3, 7, EVarType::Unsigned},
		{ "WetDryAmount"			, "16_24", 12, 0, 7, EVarType::Signed} };

	effect_conversions[50] = { // St. BPM Delay
		{ "BPMVal"					, "0_24", 4 ,  0, 6, EVarType::Unsigned},
		{ "L Delay BaseNote"		, "2_24", 5 ,  0, 2, EVarType::Unsigned},
		{ "L Delay Times"			, "3_24", 5 ,  3, 6, EVarType::Unsigned},
		{ "L Delay Adjust"			, "4_24", 10,  0, 7, EVarType::Signed, 11, 7, 7},
		{ "R Relay BaseNote"		, "6_24", 6 ,  0, 2, EVarType::Unsigned},
		{ "R Relay Times"			, "7_24", 6 ,  3, 6, EVarType::Unsigned},
		{ "R Relay Adjust"			, "8_24", 12,  0, 7, EVarType::Signed, 13, 7, 7},
		{ "L Feedback"				, "13_24", 0,  0, 7, EVarType::Signed},
		{ "L FeedbackSource"		, "15_24", 15,  0, 4, EVarType::Unsigned},
		{ "Amount L"				, "16_24", 2 ,  0, 7, EVarType::Signed},
		{ "R Feedback"				, "14_24", 1 , 0, 7, EVarType::Signed},
		{ "Amount R"				, "17_24", 3 , 0, 7, EVarType::Signed},
		{ "HighDamp"				, "18_24", 11, 0, 6, EVarType::Unsigned},
		{ "LowDamp"					, "19_24", 13, 0, 6, EVarType::Unsigned},
		{ "InputLevelDmod"			, "21_24", 7 , 0, 7, EVarType::Signed},
		{ "InputLevelSource"		, "20_24", 14, 0, 1, EVarType::Unsigned, 15, 5, 7},
		{ "WetDry"					, "10_24", 9 , 0, 6, EVarType::Unsigned},
		{ "WetDrySource"			, "11_24", 14, 2, 6, EVarType::Unsigned},
		{ "WetDryAmount"			, "12_24", 8 , 0, 7, EVarType::Signed} };

	effect_conversions[51] = { // Sequence Delay
		{ "BPMVal"					, "0_24",  0 , 0, 7, EVarType::Unsigned},
		{ "RhythmPattern"			, "2_24",  1 , 0, 4, EVarType::Unsigned},
		{ "Tap1Pan"					, "13_24", 11, 0, 6, EVarType::Unsigned},
		{ "Tap2Pan"					, "14_24", 10, 0, 5, EVarType::Unsigned, 11, 7, 7},
		{ "Tap3Pan"					, "15_24", 9 , 0, 4, EVarType::Unsigned, 10, 6, 7},
		{ "Tap4Pan"					, "16_24", 8 , 0, 3, EVarType::Unsigned, 9, 5, 7},
		{ "Feedback"				, "3_24",  2 , 0, 7, EVarType::Signed},
		{ "FeedbackSource"			, "11_24", 14, 2, 6, EVarType::Unsigned},
		{ "FeedbackAmount"			, "5_24",  3 , 0, 7, EVarType::Signed},
		{ "HighDamp"				, "6_24",  6 , 1, 7, EVarType::Unsigned},
		{ "LowDamp"					, "7_24",  7 , 1, 7, EVarType::Unsigned},
		{ "InputLevelDmod"			, "9_24",  4 , 0, 7, EVarType::Signed},
		{ "InputLevelSource"		, "8_24",  14, 0, 1, EVarType::Unsigned, 15, 5, 7},
		{ "WetDry"					, "10_24", 13, 0, 6, EVarType::Unsigned},
		{ "WetDrySource"			, "4_24",  15, 0, 4, EVarType::Unsigned},
		{ "WetDryAmount"			, "12_24", 5 , 0, 7, EVarType::Signed},
	};

	effect_conversions[52] =   // Reverb Hall
	effect_conversions[53] =   // Reverb SmoothHall
	effect_conversions[54] =   // Reverb Wet Plate
	effect_conversions[55] = { // Reverb Dry Plate
		{ "Reverb Time"				, "0_24",  0,  0, 6, EVarType::Unsigned},
		{ "High Damp"				, "1_24",  1,  0, 7, EVarType::Unsigned},
		{ "Pre Delay"				, "2_24",  2,  0, 7, EVarType::Unsigned},
		{ "Pre Delay Thru"			, "3_24",  3,  0, 6, EVarType::Unsigned},
		{ "EQ Trim"					, "4_24",  6,  0, 6, EVarType::Unsigned},
		{ "Pre LEQ Gain"			, "5_24",  7,  0, 4, EVarType::Signed},
		{ "Pre HEQ Gain"			, "6_24",  8,  0, 4, EVarType::Signed},
		{ "WetDry"					, "7_24",  9,  0, 6, EVarType::Unsigned},
		{ "WetDrySource"			, "8_24",  10, 0, 4, EVarType::Unsigned},
		{ "WetDryAmount"			, "9_24",  11, 0, 7, EVarType::Signed} };

	effect_conversions[56] = { // Reverb Room
		{ "Reverb Time"				, "0_24",  0,  0, 6, EVarType::Unsigned},
		{ "High Damp"				, "1_24",  1,  0, 7, EVarType::Unsigned},
		{ "Pre Delay"				, "2_24",  2,  0, 7, EVarType::Unsigned},
		{ "Pre Delay Thru"			, "3_24",  3,  0, 6, EVarType::Unsigned},
		{ "ER Level"				, "4_24",  4,  0, 6, EVarType::Unsigned},
		{ "Reverb Level"			, "5_24",  5,  0, 6, EVarType::Unsigned},
		{ "EQ Trim"					, "6_24",  6,  0, 6, EVarType::Unsigned},
		{ "Pre LEQ Gain"			, "7_24",  7,  0, 4, EVarType::Signed},
		{ "Pre HEQ Gain"			, "8_24",  8,  0, 4, EVarType::Signed},
		{ "WetDry"					, "9_24",  9,  0, 6, EVarType::Unsigned},
		{ "WetDrySource"			, "10_24", 10, 0, 4, EVarType::Unsigned},
		{ "WetDryAmount"			, "11_24", 11, 0, 7, EVarType::Signed} };

	effect_conversions[90] = { // PianoBody/Damper Simulation
		{ "Sound Board Depth"		, "0_24", 0, 0, 7, EVarType::Unsigned },
		{ "Damper Depth"			, "1_24", 1, 0, 7, EVarType::Unsigned },
		{ "Src"						, "2_24", 2, 0, 7, EVarType::Unsigned },
		{ "Tone"					, "3_24", 3, 0, 7, EVarType::Unsigned },
		{ "Mid Shape"				, "4_24", 4, 0, 7, EVarType::Unsigned },
		{ "Tune"					, "8_24", 8, 0, 3, EVarType::Signed },
		{ "Wet/Dry"					, "5_24", 5, 0, 6, EVarType::Unsigned },
		{ "Source"					, "6_24", 6, 0, 4, EVarType::Unsigned },
		{ "Amt"						, "7_24", 7, 0, 7, EVarType::Signed } };

	auto handledItems = effect_conversions.size();
}
