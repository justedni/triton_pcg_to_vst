#pragma once

#include <fstream>
#include <string>
#include <thread>
#include <assert.h>
#include <functional>
#include <optional>
#include <map>

struct KorgPCG;
struct KorgBank;
enum class EnumKorgModel : uint8_t;
enum class EPatchMode : uint8_t;
enum class EVarType : uint8_t { Signed, Unsigned };

struct Byte
{
	int offset;
	int bit_start;
	int bit_end;
};

struct TritonStruct
{
	std::string desc;
	std::string jsonParam;
	int pcgOffset;
	int pcgBitStart;
	int pcgBitEnd;
	EVarType varType = EVarType::Signed;

	int pcgLSBOffset = -1;
	int pcgLSBBitStart = -1;
	int pcgLSBBitEnd = -1;

	std::optional<Byte> third;

	std::function<int(int val, const std::string&, unsigned char*)> optionalConverter;
};

struct SubParam
{
	int id = 0;
	int startOffset = 0;
};

class PCG_Converter
{
public:
	PCG_Converter(
		EnumKorgModel model,
		KorgPCG* pcg,
		const std::string destFolder);

	PCG_Converter(const PCG_Converter& other, const std::string destFolder);

	void convertPrograms(const std::vector<std::string>& letters);
	void convertCombis(const std::vector<std::string>& letters);

	struct ProgParam
	{
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

	void patchCombiToStream(int bankId, int presetId, const std::string& presetName, unsigned char* data,
		const std::string& targetLetter, std::ostream& out_stream);
	void patchProgramToStream(int bankId, int presetId, const std::string& presetName, unsigned char* data,
		const std::string& targetLetter, std::ostream& out_stream);

	void patchToStream(EPatchMode mode, int bankId, int presetId, const std::string& presetName, unsigned char* data,
		const std::string& targetLetter, std::ostream& out_stream);

	static int getPCGValue(unsigned char* data, TritonStruct& info);

	void utils_convertGMProgramsToBin(const std::string& sourceFolder, const std::string& outFile);

private:
	void retrieveTemplatesData();
	void retrieveProgramNamesList();
	void retrieveGMData();

	typedef std::map<int, ProgParam> ParamList;
	void patchInnerProgram(ParamList& content, const std::string& prefix, unsigned char* data, const std::string& progName, EPatchMode mode);
	void patchSharedConversions(EPatchMode mode, ParamList& content, const std::string& prefix, unsigned char* data);
	void patchEffect(EPatchMode mode, ParamList& content, int dataOffset, unsigned char* data, int effectId, const std::string& prefix);
	void patchArpeggiator(ParamList& content, const std::string& prefix, const std::string& patternNoKey, unsigned char* data, EPatchMode mode);
	void patchDrumKit(ParamList& content, const std::string& prefix, unsigned char* data, EPatchMode mode);

	KorgBank* findDependencyBank(KorgPCG* pcg, int depBank);

	ProgParam* findParamByKey(EPatchMode mode, PCG_Converter::ParamList& content, const std::string& key);
	void patchValue(EPatchMode mode, ParamList& content, const std::string& jsonName, int value);

	void patchCombiUnusedValues(ParamList& content, const std::string& prefix);
	void patchProgramUnusedValues(EPatchMode mode, ParamList& content, const std::string& prefix);
	void postPatchCombi(ParamList& content);

	static std::pair<int, std::string> getCategory(EPatchMode mode, EnumKorgModel model, const ParamList& content);

	struct Timber
	{
		std::string bankName;
		int programId = 0;
		std::string programName;
	};

	static void jsonWriteHeaderBegin(std::ostream& json, const std::string& presetName, EPatchMode mode, EnumKorgModel model, const ParamList& content);
	static void jsonWriteHeaderEnd(std::ostream& json, int presetId, int bankNumber,
		const std::string& targetLetter, const std::string& mode);
	static void jsonWriteEnd(std::ostream& json, const std::string& presetType);
	static void jsonWriteDSPSettings(std::ostream& json, const ParamList& content);
	static void jsonWriteTimbers(std::ostream& json, const std::vector<Timber>& timbers);

	void convertProgramJsonToBin(PCG_Converter::ParamList& content, const std::string& programName, std::ostream& outStream);

	const EnumKorgModel m_model;
	KorgPCG* m_pcg = nullptr;
	const std::string m_destFolder;

	ParamList m_dictProgParams;
	ParamList m_dictCombiParams;

	static std::vector<char> m_gmData;

	struct GMInfo
	{
		std::string bank;
		int id;
		std::string osc;
		std::string name;
	};
	static std::vector<GMInfo> m_gmInfo;

	struct GMBankData
	{
		uint8_t bankId;
		uint8_t programId;
		int dataOffset = 0;
	};
	static std::vector<GMBankData> m_mappedGMInfo;

	static std::map<std::string, int> m_mapProgram_keyToId;
	static std::map<std::string, int> m_mapCombi_keyToId;

	std::map<std::string, std::string> m_mapProgramsNames;

	static std::vector<TritonStruct> shared_conversions;

	static int convertOSCBank(int pcgBank, const std::string& paramName, unsigned char* data);

	static std::vector<TritonStruct> program_conversions;
	static std::vector<TritonStruct> triton_extreme_conversions;
	static std::vector<TritonStruct> program_osc_conversions;
	static std::vector<SubParam> program_ifx_offsets;
	static std::vector<TritonStruct> program_ifx_conversions;

	static std::vector<TritonStruct> arpeggiator_global_conversions;
	static std::vector<TritonStruct> arpeggiator_step_conversions;

	static std::vector<std::string> drumkit_notes;
	static std::vector<TritonStruct> drumkit_conversions;

	static std::map<int, std::vector<TritonStruct>> effect_conversions;
	static void initEffectConversions();

	static std::vector<TritonStruct> combi_conversions;
	static std::vector<SubParam> combi_timbres;
	static std::vector<TritonStruct> combi_timbre_conversions;

	static std::vector<std::string> vst_bank_letters;
};