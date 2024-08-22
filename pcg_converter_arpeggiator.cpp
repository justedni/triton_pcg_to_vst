#include "pcg_converter.h"
#include <iostream>
#include <format>

std::vector<TritonStruct> PCG_Converter::arpeggiator_global_conversions = {
	{ ""					, "octave_motion"	, 16, 0, 1, EVarType::Unsigned },
	{ ""					, "arpeggio_type"	, 16, 2, 3, EVarType::Unsigned },
	{ ""					, "tone_mode"		, 16, 4, 4, EVarType::Unsigned },
	{ ""					, "fixed_note_mode"	, 16, 5, 5, EVarType::Unsigned },
	{ ""					, "length"			, 17, 0, 7, EVarType::Unsigned },
	{ ""					, "tone_note_no.1"	, 20, 0, 7 },
	{ ""					, "tone_note_no.2"	, 21, 0, 7 },
	{ ""					, "tone_note_no.3"	, 22, 0, 7 },
	{ ""					, "tone_note_no.4"	, 23, 0, 7 },
	{ ""					, "tone_note_no.5"	, 24, 0, 7 },
	{ ""					, "tone_note_no.6"	, 25, 0, 7 },
	{ ""					, "tone_note_no.7"	, 26, 0, 7 },
	{ ""					, "tone_note_no.8"	, 27, 0, 7 },
	{ ""					, "tone_note_no.9"	, 28, 0, 7 },
	{ ""					, "tone_note_no.10"	, 29, 0, 7 },
	{ ""					, "tone_note_no.11"	, 30, 0, 7 },
	{ ""					, "tone_note_no.12"	, 31, 0, 7 },
};

std::vector<TritonStruct> PCG_Converter::arpeggiator_step_conversions = {
	{ ""					, "pitch_offset", 32, 0, 7, EVarType::Signed },
	{ ""					, "gate"		, 33, 0, 7, EVarType::Unsigned },
	{ ""					, "velocity"	, 34, 0, 7, EVarType::Unsigned },
	{ ""					, "flam"		, 35, 0, 7, EVarType::Signed },
	{ ""					, "tone_0"		, 37, 0, 0 },
	{ ""					, "tone_1"		, 37, 1, 1 },
	{ ""					, "tone_2"		, 37, 2, 2 },
	{ ""					, "tone_3"		, 37, 3, 3 },
	{ ""					, "tone_4"		, 37, 4, 4 },
	{ ""					, "tone_5"		, 37, 5, 5 },
	{ ""					, "tone_6"		, 37, 6, 6 },
	{ ""					, "tone_7"		, 37, 7, 7 },
	{ ""					, "tone_8"		, 36, 0, 0 },
	{ ""					, "tone_9"		, 36, 1, 1 },
	{ ""					, "tone_10"		, 36, 2, 2 },
	{ ""					, "tone_11"		, 36, 3, 3 },
};
