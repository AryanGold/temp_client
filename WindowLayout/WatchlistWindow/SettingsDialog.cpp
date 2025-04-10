#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include <QPushButton>
#include <QLabel>

SettingsDialog::SettingsDialog(const QString& symbol, const QString& model, const QVariantMap& currentSettings, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog),
    m_symbol(symbol),
    m_model(model),
    m_currentSettings(currentSettings)
{
    ui->setupUi(this);
    setWindowTitle(QString("Settings - %1 / %2").arg(symbol, model));

    QLabel* placeholderLabel = new QLabel(QString("Placeholder for settings specific to %1 / %2").arg(symbol, model), this);
    ui->verticalLayout->insertWidget(0, placeholderLabel); 


    QPushButton* applyButton = ui->buttonBox->button(QDialogButtonBox::Ok); 
    if (applyButton) {
        applyButton->setText("Apply");
    }

    m_newSettings = m_currentSettings; // Initialize with current
}

SettingsDialog::~SettingsDialog() {
    delete ui;
}

QVariantMap SettingsDialog::getNewSettings() const {
    return m_newSettings;
}

void SettingsDialog::onApplyClicked() {
    accept();
}