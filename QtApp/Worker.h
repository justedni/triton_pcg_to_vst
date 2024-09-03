#pragma once

#include <QObject>

enum class EnumKorgModel : uint8_t;
struct KorgPCG;

struct BankSelection;

class Worker : public QObject
{
    Q_OBJECT

public:
    Worker(EnumKorgModel in_model, KorgPCG* in_pcg, const std::string& in_path,
        const std::vector<BankSelection>& in_programSelection,
        const std::vector<BankSelection>& in_combiSelection);
    ~Worker();

    void logCallback(const std::string& str);

public slots:
    void process();

signals:
    void finished(bool success);
    void log(const std::string& str);

private:
    EnumKorgModel m_model;
    KorgPCG* m_pcg = nullptr;
    std::string m_targetPath;

    const std::vector<BankSelection>& m_selectedPrograms;
    const std::vector<BankSelection>& m_selectedCombis;
};
