#pragma once

#include <fstream>
#include <string>
#include <thread>
#include <assert.h>

#include "include/nlohmann/json.hpp"

struct KorgPCG;
struct KorgBank;
enum class EnumKorgModel : uint8_t;
enum class EPatchMode : uint8_t;

struct TritonStruct
{
	enum class EVarType { Signed, Unsigned };

	std::string desc;
	std::string jsonParam;
	int pcgOffset;
	int pcgBitStart;
	int pcgBitEnd;
	int pcgLSBOffset = -1;
	int pcgLSBBitStart = -1;
	int pcgLSBBitEnd = -1;
	int pcgMSBBitShift = 8;
	EVarType varType = EVarType::Signed;

	std::function<int(int val, const std::string&, unsigned char*)> optionalConverter;
};

struct SubParam
{
	int id = 0;
	int startOffset = 0;
};

struct IfxType
{
	int id = 0;
	std::string name;
	std::vector<TritonStruct> conversions;
};


class PCG_Converter
{
public:
	PCG_Converter(
		EnumKorgModel model,
		KorgPCG* pcg,
		const std::string destFolder);

	void convertPrograms(const std::vector<std::string>& letters);
	void convertCombis(const std::vector<std::string>& letters);

	struct ProgParam
	{
		int index = -1;
		std::string key;
		int value = 0;
	};

	struct Prog
	{
		int bank = -1;
		int program = -1;
	};

	void patchCombiToJson(int bankId, int presetId, const std::string& presetName, unsigned char* data,
		const std::string& userFolder, const std::string& targetLetter);
	void patchProgramToJson(int bankId, int presetId, const std::string& presetName, unsigned char* data,
		const std::string& userFolder, const std::string& targetLetter);

	static int getPCGValue(unsigned char* data, TritonStruct& info);

	static bool unitTest(EnumKorgModel model, const std::string& inPCGBinName, const std::string& inVSTRefName);
private:
	void retrieveTemplatesData();
	void retrieveProgramNamesList();

	typedef std::vector<ProgParam> ParamList;
	void patchInnerProgram(ParamList& content, const std::string& prefix, unsigned char* data, const std::string& progName, EPatchMode mode);
	void patchSharedConversions(PCG_Converter::ParamList& content, const std::string& prefix, unsigned char* data);
	void patchEffect(ParamList& content, int dataOffset, unsigned char* data, int effectId, const std::string& prefix);

	KorgBank* findDependencyBank(KorgPCG* pcg, int depBank);

	void patchValue(PCG_Converter::ParamList& content, const std::string& jsonName, int value);

	struct Timber
	{
		std::string bankName;
		int programId = 0;
		std::string programName;
	};

	static void jsonWriteHeaderBegin(std::ofstream& json, const std::string& presetName);
	static void jsonWriteHeaderEnd(std::ofstream& json, int presetId, int bankNumber,
		const std::string& targetLetter, const std::string& mode);
	static void jsonWriteEnd(std::ofstream& json, const std::string& presetType);
	static void jsonWriteDSPSettings(std::ofstream& json, const ParamList& content);
	static void jsonWriteTimbers(std::ofstream& json, const std::vector<Timber>& timbers);

	// For unit tests:
	PCG_Converter(EnumKorgModel model);

	// Members
	const EnumKorgModel m_model;
	KorgPCG* m_pcg = nullptr;
	const std::string m_destFolder;

	nlohmann::json m_templateProg;
	nlohmann::json m_templateCombi;
	ParamList m_dictProgParams;
	ParamList m_dictCombiParams;

	std::map<std::string, std::string> m_mapProgramsNames;

	static std::vector<TritonStruct> shared_conversions;

	static std::vector<TritonStruct> program_conversions;
	static std::vector<TritonStruct> triton_extreme_prog_conversions;
	static std::vector<TritonStruct> triton_extreme_combi_conversions;
	static std::vector<TritonStruct> program_osc_conversions;
	static std::vector<SubParam> program_ifx_offsets;
	static std::vector<TritonStruct> program_ifx_conversions;

	static std::vector<IfxType> effect_conversions;

	static std::vector<TritonStruct> combi_conversions;
	static std::vector<SubParam> combi_timbres;
	static std::vector<TritonStruct> combi_timbre_conversions;

	static std::vector<std::string> vst_bank_letters;
};