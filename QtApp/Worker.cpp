#include "Worker.h"

#include "QtPCGToVSTUI.h"

#include "pcg_converter.h"

Worker::Worker(EnumKorgModel in_model,
    KorgPCG* in_pcg,
    const std::string& in_path,
    const std::vector<BankSelection>& in_programSelection,
    const std::vector<BankSelection>& in_combiSelection)
    : m_model(in_model)
    , m_pcg(in_pcg)
    , m_targetPath(in_path)
    , m_selectedPrograms(in_programSelection)
    , m_selectedCombis(in_combiSelection)
{
}

Worker::~Worker()
{
}

void Worker::process()
{
    auto converter = PCG_Converter(
        m_model,
        m_pcg,
        m_targetPath,
        std::bind(&Worker::logCallback, this, std::placeholders::_1)
    );

    if (converter.isInitialized())
    {
        auto process = [](auto& selected, auto&& func)
        {
            if (!selected.empty())
            {
                std::vector<std::string> letters;
                std::vector<int> targetLetterIds;
                for (auto& sel : selected)
                {
                    letters.push_back(sel.srcBank);
                    targetLetterIds.push_back(sel.targetBankId);
                }

                func(letters, targetLetterIds);
            }

        };

        process(m_selectedPrograms, [&](const auto& letters, const auto& targets) { converter.convertPrograms(letters, targets); });
        process(m_selectedCombis, [&](const auto& letters, const auto& targets) { converter.convertCombis(letters, targets); });

        emit finished(true);
    }
    else
    {
        emit finished(false);
    }
}

void Worker::logCallback(const std::string& str)
{
    emit log(str);
}
