#include "pcg_converter.h"

std::vector<SubParam> PCG_Converter::program_ifx_offsets = {
	{ 1, 16 }, { 2, 40 }, { 3, 64 }, { 4, 88 }, { 5, 112 }
};

std::vector<TritonStruct> PCG_Converter::program_ifx_conversions = {
	{ "IFX1 Effect Type"							, "effect_type", 16, 0, 7 },
	{ "IFX1 On/Off"									, "onoff", 17, 6, 6 },
	{ "IFX1 Chain"									, "chain", 17, 7, 7 },
	{ "IFX1 Pan"									, "pan", 20, 0, 7 },
	{ "IFX1 Routing"								, "bus_select", 21, 0, 7 },
	{ "IFX1 Send 1"									, "send_1_level", 22, 0, 7 },
	{ "IFX1 Send 2"									, "send_2_level", 23, 0, 7 },
};

std::vector<IfxType> PCG_Converter::effect_conversions = {
	{ 6, "OD/Hi.Gain Wah", {
		{ "Sw"						, "", 3, 0, 0 }, // 19 Sw: byte 0: toggle(0)/moment(1)
		{ "Wah On/Off"				, "", 11, 0, 0 }, // 27 Wah On/Off
		{ "Src"						, "", 11, 1, 5 } } // 27 Src dropdown
	},
	{ 8, "St. Graphic 7EQ", {
		{ "Type"					, "specific_parameter_0_24", 0, 0, 7 }, // 16 dropdown val -1 (ex: 0x03 -> 4 in dropdown)
		{ "Trim"					, "specific_parameter_1_24", 1, 0, 7 }, // 17
		{ "Bank1 dB"				, "specific_parameter_2_24", 2, 0, 7 }, // 18
		{ "Bank2 dB"				, "specific_parameter_3_24", 3, 0, 7 }, // 19
		{ "Bank3 dB"				, "specific_parameter_4_24", 4, 0, 7 }, // 20
		{ "Bank4 dB"				, "specific_parameter_5_24", 5, 0, 7 }, // 21
		{ "Bank5 dB"				, "specific_parameter_6_24", 6, 0, 7 }, // 22
		{ "Bank6 dB"				, "specific_parameter_7_24", 7, 0, 7 }, // 23
		{ "Bank7 dB"				, "specific_parameter_8_24", 8, 0, 7 }, // 24
		{ "Wet/Dry"					, "specific_parameter_9_24", 9, 0, 7 }, // 25
		{ "Source"					, "specific_parameter_10_24", 10, 0, 7 }, // 26
		{ "Amt"						, "specific_parameter_11_24", 11, 0, 7 } }// 27
	},
	{ 16, "Stereo Chorus", {
		{ "LFO Waveform"				, "specific_parameter_0_24", 12, 0, 0 }, // 148
		{ "LFO Phase"					, "specific_parameter_1_24", 12, 1, 5 }, // 148
		{ "LFO Frequency"				, "specific_parameter_2_24", 0, 0, 7 }, // 136
		{ "Src"							, "specific_parameter_3_24", 11, 0, 4 }, // 147
		{ "Amt"							, "specific_parameter_4_24", 1, 0, 7 }, // 137
		{ "BPM/Midi Sync"				, "specific_parameter_5_24", 13, 0, 0 }, // 149
		{ "BPM"							, "specific_parameter_6_24", 14, 0, 7 }, // 150
		{ "Base Note"					, "specific_parameter_7_24", 15, 0, 2 }, // 151
		{ "Times"						, "specific_parameter_8_24", 15, 3, 7 }, // 151
		{ "L Pre Delay"					, "specific_parameter_10_24", 2, 0, 7 }, // 138
		{ "R Pre Delay"					, "specific_parameter_11_24", 3, 0, 7 }, // 139
		{ "Depth"						, "specific_parameter_12_24", 4, 0, 7 }, // 140
		{ "Src"							, "specific_parameter_13_24", 10, 0, 1, 11, 5, 7, 3 }, // 147
		{ "Amt"							, "specific_parameter_14_24", 5, 0, 7 }, // 141
		{ "EQ Trim"						, "specific_parameter_15_24", 13, 1, 7 }, // 149
		{ "Pre LEQ Gain"				, "specific_parameter_16_24", 10, 2, 7 }, // 146
		{ "Pre HEQ Gain"				, "specific_parameter_17_24", 6, 0, 7 }, // 142
		{ "Wet/Dry"						, "specific_parameter_18_24", 7, 0, 7 }, // 143
		{ "Source"						, "specific_parameter_19_24", 8, 0, 7 }, // 144
		{ "Amt"							, "specific_parameter_20_24", 9, 0, 7 } } // 145
	},
	{ 19, "Ensemble", {
		{ "Speed"						, "specific_parameter_0_24", 0, 0, 7 }, // 136
		{ "Src"							, "specific_parameter_1_24", 1, 0, 7 }, // todo double check
		{ "Amt"							, "specific_parameter_2_24", 2, 0, 7 }, // todo double check
		{ "Depth"						, "specific_parameter_3_24", 3, 0, 7 }, // 139
		{ "Src"							, "specific_parameter_4_24", 4, 0, 7 }, // todo double check
		{ "Amt"							, "specific_parameter_5_24", 5, 0, 7 }, // todo double check
		{ "Shimmer"						, "specific_parameter_6_24", 6, 0, 7 } } // 142

		//{ "Wet/Dry"					, "specific_parameter_7_24", 9, 0, 7 }, // todo double check
		//{ "Source"					, "specific_parameter_8_24", 10, 0, 7 }, // todo double check
		//{ "Amt"						, "specific_parameter_9_24", 11, 0, 7 }  } // todo double check
	},
	{ 52, "ReverbHall", {
		{ "Reverb Time"				, "specific_parameter_0_24", 0, 0, 7 },
		{ "High Damp"				, "specific_parameter_1_24", 1, 0, 7 },
		{ "Pre Delay"				, "specific_parameter_2_24", 2, 0, 7 },
		{ "Pre Delay Thru"			, "specific_parameter_3_24", 3, 0, 7 },
		{ "EQ Trim"					, "specific_parameter_4_24", 6, 0, 7 },
		{ "Pre LEQ Gain"			, "specific_parameter_5_24", 7, 0, 7 },
		{ "Pre HEQ Gain"			, "specific_parameter_6_24", 8, 0, 7 },

		{ "Wet/Dry"					, "specific_parameter_7_24", 9, 0, 7 },
		{ "Source"					, "specific_parameter_8_24", 10, 0, 7 },
		{ "Amt"						, "specific_parameter_9_24", 11, 0, 7 }  }
	},
	{ 90, "PianoBody/Damper", {
		{ "Sound Board Depth"		, "specific_parameter_0_24", 0, 0, 7 },
		{ "Damper Depth"			, "specific_parameter_1_24", 1, 0, 7 },
		{ "Src"						, "specific_parameter_2_24", 2, 0, 7 },
		{ "Tone"					, "specific_parameter_3_24", 3, 0, 7 },
		{ "Mid Shape"				, "specific_parameter_4_24", 4, 0, 7 },
		{ "Tune"					, "specific_parameter_8_24", 8, 0, 7 },
		{ "Wet/Dry"					, "specific_parameter_5_24", 5, 0, 7 },
		{ "Source"					, "specific_parameter_6_24", 6, 0, 7 },
		{ "Amt"						, "specific_parameter_7_24", 7, 0, 7 } }
	},
};
