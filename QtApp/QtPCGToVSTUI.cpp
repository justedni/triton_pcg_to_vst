#include "QtPCGToVSTUI.h"

#include <QLabel>
#include <QFileDialog>
#include <QCheckbox>
#include <QFuture>
#include <QDir>
#include <QDirIterator>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrentRun>

#include "Worker.h"

#include "alchemist.h"
#include "pcg_converter.h"
#include "helpers.h"

QtPCGToVSTUI::QtPCGToVSTUI(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    auto font = QFont(ui.textEditLog->font());
    font.setPointSize(8);
    ui.textEditLog->setFont(font);

    analysePCG();
    updateVisibility();
}

QtPCGToVSTUI::~QtPCGToVSTUI()
{}

void QtPCGToVSTUI::on_checkBoxStateChanged(Qt::CheckState state, QCheckBox* checkbox)
{
    const auto letter = checkbox->text().toStdString();
    auto it = std::find(programCheckboxes.begin(), programCheckboxes.end(), checkbox);

    auto handleLetter = [&](auto& letterVec, auto& checkBoxVec)
    {
        if (state == Qt::CheckState::Checked)
        {
            letterVec.push_back(letter);
        }
        else
        {
            auto letterIt = std::find(letterVec.begin(), letterVec.end(), letter);
            Q_ASSERT(letterIt != letterVec.end());
            letterVec.erase(letterIt);
        }
    };

    if (it != programCheckboxes.end())
    {
        handleLetter(selectedProgramLetters, programCheckboxes);
    }
    else
    {
        it = std::find(combiCheckboxes.begin(), combiCheckboxes.end(), checkbox);
        if (it != combiCheckboxes.end())
        {
            handleLetter(selectedCombiLetters, combiCheckboxes);
        }
    }

    auto refreshLabel = [](auto&& prefix, auto& letterVec, auto& uiLabel)
    {
        auto text = QString(std::move(prefix));

        auto first = true;
        char targetLetter = 'A';
        for (auto& let : letterVec)
        {
            if (!first) text += "; ";
            text += QString::asprintf("%s-> USER-%c", let.c_str(), targetLetter);
            first = false;
            targetLetter++;
        }
        uiLabel->setText(text);
    };

    refreshLabel("Programs: ", selectedProgramLetters, ui.labelProgramSelection);
    refreshLabel("Combis: ", selectedCombiLetters, ui.labelCombiSelection);

    updateCheckboxes();
}

void QtPCGToVSTUI::updateCheckboxes()
{
    auto verify = [](auto& container)
    {
        int selectedCount = 0;
        for (auto* checkbox : container)
        {
            if (checkbox->isChecked())
                selectedCount++;
        }

        if (selectedCount >= 4)
        {
            for (auto* checkbox : container)
            {
                if (!checkbox->isChecked())
                    checkbox->setEnabled(false);
            }
        }
        else
        {
            for (auto* checkbox : container)
            {
                checkbox->setEnabled(true);
            }
        }
    };

    verify(programCheckboxes);
    verify(combiCheckboxes);

    updateVisibility();
}

void QtPCGToVSTUI::on_browsePCG_clicked()
{
    QString pcgPath = QFileDialog::getOpenFileName(this, "Open PCG", "", "Triton/Triton Extreme PCG file (*.pcg)");
    if (pcgPath.isEmpty())
        return;

    ui.linePCGPath->setText(pcgPath);
    analysePCG();
    updateVisibility();
}


void QtPCGToVSTUI::analysePCG()
{
    auto pcgPath = ui.linePCGPath->text();
    EnumKorgModel model;
    m_pcg = LoadTritonPCG(pcgPath.toStdString().c_str(), model);
    m_model = model;

    bool bIsTritonExtreme = (m_model == EnumKorgModel::KORG_TRITON_EXTREME);
    ui.radioTriton->setChecked(!bIsTritonExtreme);
    ui.radioTritonExtreme->setChecked(bIsTritonExtreme);


    auto addCheckboxes = [&](auto* pcgContainer, auto& guiCheckboxes, auto* layout)
    {
        for (auto* checkbox : guiCheckboxes)
        {
            layout->removeWidget(checkbox);
            delete checkbox;
        }

        if (pcgContainer)
        {
            for (uint32_t i = 0; i < pcgContainer->count; i++)
            {
                auto* bank = pcgContainer->bank[i];
                auto bankLetter = Helpers::bankIdToLetter(bank->bank);

                QCheckBox* checkbox = new QCheckBox(bankLetter.c_str(), this);
                connect(checkbox, &QCheckBox::checkStateChanged, this, [this, checkbox](auto state) { on_checkBoxStateChanged(state, checkbox); });
                guiCheckboxes.push_back(checkbox);
                layout->addWidget(checkbox);
            }
        }

        if (guiCheckboxes.size() <= 4)
        {
            for (auto* checkbox : guiCheckboxes)
            {
                checkbox->setChecked(true);
            }
        }
    };

    if (m_pcg)
    {
        addCheckboxes(m_pcg->Program, programCheckboxes, ui.hLayout_progLetters);
        addCheckboxes(m_pcg->Combination, combiCheckboxes, ui.hLayout_combiLetters);
    }
}

void QtPCGToVSTUI::on_browseTargetFolder_clicked()
{
    QString targetFolder = QFileDialog::getExistingDirectory(this, "Choose target directory");
    if (targetFolder.isEmpty())
        return;

    ui.lineTargetFolder->setText(targetFolder);
    updateVisibility();
}

void QtPCGToVSTUI::on_generateButton_clicked()
{
    auto outPath = ui.lineTargetFolder->text();

    if (outPath.isEmpty())
        return;

    int fileCount = 0;
    QDirIterator it(outPath, QStringList() << "*.patch", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        fileCount++;
        it.next();
    }

    if (fileCount > 0)
    {
        QMessageBox::StandardButton reply;
        auto msg = QString::asprintf("Target folder %s already contains %d .patch files.\nThe process will overwrite them. Continue?",
            outPath.toStdString().c_str(), fileCount);
        reply = QMessageBox::question(this, "Overwrite files in folder?", msg,
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
            return;
    }

    auto targetModel = ui.radioTritonExtreme->isChecked() ? EnumKorgModel::KORG_TRITON_EXTREME : EnumKorgModel::KORG_TRITON;

    QThread* thread = new QThread();
    Worker* worker = new Worker(targetModel, m_pcg, outPath.toStdString(), selectedProgramLetters, selectedCombiLetters);
    worker->moveToThread(thread);
    connect(worker, SIGNAL(log(const std::string&)), this, SLOT(logCallback(const std::string&)));
    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    connect(worker, &Worker::finished, thread, [thread](bool) { thread->quit(); });
    connect(worker, &Worker::finished, thread, [worker](bool state){ worker->deleteLater(); });
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(worker, SIGNAL(finished(bool)), this, SLOT(workerFinished(bool)));
    thread->start();

    disableEverything(true);
}

void QtPCGToVSTUI::logCallback(const std::string& text)
{
    ui.textEditLog->moveCursor(QTextCursor::End);
    ui.textEditLog->insertPlainText(QString(text.c_str()));
    ui.textEditLog->moveCursor(QTextCursor::End);
}

void QtPCGToVSTUI::workerFinished(bool success)
{
    if (success)
        logCallback("~~ finished! ~~");
    disableEverything(false);
}

void QtPCGToVSTUI::disableEverything(bool disable)
{
    ui.generateButton->setDisabled(disable);
    ui.browsePCG->setDisabled(disable);
    ui.browseTargetFolder->setDisabled(disable);

    if (disable)
    {
        for (auto* checkbox : programCheckboxes)
        {
            checkbox->setDisabled(true);
        }

        for (auto* checkbox : combiCheckboxes)
        {
            checkbox->setDisabled(true);
        }
    }
    else
    {
        updateCheckboxes();
        updateVisibility();
    }
}

void QtPCGToVSTUI::updateVisibility()
{
    bool bEnableGenerate = (!ui.linePCGPath->text().isEmpty())
        && (!ui.lineTargetFolder->text().isEmpty() && m_pcg && (!selectedProgramLetters.empty() || !selectedCombiLetters.empty()));
    ui.generateButton->setEnabled(bEnableGenerate);
}