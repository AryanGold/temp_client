#pragma once

#include <QDialog>

// Forward declarations
QT_BEGIN_NAMESPACE
namespace Ui { class AddSymbolDialog; }
QT_END_NAMESPACE
class QLineEdit;
class QListWidget;
class QPushButton;

class AddSymbolDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddSymbolDialog(const QStringList& availableModels, QWidget* parent = nullptr);
    ~AddSymbolDialog() override;

    QString getSelectedSymbol() const;
    QString getSelectedModel() const;

private slots:
    void onAddClicked();
    void validateInput(); // Enable Add button only when valid

private:
    Ui::AddSymbolDialog* ui; 

    QStringList m_availableModels;
};
