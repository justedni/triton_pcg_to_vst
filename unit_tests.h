#pragma once

#include <stdint.h>
#include <string>
#include <vector>
#include <thread>

enum class EnumKorgModel : uint8_t;
struct KorgPCG;

enum class EPatchMode : uint8_t;
class PCG_Converter;

class UnitTest
{
public:
	UnitTest(const PCG_Converter* converterTemplate, KorgPCG* pcg, EPatchMode type, const std::string& unitTestFolder,
		const std::string& pcgBankLetter, int pcgProgId, const std::string& patchRefName);
	~UnitTest();

	void startThread();
	bool perform();

	void wait();

	void log(std::string&& txt);

private:
	std::thread* m_thread = nullptr;

	std::vector<std::string> m_outputLog;
	bool m_retValue = false;

	bool m_bUseThread = false;

	const PCG_Converter* m_converterTemplate;
	KorgPCG* m_pcg;
	const EPatchMode m_type;
	const std::string m_unitTestFolder;
	const std::string m_pcgBankLetter;
	const int m_pcgProgId;
	const std::string m_patchRefName;
};

void doUnitTests();