#include "pcg_converter.h"

#include "alchemist.h"
#include "helpers.h"

#include <sstream>
#include <iostream>
#include <iomanip>
#include <array>
#include <format>
#include <regex>

#include "include/rapidjson/document.h"
#include "include/rapidjson/istreamwrapper.h"

std::vector<std::string> PCG_Converter::vst_bank_letters = { "A", "B", "C", "D" };

PCG_Converter::PCG_Converter(
	EnumKorgModel model,
	KorgPCG* pcg,
	const std::string destFolder)
	: m_model(model)
	, m_pcg(pcg)
	, m_destFolder(destFolder)
{
	retrieveTemplatesData();
	retrieveProgramNamesList();
	initEffectConversions();
}

PCG_Converter::PCG_Converter(const PCG_Converter& other, const std::string destFolder)
	: m_model(other.m_model)
	, m_pcg(other.m_pcg)
	, m_destFolder(destFolder)
{
	m_dictProgParams = other.m_dictProgParams;
	m_dictCombiParams = other.m_dictCombiParams;
	m_mapProgramsNames = other.m_mapProgramsNames;
}

void PCG_Converter::retrieveTemplatesData()
{
	auto parse = [](auto& path, auto& target)
	{
		std::ifstream ifs(path);
		if (!ifs.is_open())
		{
			std::cerr << "Template " << path << " not found!!\n";
			assert(ifs.is_open());
		}

		rapidjson::IStreamWrapper refisw{ ifs };
		target.ParseStream(refisw);
	};

	rapidjson::Document templateProgDoc;
	rapidjson::Document templateCombiDoc;

	if (m_model == EnumKorgModel::KORG_TRITON_EXTREME)
	{
		parse("Extreme_Program.patch", templateProgDoc);
		parse("Extreme_Combi.patch", templateCombiDoc);
	}
	else
	{
		parse("Triton_Program.patch", templateProgDoc);
		parse("Triton_Combi.patch", templateCombiDoc);
	}

	auto progDspSettings = templateProgDoc["dsp_settings"].GetArray();
	for (auto& setting: progDspSettings)
	{
		m_dictProgParams.emplace_back(setting["index"].GetInt(), setting["key"].GetString(), setting["value"].GetInt());
	}

	auto combiDspSettings = templateCombiDoc["dsp_settings"].GetArray();
	for (auto& setting : combiDspSettings)
	{
		m_dictCombiParams.emplace_back(setting["index"].GetInt(), setting["key"].GetString(), setting["value"].GetInt());
	}
}

void PCG_Converter::retrieveProgramNamesList()
{
	m_mapProgramsNames.clear();

	for (uint32_t i = 0; i < m_pcg->Program->count; i++)
	{
		auto* prog = m_pcg->Program->bank[i];
		for (uint32_t j = 0; j < prog->count; j++)
		{
			auto* item = prog->item[j];
			auto name = std::string((char*)item->data, 16);
			auto uniqueId = Helpers::getUniqueId(Helpers::bankIdToLetter(prog->bank), j);
			m_mapProgramsNames[uniqueId] = name;
		}
	}
}

KorgBank* PCG_Converter::findDependencyBank(KorgPCG* pcg, int depBank)
{
	auto depBankLetter = Helpers::pcgProgBankIdToLetter(depBank);
	KorgBank* foundBank = nullptr;

	for (uint32_t i = 0; i < pcg->Program->count; i++)
	{
		auto* prog = pcg->Program->bank[i];
		auto bankLetter = Helpers::bankIdToLetter(prog->bank);
		if (bankLetter == depBankLetter)
		{
			foundBank = prog;
			break;
		}
	}

	return foundBank;
}

void PCG_Converter::convertPrograms(const std::vector<std::string>& letters)
{
	int currentUserBank = 0;

	for (uint32_t i = 0; i < m_pcg->Program->count; i++)
	{
		auto* prog = m_pcg->Program->bank[i];
		auto bankLetter = Helpers::bankIdToLetter(prog->bank);

		if (std::find(letters.begin(), letters.end(), bankLetter) == letters.end())
			continue;

		auto targetLetter = vst_bank_letters[currentUserBank];
		auto userFolder = Helpers::createSubfolders(m_destFolder, "Program", targetLetter);

		for (uint32_t j = 0; j < prog->count; j++)
		{
			auto* item = prog->item[j];
			auto name = std::string((char*)item->data, 16);

			auto msg = utils::string_format("%s::%d Patching %s\n", Helpers::bankIdToLetter(prog->bank).c_str(), j, name.c_str());
			std::cout << msg;

			patchProgramToJson(prog->bank, j, name, item->data, userFolder, targetLetter);
		}

		currentUserBank++;
	}
}

void PCG_Converter::convertCombis(const std::vector<std::string>& letters)
{
	int currentUserBank = 0;

	for (uint32_t i = 0; i < m_pcg->Combination->count; i++)
	{
		auto* bank = m_pcg->Combination->bank[i];
		auto bankLetter = Helpers::bankIdToLetter(bank->bank);

		if (std::find(letters.begin(), letters.end(), bankLetter) == letters.end())
			continue;

		auto targetLetter = vst_bank_letters[currentUserBank];
		auto userFolder = Helpers::createSubfolders(m_destFolder, "Combi", targetLetter);

		for (uint32_t jPreset = 0; jPreset < bank->count; jPreset++)
		{
			auto* item = bank->item[jPreset];
			auto name = std::string((char*)item->data, 16);

			patchCombiToJson(bank->bank, jPreset, name, item->data, userFolder, targetLetter);
		}

		currentUserBank++;
	}
}

template<typename T>
T getBits_Impl(void* data, int offset, int bitstart, int bitend)
{
	assert(bitend >= bitstart);
	T* s = (T*)data + offset;
	unsigned bitmask = (1 << (bitend - bitstart + 1)) - 1;
	return (T)((*s >> bitstart) & bitmask);
}

short getBits(EVarType type, void* data, int offset, int bitstart, int bitend)
{
	if (type == EVarType::Signed)
		return getBits_Impl<char>(data, offset, bitstart, bitend);
	else
		return getBits_Impl<unsigned char>(data, offset, bitstart, bitend);
}

struct twoBits { char d : 2; };
struct threeBits { char d : 3; };
struct fourBits { char d : 4; };
struct fiveBits { char d : 5; };
struct sixBits { char d : 6; };
struct sevenBits { char d : 7; };
struct eightBits { char d : 8; };
struct nineBits { short d : 9; };
struct tenBits { short d : 10; };
struct elevenBits { short d : 11; };
struct twelveBits { short d : 12; };
struct thirteenBits { short d : 13; };
struct sixteenBits { short d : 16; };

template<typename S, typename T>
T convertOddValue(T val)
{
	S temp = static_cast<S>(val);
	return static_cast<T>(temp.d);
}

int convertValue(int val, int numBits)
{
	if (numBits == 2)
		return convertOddValue<twoBits>(val);
	else if (numBits == 3)
		return convertOddValue<threeBits>(val);
	else if (numBits == 4)
		return convertOddValue<fourBits>(val);
	else if (numBits == 5)
		return convertOddValue<fiveBits>(val);
	else if (numBits == 6)
		return convertOddValue<sixBits>(val);
	else if (numBits == 7)
		return convertOddValue<sevenBits>(val);
	else if (numBits == 8)
		return convertOddValue<eightBits>(val);
	else if (numBits == 9)
		return convertOddValue<nineBits>(val);
	else if (numBits == 10)
		return convertOddValue<tenBits>(val);
	else if (numBits == 11)
		return convertOddValue<elevenBits>(val);
	else if (numBits == 12)
		return convertOddValue<twelveBits>(val);
	else if (numBits == 13)
		return convertOddValue<thirteenBits>(val);
	else if (numBits == 16)
		return convertOddValue<sixteenBits>(val);

	return val;
}

int PCG_Converter::getPCGValue(unsigned char* data, TritonStruct& info)
{
	auto varType = (info.pcgLSBOffset >= 0 || info.third.has_value()) ? EVarType::Unsigned : info.varType;
	int result = getBits(varType, data, info.pcgOffset, info.pcgBitStart, info.pcgBitEnd);

	auto numBits = info.pcgBitEnd - info.pcgBitStart + 1;
	if (info.pcgLSBOffset >= 0)
	{
		short bitShift = (info.pcgLSBBitEnd - info.pcgLSBBitStart + 1);
		result <<= bitShift;

		int lsbVal = static_cast<int>(getBits(varType, data, info.pcgLSBOffset, info.pcgLSBBitStart, info.pcgLSBBitEnd));
		result |= lsbVal;

		numBits += bitShift;
	}

	if (info.third.has_value())
	{
		short bitShift = (info.third->bit_end - info.third->bit_start + 1);
		result <<= bitShift;

		int lsbVal = static_cast<int>(getBits(info.varType, data, info.third->offset, info.third->bit_start, info.third->bit_end));
		result |= lsbVal;

		numBits += bitShift;
	}

	// Convert values with odd num bits
	if (info.varType == EVarType::Signed)
	{
		result = convertValue(result, numBits);
	}

	return result;
}

std::string getPresetNameSafe(const std::string& presetName)
{
	auto safePresetName = std::string(presetName);
	std::string backslash = "\"";
	std::string escapedBackslash = "\\\"";

	std::vector<size_t> positions;

	size_t pos = safePresetName.find(backslash, 0);
	while (pos != std::string::npos)
	{
		positions.push_back(pos);
		pos = safePresetName.find(backslash, pos + 1);
	}

	for (int i = positions.size() - 1; i >= 0; i--)
	{
		safePresetName.replace(positions[i], backslash.size(), escapedBackslash);
	}

	return safePresetName;
}

void PCG_Converter::jsonWriteHeaderBegin(std::ostream& json, const std::string& presetName)
{
	auto safePresetName = getPresetNameSafe(presetName);
	json << "{\"version\": 1280, \"general_program_information\": {";
	json << "\"name\": \"" << safePresetName << "\", \"category\": \"Keyboard        \", \"categoryIndex\": 0, ";
	json << "\"author\": \"\", ";
}

void PCG_Converter::jsonWriteHeaderEnd(std::ostream& json, int presetId, int bankNumber,
	const std::string& targetLetter, const std::string& mode)
{
	json << "\"mode\": \"" << mode << "\", \"bank\": \"USER-" << targetLetter << "\", ";
	json << "\"number\": " << presetId << ", \"triton_bank_number\": " << bankNumber << ", ";
	json << "\"triton_program_number\": " << presetId << ", \"characters\": {\"arp_basic_phrase\": 0, ";
	json << "\"bright_dark\": 1, \"fast_slow\": 1, \"mono_poly\": 2, \"percussive_gate\": 2, \"pitch_mod_filter_mod\": 0, \"type\": 4}}, ";
}

void PCG_Converter::jsonWriteTimbers(std::ostream& json, const std::vector<Timber>& timbers)
{
	json << "\"timbres\": [";

	bool bAddComma = false;
	for(auto& timber : timbers)
	{
		if (bAddComma) json << ", ";
		json << "{\"timbre_name\":\"" << timber.programName << "\", \"bank_name\" : \"" << timber.bankName;
		json << "\", \"program_number\" : " << timber.programId << "}";

		if (!bAddComma) bAddComma = true;
	}

	json << " ],";
}

void PCG_Converter::jsonWriteEnd(std::ostream& json, const std::string& presetType)
{
	json << "], \"data_type\": \"" << presetType << "\"}";
	json << std::endl;
}

std::string getOutputPatchPath(int presetId, const std::string& userFolder)
{
	std::string outFilePath;
	std::ostringstream ss;
	ss << userFolder << std::setw(3) << std::setfill('0') << presetId << ".patch";
	return ss.str();
}

void PCG_Converter::jsonWriteDSPSettings(std::ostream& json, const PCG_Converter::ParamList& content)
{
	json << "\"dsp_settings\": [";

	bool bAddComma = false;
	for (auto& param : content)
	{
		if (bAddComma) json << ", ";
		json << "{\"index\": " << param.index << ", \"key\": \"" << param.key << "\", \"value\": " << param.value << "}";

		if (!bAddComma) bAddComma = true;
	}
}

void PCG_Converter::patchProgramToStream(int bankId, int presetId, const std::string& presetName, unsigned char* data,
	const std::string& targetLetter, std::ostream& out_stream)
{
	auto bankNumber = Helpers::getVSTBankNumber(EPatchMode::Program, targetLetter, m_model);
	jsonWriteHeaderBegin(out_stream, presetName);
	jsonWriteHeaderEnd(out_stream, presetId, bankNumber, targetLetter, "Program");

	auto& content = m_dictProgParams;
	patchInnerProgram(content, "prog_", data, presetName, EPatchMode::Program);
	patchProgramUnusedValues(content, "prog_");
	jsonWriteDSPSettings(out_stream, content);

	jsonWriteEnd(out_stream, "program");
}

void PCG_Converter::patchProgramToJson(int bankId, int presetId, const std::string& presetName, unsigned char* data,
	const std::string& userFolder, const std::string& targetLetter)
{
	auto outFilePath = getOutputPatchPath(presetId, userFolder);
	std::ofstream json(outFilePath);
	patchProgramToStream(bankId, presetId, presetName, data, targetLetter, json);
}

void PCG_Converter::patchEffect(PCG_Converter::ParamList& content, int dataOffset, unsigned char* data, int effectId, const std::string& prefix)
{
	if (effectId == 0) // No effect
		return;

	auto foundEntry = effect_conversions.find(effectId);
	if (foundEntry != effect_conversions.end())
	{
		for (const auto& spec_conv : foundEntry->second)
		{
			if (spec_conv.jsonParam.empty())
				continue;

			auto info = TritonStruct(spec_conv);
			info.pcgOffset += dataOffset;
			if (info.pcgLSBOffset != -1)
				info.pcgLSBOffset += dataOffset;
			if (info.third.has_value())
				info.third->offset += dataOffset;

			auto jsonName = utils::string_format("%s_specific_parameter_%s", prefix.c_str(), spec_conv.jsonParam.c_str());
			auto pcgVal = getPCGValue(data, info);

			auto found = std::find_if(content.begin(), content.end(), [&jsonName](auto& e) { return e.key == jsonName; });
			assert(found != content.end());
			found->value = pcgVal;
		}
	}
	else
	{
		auto msg = std::format(" Unhandled effect {} \n", effectId);
		std::cout << msg;
	}
}

void PCG_Converter::patchValue(PCG_Converter::ParamList& content, const std::string& jsonName, int value)
{
	auto found = std::find_if(content.begin(), content.end(), [&jsonName](auto& e) { return e.key == jsonName; });
	assert(found != content.end());
	found->value = value;
}

void PCG_Converter::patchCombiUnusedValues(PCG_Converter::ParamList& content, const std::string& prefix)
{
	struct NameValue { std::string name; int value; };

	std::vector<NameValue> ignoredParams =
	{
		{ "arpeggiator_gate_control", 0 },
		{ "arpeggiator_velocity_control", 0 },
		{ "arpeggiator_tempo", 40 },
		{ "arpeggiator_switch", 0 },
		{ "arpeggiator_pattern_no.", 0 },
		{ "arpeggiator_resolution", 0 },
		{ "arpeggiator_octave", 0 },
		{ "arpeggiator_gate", 0 },
		{ "arpeggiator_velocity", 1 },
		{ "arpeggiator_swing", 0 },
		{ "arpeggiator_sort_onoff", 0 },
		{ "arpeggiator_latch_onoff", 0 },
		{ "arpeggiator_keysync_onoff", 0 },
		{ "arpeggiator_keyboard_onoff", 0 },
		{ "arpeggiator_top_key", 0 },
		{ "arpeggiator_bottom_key", 0 },
		{ "arpeggiator_top_velocity", 1 },
		{ "arpeggiator_bottom_velocity", 1 },
		{ "osc_1_low_start_offset", 0 },
		{ "osc_2_low_start_offset", 0 },
		{ "knob1", 0 },
		{ "knob2", 0 },
		{ "knob3", 0 },
		{ "knob4", 0 },
	};

	for (auto& param : content)
	{
		for (auto& ignoredParam : ignoredParams)
		{
			std::string fullParamName = prefix + ignoredParam.name;
			if (param.key == fullParamName)
			{
				param.value = ignoredParam.value;
			}
		}
	}
}

void PCG_Converter::patchProgramUnusedValues(PCG_Converter::ParamList& content, const std::string& prefix)
{
	struct NameValue { std::string name; int value; };

	struct ProgramPatch
	{
		NameValue param;
		std::vector<NameValue> paramsToUpdate;
	};

	auto fullParamName = [&](auto& name)
	{
		return prefix + name;
	};

	std::vector<ProgramPatch> allPatches =
	{
		{ { "common_oscillator_mode", 2 }, {{ "osc_1_output_use_drum_kit_setting", 1 }} },
		{ { "osc_1_low_sample_no.", 4095 }, {{ "osc_1_low_sample_no.", 999 }} }, // N/A patch
		{ { "osc_2_low_sample_no.", 4095 }, {{ "osc_2_low_sample_no.", 999 }} }, // N/A patch
	};

	for (auto& param : content)
	{
		for (auto& patch : allPatches)
		{
			if (param.key == fullParamName(patch.param.name) && param.value == patch.param.value)
			{
				for (auto& paramToUpdate : patch.paramsToUpdate)
				{
					std::string targetName = fullParamName(paramToUpdate.name);
					auto found = std::find_if(content.begin(), content.end(), [&targetName](auto& e) { return e.key == targetName; });
					assert(found != content.end());
					found->value = paramToUpdate.value;
				}
			}
		}

		// Patching knobs default value
		for (int i = 1; i <= 4; i++)
		{
			std::string paramName;
			if (prefix.find("combi_") != std::string::npos)
				paramName = utils::string_format("%sknob%d_assign_type", prefix.c_str(), i);
			else
				paramName = utils::string_format("%scommon_knob%d_assign_type", prefix.c_str(), i);

			if (param.key == paramName) // 0:Off; 1-4: Assignable Knob; 5+: assigned params
			{
				std::string targetName = utils::string_format("%sknob%d", prefix.c_str(), i);
				auto found = std::find_if(content.begin(), content.end(), [&targetName](auto& e) { return e.key == targetName; });
				assert(found != content.end());

				switch (param.value)
				{
				case 10: // Expression
					found->value = 127;
					break;
				case 8: // Post IFX Pan
				case 9: // Pan
				case 13: // LPF Cutoff
				case 14: // Resonance/HPF
				case 15: // Filter EG Int.
				case 16: // Filter/Amp Attack
				case 17: // Filter/Amp Decay
				case 18: // Filter/Amp Sustain
				case 19: // Filter/Amp Release
				case 20: // LFO1 Speed
				case 21: // LFO1 Pitch Depth
					found->value = 64;
					break;
				default:
					found->value = 0;
				}
			}
		}
	}
}

void PCG_Converter::patchSharedConversions(PCG_Converter::ParamList& content, const std::string& prefix, unsigned char* data)
{
	for (auto& conversion : shared_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto jsonName = utils::string_format("%s%s", prefix.c_str(), conversion.jsonParam.c_str());
		auto pcgVal = getPCGValue(data, conversion);
		patchValue(content, jsonName, pcgVal);

		if (conversion.jsonParam.find("effect_type") != std::string::npos)
		{
			int index = conversion.jsonParam.find("mfx1") != std::string::npos ? 1 : 2;
			int startOffset = conversion.jsonParam.find("mfx1") != std::string::npos ? 136 : 156;
			auto fxprefix = utils::string_format("%smfx%d", prefix.c_str(), index);
			patchEffect(content, startOffset, data, pcgVal, fxprefix);
		}
	}

	for (auto& ifx_struct : program_ifx_offsets)
	{
		for (auto& conversion : program_ifx_conversions)
		{
			if (conversion.jsonParam.empty())
				continue;

			TritonStruct info = TritonStruct(conversion);
			info.pcgOffset += ifx_struct.startOffset;
			if (info.pcgLSBOffset != -1)
				info.pcgLSBOffset += ifx_struct.startOffset;

			auto jsonName = utils::string_format("%sifx%d_%s", prefix.c_str(), ifx_struct.id, conversion.jsonParam.c_str());
			auto pcgVal = getPCGValue(data, info);

			patchValue(content, jsonName, pcgVal);

			if (info.jsonParam.find("effect_type") != std::string::npos)
			{
				auto fxprefix = utils::string_format("%sifx%d", prefix.c_str(), ifx_struct.id);
				patchEffect(content, ifx_struct.startOffset, data, pcgVal, fxprefix);
			}
		}
	}
}

void PCG_Converter::patchInnerProgram(PCG_Converter::ParamList& content, const std::string& prefix, unsigned char* data,
	const std::string& progName, EPatchMode mode)
{
	for (auto& conversion : program_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto jsonName = utils::string_format("%s%s", prefix.c_str(), conversion.jsonParam.c_str());
		auto pcgVal = getPCGValue(data, conversion);
		patchValue(content, jsonName, pcgVal);
	}

	if (mode == EPatchMode::Program) // Program shared data (IFX, MFX, Valve..) not used in Combi mode
	{
		patchSharedConversions(content, prefix, data);

		if (m_model == EnumKorgModel::KORG_TRITON_EXTREME)
		{
			for (auto& conversion : triton_extreme_conversions)
			{
				if (conversion.jsonParam.empty())
					continue;

				auto pcgVal = getPCGValue(data, conversion);
				auto jsonName = utils::string_format("%s%s", prefix.c_str(), conversion.jsonParam.c_str());

				patchValue(content, jsonName, pcgVal);
			}
		}
	}

	static const int NUM_OSC = 2;
	for (int iOscId = 0; iOscId < NUM_OSC; iOscId++)
	{
		for (auto& conversion : program_osc_conversions)
		{
			if (conversion.jsonParam.empty())
				continue;

			TritonStruct info = TritonStruct(conversion);
			if (iOscId == 1)
			{
				info.pcgOffset += 154;
				if (info.pcgLSBOffset != -1)
					info.pcgLSBOffset += 154;
			}

			auto jsonName = utils::string_format("%sosc_%d_%s", prefix.c_str(), iOscId + 1, conversion.jsonParam.c_str());
			auto pcgVal = getPCGValue(data, info);

			if (conversion.optionalConverter)
				pcgVal = conversion.optionalConverter(pcgVal, jsonName, data);

			patchValue(content, jsonName, pcgVal);
		}
	}
}

void PCG_Converter::patchCombiToJson(int bankId, int presetId,
	const std::string& presetName, unsigned char* data, const std::string& userFolder, const std::string& targetLetter)
{
	auto outFilePath = getOutputPatchPath(presetId, userFolder);
	std::ofstream json(outFilePath);
	patchCombiToStream(bankId, presetId, presetName, data, targetLetter, json);
}

void PCG_Converter::patchToStream(EPatchMode mode, int bankId, int presetId, const std::string& presetName, unsigned char* data,
	const std::string& targetLetter, std::ostream& out_stream)
{
	if (mode == EPatchMode::Program)
	{
		patchProgramToStream(0, presetId, presetName, (unsigned char*)data, "A", out_stream);
	}
	else
	{
		patchCombiToStream(0, presetId, presetName, (unsigned char*)data, "A", out_stream);
	}
}

void PCG_Converter::patchCombiToStream(int bankId, int presetId, const std::string& presetName, unsigned char* data,
		const std::string& targetLetter, std::ostream& out_stream)
{
	auto msg = utils::string_format("%s::%d Patching %s\n", Helpers::bankIdToLetter(bankId).c_str(), presetId, presetName.c_str());
	std::cout << msg;

	auto& content = m_dictCombiParams;

	for (auto& conversion : combi_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto pcgVal = getPCGValue(data, conversion);
		patchValue(content, conversion.jsonParam, pcgVal);
	}

	patchSharedConversions(content, "combi_", data);

	for (auto& conversion : triton_extreme_conversions)
	{
		if (conversion.jsonParam.empty())
			continue;

		auto jsonName = utils::string_format("combi_%s", conversion.jsonParam.c_str());
		auto pcgVal = getPCGValue(data, conversion);
		patchValue(content, jsonName, pcgVal);
	}

	std::array<PCG_Converter::Prog, 8> associatedPrograms;

	for (auto& timbre : combi_timbres)
	{
		for (auto& conversion : combi_timbre_conversions)
		{
			if (conversion.jsonParam.empty())
				continue;

			auto jsonName = utils::string_format("combi_timbre_%d_%s", timbre.id, conversion.jsonParam.c_str());

			TritonStruct timbreInfo = TritonStruct(conversion);
			timbreInfo.pcgOffset += timbre.startOffset;
			if (timbreInfo.pcgLSBOffset != -1)
			{
				timbreInfo.pcgLSBOffset += timbre.startOffset;
			}
			
			auto pcgVal = getPCGValue(data, timbreInfo);

			if (conversion.optionalConverter)
				pcgVal = conversion.optionalConverter(pcgVal, jsonName, data);

			patchValue(content, jsonName, pcgVal);

			if (conversion.jsonParam.find("program_no") != std::string::npos)
			{
				associatedPrograms[timbre.id - 1].program = pcgVal;
			}
			else if (conversion.jsonParam.find("program_bank") != std::string::npos)
			{
				associatedPrograms[timbre.id - 1].bank = pcgVal;
			}
		}
	}

	// Global fields to patch (IFX...)
	patchProgramUnusedValues(content, "combi_");

	std::vector<Timber> timbersToWrite;

	int iTimber = 0;
	for (auto& prog : associatedPrograms)
	{
		std::string programName = "Unknown";
		auto letter = Helpers::pcgProgBankIdToLetter(prog.bank);
		auto uniqueId = Helpers::getUniqueId(letter, prog.program);
		if (m_mapProgramsNames.find(uniqueId) != m_mapProgramsNames.end())
		{
			programName = m_mapProgramsNames[uniqueId];
		}

		auto timberBankName = Helpers::getVSTProgramBankName(prog.bank, m_model);
		timbersToWrite.emplace_back(timberBankName, prog.program, programName);

		if (auto* progBank = findDependencyBank(m_pcg, prog.bank))
		{
			auto* progItem = progBank->item[prog.program];
			auto depProgName = std::string((char*)progItem->data, 16);

			auto prefix = utils::string_format("combi_timbre_%d_", iTimber + 1);
			patchInnerProgram(content, prefix, progItem->data, depProgName, EPatchMode::Combi);
			patchProgramUnusedValues(content, prefix);
			patchCombiUnusedValues(content, prefix);
		}

		iTimber++;
	}

	auto bankNumber = Helpers::getVSTBankNumber(EPatchMode::Combi, targetLetter, m_model);

	jsonWriteHeaderBegin(out_stream, presetName);
	jsonWriteTimbers(out_stream, timbersToWrite);
	jsonWriteHeaderEnd(out_stream, presetId, bankNumber, targetLetter, "Combi");
	jsonWriteDSPSettings(out_stream, content);

	jsonWriteEnd(out_stream, "combi");
}
