#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_QtPCGToVSTUI.h"

#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;

enum class EnumKorgModel : uint8_t;
struct KorgPCG;
class PCG_Converter;

struct BankSelection
{
    BankSelection(const std::string& inSrcBank, QComboBox* inCombo, QLabel* inLabel, int inTargetBankId)
        : srcBank(inSrcBank)
        , targetCombo(inCombo)
        , label(inLabel)
        , targetBankId(inTargetBankId)
    {}

    std::string srcBank;
    QComboBox* targetCombo = nullptr;
    QLabel* label = nullptr;
    int targetBankId = 0;

    void cleanup(QHBoxLayout* layout);
};

class QtPCGToVSTUI : public QMainWindow
{
    Q_OBJECT

public:
    QtPCGToVSTUI(QWidget* parent = nullptr);
    ~QtPCGToVSTUI();

private slots:
    void on_browsePCG_clicked();
    void on_browseTargetFolder_clicked();
    void on_generateButton_clicked();

    void on_checkBoxStateChanged(Qt::CheckState state, QCheckBox* checkbox);
    void on_targetComboboxChanged(int index, QComboBox* combo);

    void logCallback(const std::string& text);
    void workerFinished(bool success);

private:
    void analysePCG();

    void disableEverything(bool disable);
    void updateVisibility();
    void updateCheckboxes();

    EnumKorgModel m_model;
    KorgPCG* m_pcg = nullptr;

    Ui::QtPCGToVSTUIClass ui;

    std::vector<QCheckBox*> programCheckboxes;
    std::vector<QCheckBox*> combiCheckboxes;

    std::vector<BankSelection> programBankSelection;
    std::vector<BankSelection> combiBankSelection;
};
