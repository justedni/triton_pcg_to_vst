#include "pcg_converter.h"
#include <iostream>

std::vector<TritonStruct> PCG_Converter::triton_extreme_prog_conversions = {
	{ "Valve Force On/Off"					, "valveforce_onoff", 34, 0, 0 },
	{ "Placement"							, "valveforce_placement", 34, 1, 1 },
	{ "Ultra Boost"							, "valveforce_ultra_boost", 59, 0, 7 },
	{ "Tube Gain"							, "valveforce_gain", 82, 0, 7 },
	{ "Output Level"						, "valveforce_output_level", 83, 0, 7 },
	{ "Input Trim"							, "valveforce_input_trim", 84, 0, 7 },
	{ "Pan"									, "valveforce_pan", 106, 0, 7 },
	{ "Send1"								, "valveforce_send1_level", 130, 0, 7 },
	{ "Send2"								, "valveforce_send2_level", 131, 0, 7 },
};

std::vector<TritonStruct> PCG_Converter::triton_extreme_combi_conversions = {
	{ "Valve Force On/Off"					, "valveforce_onoff", 34, 0, 0 },
	{ "Input Trim"							, "valveforce_input_trim", 58, 0, 7 },
};

std::vector<TritonStruct> PCG_Converter::shared_conversions = {
	// 136 : mfx1 params block start
	{ "MFX1 Effect Type"							, "mfx1_effect_type", 152, 0, 7 },
	{ "MFX1 On/Off"									, "mfx1_onoff", 153, 6, 6 },
	// 156 : mfx2 params block start
	{ "MFX2 Effect Type"							, "mfx2_effect_type", 172, 0, 7 },
	{ "MFX2 On/Off"									, "mfx2_onoff", 173, 6, 6 },
	{ "MFX1 Return1"								, "mfx1_return_level", 176, 0, 7 },
	{ "MFX2 Return2"								, "mfx2_return_level", 177, 0, 7 },
	{ "MFX Send"									, "mfx_chain_onoff", 178, 3, 3 },
	{ "MFX Chain Direction"							, "mfx_chain_direction", 178, 2, 2 },
	{ "MFX Chain Signal"							, "mfx_chain_signal", 178, 0, 1 },
	{ "MFX Chain Level"								, "mfx_chain_level", 179, 0, 7 },
	{ "Master EQ Gain Low dB"						, "master_eq_low_gain", 180, 0, 7 },
	{ "Master EQ Gain Mid dB"						, "master_eq_mid_gain", 181, 0, 7 },
	{ "Master EQ Gain High dB"						, "master_eq_high_gain", 182, 0, 7 },
	{ "Master EQ Low Cutoff"						, "master_eq_low_fc", 183, 0, 7 },
	{ "Master EQ Mid Cutoff"						, "master_eq_mid_fc", 184, 0, 7 },
	{ "Master EQ High Cutoff"						, "master_eq_high_fc", 185, 0, 7 },
	{ "Master EQ Q"									, "master_eq_mid_q", 186, 0, 7 },
	{ "Master EQ Low Gain Mod-Src"					, "master_eq_low_dmod", 187, 0, 7 },
	{ "Master EQ High Gain Mod-Src"					, "master_eq_high_dmod", 188, 0, 7 },
};

std::vector<TritonStruct> PCG_Converter::program_conversions = {
	{ "Category"							, "common_category", 206, 0, 7 },
	{ "Oscillator Mode  Oscillator Mode"	, "common_oscillator_mode", 204, 0, 1 },
	{ "Voice Assign Mode Poly/Mono"			, "", 204, 2, 2 },
	{ "Voice Assign Mode Legato"			, "common_legato", 204, 3, 3 },
	{ "Voice Assign Mode Priority"			, "common_priority", 204, 4, 5, -1, -1, -1, 8, TritonStruct::EVarType::Unsigned },
	{ "Voice Assign Mode Single Trigger"	, "common_single_trigger", 204, 6, 6 },
	{ "Voice Assign Mode Hold"				, "common_hold", 204, 7, 7 },
	{ "Bus Select All OSCs to"				, "common_bus_select", 205, 0, 6 },
	{ "Use Dkit Setting"					, "common_use_drum_kit_setting", 205, 7, 7 },
	{ "Scale Type"							, "common_scale_type", 207, 0, 7 },
	{ "Scale Key"							, "common_scale_key", 208, 0, 7 },
	{ "Scale Random"						, "common_random_intensity", 209, 0, 7 },
	{ "Panel Switch Assign SW1 Assign Type"	, "common_sw1_assign_type", 210, 0, 5 },
	{ "Panel Switch Assign SW2 Assign Type"	, "common_sw2_assign_type", 211, 0, 5 },
	{ "Switches On/Off Status  SW1"			, "common_sw1_onoff", 210, 7, 7 },
	{ "Switches On/Off Status  SW2"			, "common_sw2_onoff", 211, 7, 7 },
	{ "Panel Switch Assign SW1 Toggle/Momentary", "common_sw1_togglemomentary", 210, 6, 6 },
	{ "Panel Switch Assign SW2 Toggle/Momentary", "common_sw2_togglemomentary", 211, 6, 6 },
	{ "Realtime Control Kobs B - Assign Knob 1-B Assign Type", "common_knob1_assign_type", 212, 0, 6 },
	{ "Realtime Control Kobs B - Assign Knob 2-B Assign Type", "common_knob2_assign_type", 213, 0, 7 },
	{ "Realtime Control Kobs B - Assign Knob 3-B Assign Type", "common_knob3_assign_type", 214, 0, 7 },
	{ "Realtime Control Kobs B - Assign Knob 4-B Assign Type", "common_knob4_assign_type", 215, 0, 7 },
	{ "Switches On/Off Status  Real Time Controls", "", 212, 7, 7 },
	{ "Level Start"							, "common_pitch_eg_start_level", 216, 0, 7 },
	{ "Time Attack"							, "common_pitch_eg_attack_time", 217, 0, 7 },
	{ "Level Attack"						, "common_pitch_eg_attack_level", 218, 0, 7 },
	{ "Time Decay"							, "common_pitch_eg_decay_time", 219, 0, 7 },
	{ "Time Release"						, "common_pitch_eg_release_time", 220, 0, 7 },
	{ "Level Release"						, "common_pitch_eg_release_level", 221, 0, 7 },
	{ "Time Modulation AMS Source"			, "common_pitch_eg_time_mod_ams_source", 226, 0, 7 },
	{ "Time Modulation AMS Intensity"		, "common_pitch_eg_time_mod_ams_intensity", 227, 0, 7 },
	{ "Level Modulation 1 AMS Source"		, "common_pitch_eg_level_mod_ams1_source", 222, 0, 7 },
	{ "Level Modulation 1 AMS Intensity"	, "common_pitch_eg_level_mod_ams1_intensity", 223, 0, 7 },
	{ "Level Modulation 2 AMS Source"		, "common_pitch_eg_level_mod_ams2_source", 224, 0, 7 },
	{ "Level Modulation 2 AMS Intensity"	, "common_pitch_eg_level_mod_ams2_intensity", 225, 0, 7 },
	{ "Time Modulation AMS SW Attack"		, "common_pitch_eg_time_mod_ams_attack_direction", 229, 0, 1 },
	{ "Time Modulation AMS SW Decay"		, "common_pitch_eg_time_mod_ams_decay_direction", 229, 2, 3 },
	{ "Level Modulation 1 AMS SW Start"		, "common_pitch_eg_level_mod_ams1_start_direction", 228, 0, 1 },
	{ "Level Modulation 1 AMS SW Attack"	, "common_pitch_eg_level_mod_ams1_attack_direction", 228, 2, 3 },
	{ "Level Modulation 2 AMS SW Start"		, "common_pitch_eg_level_mod_ams2_start_direction", 228, 4, 5 },
	{ "Level Modulation 2 AMS SW Attack"	, "common_pitch_eg_level_mod_ams2_attack_direction", 228, 6, 7 },

	{ "  Tempo"								, "arpeggiator_tempo", 192, 0, 7 },
	{ "Switches On/Off Status  Arpeggiator"	, "arpeggiator_switch", 193, 0, 7 },
	{ "  Pattern"							, "arpeggiator_pattern_no.", 194, 0, 7, -1, -1, -1, 8, TritonStruct::EVarType::Unsigned },
	{ "  Resolution"						, "arpeggiator_resolution", 195, 2, 4 },
	{ "  Octave"							, "arpeggiator_octave", 195, 0, 1 },
	{ "  Gate"								, "arpeggiator_gate", 196, 0, 7 },
	{ "  Velocity"							, "arpeggiator_velocity", 197, 0, 7, -1, -1, -1, 8, TritonStruct::EVarType::Unsigned },
	{ "  Swing"								, "arpeggiator_swing", 198, 0, 7 },
	{ "  Sort"								, "arpeggiator_sort_onoff", 199, 0, 0 },
	{ "  Latch"								, "arpeggiator_latch_onoff", 199, 1, 1 },
	{ "  Key Sync"							, "arpeggiator_keysync_onoff", 199, 2, 2 },
	{ "  Keyboard"							, "arpeggiator_keyboard_onoff", 199, 3, 3 },
	{ "  Top Key"							, "arpeggiator_top_key", 200, 0, 7 },
	{ "  Bottom Key"						, "arpeggiator_bottom_key", 201, 0, 7 },
	{ "  Top Velocity"						, "arpeggiator_top_velocity", 202, 0, 7 },
	{ "  Bottom Velocity"					, "arpeggiator_bottom_velocity", 203, 0, 7 },
};

int convertOSCBank(int pcgBank, const std::string& paramName, unsigned char* data)
{
	if (paramName.find("osc_2") != std::string::npos)
	{
		TritonStruct temp = { "Oscillator Mode" , "common_oscillator_mode", 204, 0, 1 };
		auto oscMode = PCG_Converter::getPCGValue(data, temp);
		if (oscMode == 0)
		{
			// Ignore garbage values from the PCG for Osc2: Osc mode is Single
			return pcgBank;
		}
	}

	switch (pcgBank)
	{
	case 0: return 0; // Default
	case 2: return 0; // Default ??
	case 3: return 0; // Default ??
	case 5: return 0; // Default ??
	case 6: return 8; // Vint
	case 7: return 6; // OrchS
	case 8: return 7; // OrchB
	case 9: return 2; // Piano
	case 10: return 9; // Synth
	case 11: return 5; // Best
	case 12: return 4; // New2
	case 13: return 3; // New1
	}

	if (pcgBank == 1)
	{
		auto msg = std::format(" Unhandled Bank (RAM) ({})\n", paramName.c_str());
		std::cout << msg;
		return pcgBank;
	}
	else
	{
		assert(false, "Unknown bank");
		return pcgBank;
	}
}

std::vector<TritonStruct> PCG_Converter::program_osc_conversions = {
	{ "OSC 1 Multisample High Bank"					, "hi_bank", 232, 0, 7, -1, -1, -1, 8, TritonStruct::EVarType::Unsigned, convertOSCBank },
	{ "OSC 1 Multisample High Multisample/Drumkit"	, "hi_sample_no.", 230, 0, 6, 231, 0, 7 },
	{ "OSC 1 Multisample High Start Offset"			, "hi_start_offset", 230, 7, 7 },
	{ "OSC 1 Multisample High Reverse"				, "hi_reverse", 230, 6, 6 },
	{ "OSC 1 Multisample High Level"				, "hi_level", 233, 0, 7 },
	{ "OSC 1 Multisample Low Bank"					, "low_bank", 236, 0, 7, -1, -1, -1, 8, TritonStruct::EVarType::Unsigned, convertOSCBank },
	{ "OSC 1 Multisample Low Multisample/Drumkit"	, "low_sample_no.", 234, 0, 6, 235, 0, 7 },
	{ "OSC 1 Multisample Low Start Offset"			, "low_start_offset", 234, 7, 7 },
	{ "OSC 1 Multisample Low Reverse"				, "low_reverse", 234, 6, 6 },
	{ "OSC 1 Multisample Low Level"					, "low_level", 237, 0, 7 },
	{ "OSC 1 Multisample Delay"						, "delay_start", 238, 0, 7 },
	{ "OSC 1 Multisample Switch Lo->Hi  -"			, "velocity_multisample_switch_lo->hi", 239, 0, 7 },
	{ "OSC 1 Bottom"								, "velocity_zone_bottom", 240, 0, 7 },
	{ "OSC 1 Top"									, "velocity_zone_top", 241, 0, 7 },
	{ "OSC 1 LFO 1 Waveform"						, "lfo_1_waveform", 242, 0, 4 },
	{ "OSC 1 LFO 1 Key Sync"						, "lfo_1_key_sync", 242, 7, 7 },
	{ "OSC 1 LFO 1 Frequency"						, "lfo_1_frequency", 243, 0, 7 },
	{ "OSC 1 LFO 1 Offset"							, "lfo_1_offset", 244, 0, 7 },
	{ "OSC 1 LFO 1 Delay"							, "lfo_1_delay", 245, 0, 7 },
	{ "OSC 1 LFO 1 Fade"							, "lfo_1_fade", 246, 0, 7 },
	{ "Frequency Modulation 1 AMS Source"			, "lfo_1_time_mod_ams1_source", 248, 0, 7 },
	{ "Frequency Modulation 1 AMS Intensity"		, "lfo_1_time_mod_ams1_intensity", 249, 0, 7 },
	{ "Frequency Modulation 2 AMS Source"			, "lfo_1_time_mod_ams2_source", 250, 0, 7 },
	{ "Frequency Modulation 2 AMS Intensity"		, "lfo_1_time_mod_ams2_intensity", 251, 0, 7 },
	{ "Frequency MIDI/Tempo Sync  MIDI/Tempo Sync"	, "lfo_1_miditempo_sync", 247, 7, 7 },
	{ "Frequency MIDI/Tempo Sync  Base Note"		, "lfo_1_sync_base_note", 247, 6, 4 },
	{ "Frequency MIDI/Tempo Sync  Times"			, "lfo_1_times", 248, 0, 3 },
	{ "OSC 1 LFO 2  Waveform"						, "lfo_2_waveform", 252, 0, 4 },
	{ "OSC 1 LFO 2  Key Sync"						, "lfo_2_key_sync", 252, 7, 7 },
	{ "OSC 1 LFO 2  Frequency"						, "lfo_2_frequency", 253, 0, 7 },
	{ "OSC 1 LFO 2  Offset"							, "lfo_2_offset", 254, 0, 7 },
	{ "OSC 1 LFO 2  Delay"							, "lfo_2_delay", 255, 0, 7 },
	{ "OSC 1 LFO 2  Fade"							, "lfo_2_fade", 256, 0, 7 },
	{ "Frequency Modulation 1 AMS Source"			, "lfo_2_time_mod_ams1_source", 258, 0, 7 },
	{ "Frequency Modulation 1 AMS Intensity"		, "lfo_2_time_mod_ams1_intensity", 259, 0, 7 },
	{ "Frequency Modulation 2 AMS Source"			, "lfo_2_time_mod_ams2_source", 260, 0, 7 },
	{ "Frequency Modulation 2 AMS Intensity"		, "lfo_2_time_mod_ams2_intensity", 261, 0, 7 },
	{ "Frequency MIDI/Tempo Sync  MIDI/Tempo Sync"	, "lfo_2_miditempo_sync", 257, 7, 7 },
	{ "Frequency MIDI/Tempo Sync  Base Note"		, "lfo_2_sync_base_note", 257, 6, 4 },
	{ "Frequency MIDI/Tempo Sync  Times"			, "lfo_2_times", 258, 0, 3 },
	{ "OSC 1 Multisample  Octave"					, "octave", 262, 0, 7 },
	{ "OSC 1 Multisample Transpose"					, "transpose", 263, 0, 7 },
	{ "OSC 1 Multisample Tune"						, "tune", 264, 0, 7, 265, 0, 7 },
	{ "Pitch AMS Source"							, "pitch_mod_ams_source", 266, 0, 7 },
	{ "Pitch AMS Intensity"							, "pitch_mod_ams_intensity", 267, 0, 7 },
	{ "Pitch Pitch Slope"							, "pitch_slope", 268, 0, 7 },
	{ "Pitch EG  Intensity"							, "pitch_eg_intensity", 269, 0, 7 },
	{ "Pitch EG AMS Source"							, "pitch_eg_mod_ams_source", 270, 0, 7 },
	{ "Pitch EG AMS Intensity"						, "pitch_eg_mod_ams_intensity", 271, 0, 7 },
	{ "LFO1  Intensity"								, "pitch_lfo1_intensity", 272, 0, 7 },
	{ "LFO2  Intensity"								, "pitch_lfo2_intensity", 273, 0, 7 },
	{ "Portamento Enable"							, "portamento", 274, 0, 0 },
	{ "Portamento Fingered"							, "portamento_fingered", 274, 1, 1 },
	{ "Portamento Time"								, "portamento_time", 275, 0, 7 },
	{ "Pitch  JS(+X)"								, "pitch_js_+x_intensity", 276, 0, 7 },
	{ "Pitch  JS(-X)"								, "pitch_js_-x_intensity", 277, 0, 7 },
	{ "Pitch  Ribbon"								, "pitch_ribbon_intensity", 278, 0, 7 },
	{ "LFO1  JS(+Y)"								, "pitch_lfo1_js_+y_intensity", 280, 0, 7 },
	{ "LFO2  JS(+Y)"								, "pitch_lfo2_js_+y_intensity", 281, 0, 7 },
	{ "LFO1 AMS Source"								, "pitch_lfo1_intensity_mod_ams_source", 282, 0, 7 },
	{ "LFO1 AMS Intensity"							, "pitch_lfo1_intensity_mod_ams_intensity", 283, 0, 7 },
	{ "LFO2 AMS Source"								, "pitch_lfo2_intensity_mod_ams_source", 284, 0, 7 },
	{ "LFO2 AMS Intensity"							, "pitch_lfo2_intensity_mod_ams_intensity", 285, 0, 7 },

	{ "Filter Type  Type"							, "filter_type", 286, 0, 7 },
	{ "Filter Type  Trim"							, "filter_trim", 287, 0, 7 },
	{ "Filter A  Resonance"							, "filter_resonance", 288, 0, 7 },
	{ "Filter A AMS Source"							, "filter_resonance_mod_ams_source", 289, 0, 7 },
	{ "Filter A AMS Intensity"						, "filter_resonance_mod_ams_intensity", 290, 0, 7 },
	{ "Filter EG AMS Source"						, "filter_eg_mod_ams_source", 291, 0, 7 },
	{ "LFO 1 AMS Source"							, "filter_lfo1_mod_ams_source", 292, 0, 7 },
	{ "LFO 2 AMS Source"							, "filter_lfo2_mod_ams_source", 293, 0, 7 },
	{ "Filter A  Frequency"							, "filter_a_frequency", 294, 0, 7 },
	{ "Keyboard Track  Intensity"					, "filter_a_keyboard_track_intensity", 295, 0, 7 },
	{ "Filter A Modulation AMS1 Source"				, "filter_a_mod_ams1_source", 296, 0, 7 },
	{ "Filter A Modulation AMS1 Intensity"			, "filter_a_mod_ams1_intensity", 297, 0, 7 },
	{ "Filter A Modulation AMS2 Source"				, "filter_a_mod_ams2_source", 298, 0, 7 },
	{ "Filter A Modulation AMS2 Intensity"			, "filter_a_mod_ams2_intensity", 299, 0, 7 },
	{ "Filter EG A  Intensity"						, "filter_a_eg_intensity", 300, 0, 7 },
	{ "Filter EG A  Velocity"						, "filter_a_velocity_intensity", 301, 0, 7 },
	{ "LFO 1 A  Intensity"							, "filter_a_lfo1_intensity", 302, 0, 7 },
	{ "LFO 2 A  Intensity"							, "filter_a_lfo2_intensity", 303, 0, 7 },
	{ "LFO 1 A  JS(-Y)"								, "filter_a_lfo1_js_-y_intensity", 304, 0, 7 },
	{ "LFO 2 A  JS(-Y)"								, "filter_a_lfo2_js_-y_intensity", 305, 0, 7 },
	{ "Filter EG A AMS Intensity"					, "filter_a_eg_mod_ams_intensity", 306, 0, 7 },
	{ "LFO 1 A AMS Intensity"						, "filter_a_lfo1_mod_ams_intensity", 307, 0, 7 },
	{ "LFO 2 A AMS Intensity"						, "filter_a_lfo2_mod_ams_intensity", 308, 0, 7 },
	{ "Filter B  Frequency"							, "filter_b_frequency", 309, 0, 7 },
	{ "Keyboard Track  Intensity"					, "filter_b_keyboard_track_intensity", 310, 0, 7 },
	{ "Filter B Modulation AMS1 Source"				, "filter_b_mod_ams1_source", 311, 0, 7 },
	{ "Filter B Modulation AMS1 Intensity"			, "filter_b_mod_ams1_intensity", 312, 0, 7 },
	{ "Filter B Modulation AMS2 Source"				, "filter_b_mod_ams2_source", 313, 0, 7 },
	{ "Filter B Modulation AMS2 Intensity"			, "filter_b_mod_ams2_intensity", 314, 0, 7 },
	{ "Filter EG B  Intensity"						, "filter_b_eg_intensity", 315, 0, 7 },
	{ "Filter EG B  Velocity"						, "filter_b_velocity_intensity", 316, 0, 7 },
	{ "LFO 1 B  Intensity"							, "filter_b_lfo1_intensity", 317, 0, 7 },
	{ "LFO 2 B  Intensity"							, "filter_b_lfo2_intensity", 318, 0, 7 },
	{ "LFO 1 B  JS(-Y)"								, "filter_b_lfo1_js_-y_intensity", 319, 0, 7 },
	{ "LFO 2 B  JS(-Y)"								, "filter_b_lfo2_js_-y_intensity", 320, 0, 7 },
	{ "Filter EG B AMS Intensity"					, "filter_b_eg_mod_ams_intensity", 321, 0, 7 },
	{ "LFO 1 B AMS Intensity"						, "filter_b_lfo1_mod_ams_intensity", 322, 0, 7 },
	{ "LFO 2 B AMS Intensity"						, "filter_b_lfo2_mod_ams_intensity", 323, 0, 7 },
	{ " Level Start"								, "filter_eg_start_level", 324, 0, 7 },
	{ " Time Attack"								, "filter_eg_attack_time", 325, 0, 7 },
	{ " Level Attack"								, "filter_eg_attack_level", 326, 0, 7 },
	{ " Time Decay"									, "filter_eg_decay_time", 327, 0, 7 },
	{ " Level Break"								, "filter_eg_break_point_level", 328, 0, 7 },
	{ " Time Slope"									, "filter_eg_slope_time", 329, 0, 7 },
	{ " Level Sustain"								, "filter_eg_sustain_level", 330, 0, 7 },
	{ " Time Release"								, "filter_eg_release_time", 331, 0, 7 },
	{ " Level Release"								, "filter_eg_release_level", 332, 0, 7 },

	{ "Time Modulation 1 AMS Source"				, "filter_eg_time_mod_ams1_source", 336, 0, 7 },
	{ "Time Modulation 1 AMS Intensity"				, "filter_eg_time_mod_ams1_intensity", 337, 0, 7 },
	{ "Time Modulation 2 AMS Source"				, "filter_eg_time_mod_ams2_source", 338, 0, 7 },
	{ "Time Modulation 2 AMS Intensity"				, "filter_eg_time_mod_ams2_intensity", 339, 0, 7 },
	{ "Level Modulation AMS Source"					, "filter_eg_level_mod_ams_source", 340, 0, 7 },
	{ "Level Modulation AMS Intensity"				, "filter_eg_level_mod_ams_intensity", 341, 0, 7 },
	{ "Time Modulation 1 AMS Attack"				, "filter_eg_attack_time_mod_ams1_direction", 333, 1, 0 },
	{ "Time Modulation 1 AMS Decay"					, "filter_eg_decay_time_mod_ams1_direction", 333, 3, 2 },
	{ "Time Modulation 1 AMS Slope"					, "filter_eg_slope_time_mod_ams1_direction", 333, 5, 4 },
	{ "Time Modulation 1 AMS Release"				, "filter_eg_release_time_mod_ams1_direction", 333, 7, 6 },
	{ "Time Modulation 2 AMS Attack"				, "filter_eg_attack_time_mod_ams2_direction", 334, 1, 0 },
	{ "Time Modulation 2 AMS Decay"					, "filter_eg_decay_time_mod_ams2_direction", 334, 3, 2 },
	{ "Time Modulation 2 AMS Slope"					, "filter_eg_slope_time_mod_ams2_direction", 334, 5, 4 },
	{ "Time Modulation 2 AMS Release"				, "filter_eg_release_time_mod_ams2_direction", 334, 7, 6 },
	{ "Level Modulation AMS Start"					, "filter_eg_start_level_mod_ams_direction", 335, 1, 0 },
	{ "Level Modulation AMS Attack"					, "filter_eg_attack_level_mod_ams_direction", 335, 3, 2 },
	{ "Level Modulation AMS Break"					, "filter_eg_break_level_mod_ams_direction", 335, 5, 4 },

	{ "Keyboard Track  Key Low"						, "filter_keytrack_key_low", 342, 0, 7 },
	{ "Keyboard Track  Ramp Low"					, "filter_keytrack_ramp_low", 343, 0, 7 },
	{ "Keyboard Track  Key High"					, "filter_keytrack_key_high", 344, 0, 7 },
	{ "Keyboard Track  Ramp High"					, "filter_keytrack_ramp_high", 345, 0, 7 },
	{ "Amp Level  Level"							, "amp_level", 346, 0, 7 },
	{ "Amp Modulation  Intensity"					, "amp_velocity_intensity", 347, 0, 7 },
	{ "Amp Modulation AMS Source"					, "amp_level_mod_ams_source", 348, 0, 7 },
	{ "Amp Modulation AMS Intensity"				, "amp_level_mod_ams_intensity", 349, 0, 7 },
	{ "LFO 1  Intensity"							, "amp_lfo1_intensity", 350, 0, 7 },
	{ "LFO 2  Intensity"							, "amp_lfo2_intensity", 351, 0, 7 },
	{ "LFO 1 AMS Source"							, "amp_lfo1_intensity_mod_ams_source", 352, 0, 7 },
	{ "LFO 1 AMS Intensity"							, "amp_lfo1_intensity_mod_ams_intensity", 353, 0, 7 },
	{ "LFO 2 AMS Source"							, "amp_lfo2_intensity_mod_ams_source", 354, 0, 7 },
	{ "LFO 2 AMS Intensity"							, "amp_lfo2_intensity_mod_ams_intensity", 355, 0, 7 },
	{ " Level Start"								, "amp_eg_start_level", 356, 0, 7 },
	{ " Time Attack"								, "amp_eg_attack_time", 357, 0, 7 },
	{ " Level Attack"								, "amp_eg_attack_level", 358, 0, 7 },
	{ " Time Decay"									, "amp_eg_decay_time", 359, 0, 7 },
	{ " Level Break"								, "amp_eg_break_point_level", 360, 0, 7 },
	{ " Time Slope"									, "amp_eg_slope_time", 361, 0, 7 },
	{ " Level Sustain"								, "amp_eg_sustain_level", 362, 0, 7 },
	{ " Time Release"								, "amp_eg_release_time", 363, 0, 7 },

	{ "Time Modulation 1 AMS Source"				, "amp_eg_time_mod_ams1_source", 364, 0, 7 },
	{ "Time Modulation 1 AMS Intensity"				, "amp_eg_time_mod_ams1_intensity", 365, 0, 7 },
	{ "Time Modulation 2 AMS Source"				, "amp_eg_time_mod_ams2_source", 366, 0, 7 },
	{ "Time Modulation 2 AMS Intensity"				, "amp_eg_time_mod_ams2_intensity", 367, 0, 7 },
	{ "Level Modulation AMS Source"					, "amp_eg_level_mod_ams_source", 368, 0, 7 },
	{ "Level Modulation AMS Intensity"				, "amp_eg_level_mod_ams_intensity", 369, 0, 7 },
	{ "Time Modulation 1 AMS Attack"				, "amp_eg_attack_time_mod_ams1_direction", 370, 0, 1 },
	{ "Time Modulation 1 AMS Decay"					, "amp_eg_decay_time_mod_ams1_direction", 370, 2, 3 },
	{ "Time Modulation 1 AMS Slope"					, "amp_eg_slope_time_mod_ams1_direction", 370, 4, 5 },
	{ "Time Modulation 1 AMS Release"				, "amp_eg_release_time_mod_ams1_direction", 370, 6, 7 },
	{ "Time Modulation 2 AMS Attack"				, "amp_eg_attack_time_mod_ams2_direction", 371, 0, 1 },
	{ "Time Modulation 2 AMS Decay"					, "amp_eg_decay_time_mod_ams2_direction", 371, 2, 3 },
	{ "Time Modulation 2 AMS Slope"					, "amp_eg_slope_time_mod_ams2_direction", 371, 4, 5 },
	{ "Time Modulation 2 AMS Release"				, "amp_eg_release_time_mod_ams2_direction", 371, 6, 7 },
	{ "Level Modulation AMS Start"					, "amp_eg_start_level_mod_ams_direction", 372, 0, 1 },
	{ "Level Modulation AMS Attack"					, "amp_eg_attack_level_mod_ams_direction", 372, 2, 3 },
	{ "Level Modulation AMS Break"					, "amp_eg_break_level_mod_ams_direction", 372, 4, 5 },
	{ "Keyboard Track  Key Low"						, "amp_keytrack_key_low", 374, 0, 7 },
	{ "Keyboard Track  Ramp Low"					, "amp_keytrack_ramp_low", 375, 0, 7 },
	{ "Keyboard Track  Key High"					, "amp_keytrack_key_high", 376, 0, 7 },
	{ "Keyboard Track  Ramp High"					, "amp_keytrack_ramp_high", 377, 0, 7 },
	{ "Pan  Pan"									, "output_pan", 379, 0, 6, -1, -1, -1, 8, TritonStruct::EVarType::Unsigned },
	{ "Pan AMS Source"								, "output_pan_mod_ams_source", 380, 0, 7 },
	{ "Pan AMS Intensity"							, "output_pan_mod_ams_intensity", 381, 0, 7 },
	{ "OSC MFX Send OSC 1 Send 1"					, "output_mfx1_send_level", 382, 0, 7 },
	{ "OSC MFX Send OSC 1 Send 2"					, "output_mfx2_send_level", 383, 0, 7 },
};
