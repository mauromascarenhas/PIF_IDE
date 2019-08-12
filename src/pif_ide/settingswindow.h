#ifndef SETTINGSWINDOW_H
#define SETTINGSWINDOW_H

#include <QSettings>
#include <QFileDialog>
#include <QMainWindow>
#include <QMessageBox>

#include "nmainwindow.h"

namespace Ui {
class SettingsWindow;
}

class SettingsWindow : public NMainWindow
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget *parent = nullptr);
    ~SettingsWindow();

private:
    Ui::SettingsWindow *ui;

    void loadSettings();

private slots:
    void saveSettings();
    void resetSettings();
    void useCppAsC(bool checked);

    void getCPath();
    void getCppPath();
    void getJavaPath();
    void getPifcPath();
    void getJavacPath();
};

#endif // SETTINGSWINDOW_H