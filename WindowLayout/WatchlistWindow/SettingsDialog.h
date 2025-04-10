#pragma once

#include <QDialog>
#include <QVariantMap>

QT_BEGIN_NAMESPACE
namespace Ui { class SettingsDialog; }
QT_END_NAMESPACE

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const QString& symbol, const QString& model, const QVariantMap& currentSettings, QWidget* parent = nullptr);
    ~SettingsDialog() override;

    QVariantMap getNewSettings() const;

private slots:
    void onApplyClicked();

private:
    Ui::SettingsDialog* ui;
    QString m_symbol;
    QString m_model;
    QVariantMap m_currentSettings;
    QVariantMap m_newSettings; // To store changes before applying
};
