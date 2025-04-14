#include "AddSymbolDialog.h"
#include "ui_AddSymbolDialog.h" 

#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>

AddSymbolDialog::AddSymbolDialog(const QStringList& availableModels, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::AddSymbolDialog), 
    m_availableModels(availableModels)
{
    ui->setupUi(this);
    setWindowTitle("Add Symbol/Model");

    ui->modelComboBox->addItems(m_availableModels); // Use addItems for QComboBox
    if (!m_availableModels.isEmpty()) {
        ui->modelComboBox->setCurrentIndex(0); // Select first item by default
    }
    else {
        // if no models are available
        ui->modelComboBox->setEnabled(false);
    }

    // Find the Add button (assuming it's part of QDialogButtonBox named buttonBox)
    QPushButton* addButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    if (addButton) {
        addButton->setText("Add");
        addButton->setEnabled(false); // Initially disabled
    }

    // Connect signals for validation
    connect(ui->symbolLineEdit, &QLineEdit::textChanged, this, &AddSymbolDialog::validateInput);
    connect(ui->modelComboBox, &QComboBox::currentIndexChanged, this, &AddSymbolDialog::validateInput);

    validateInput(); // Initial check
}

AddSymbolDialog::~AddSymbolDialog() {
    delete ui;
}

QString AddSymbolDialog::getSelectedSymbol() const {
    return ui->symbolLineEdit->text().trimmed().toUpper();
}

QString AddSymbolDialog::getSelectedModel() const {
    return ui->modelComboBox->currentText();
}

void AddSymbolDialog::onAddClicked() {
    accept();
}

void AddSymbolDialog::validateInput() {
    bool symbolOk = !ui->symbolLineEdit->text().trimmed().isEmpty();

    bool modelOk = ui->modelComboBox->currentIndex() != -1;

    QPushButton* addButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    if (addButton) {
        addButton->setEnabled(symbolOk && modelOk);
    }
}