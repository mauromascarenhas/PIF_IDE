#include "editorwindow.h"
#include "ui_editorwindow.h"

EditorWindow::EditorWindow(QWidget *parent) :
    NMainWindow(parent),
    ui(new Ui::EditorWindow)
{
    ui->setupUi(this);

    // Sets the custom Widgets on the parent Class
    // Otherwise, the window resizing feature will not work
    NMainWindow::setCustomWidgets(ui->centralWidget, ui->statusBar);

    hasChanged = false;

    frmAbout = nullptr;
    frmSettings = nullptr;

    setupEditor();
    setupEnvVars();
    createShortcuts();

    // Connects signals and slots
    connect(ui->btNewFile, SIGNAL(clicked()), this, SLOT(newFile()));
    connect(ui->btOpenFile, SIGNAL(clicked()), this, SLOT(openFile()));
    connect(ui->btSaveFile, SIGNAL(clicked()), this, SLOT(saveFile()));

    connect(ui->btConsoleView, SIGNAL(toggled(bool)), ui->consoleWidget, SLOT(setVisible(bool)));

    connect(ui->cIn, SIGNAL(returnPressed()), this, SLOT(sendUserInput()));

    connect(ui->btChangeExec, SIGNAL(clicked()), this, SLOT(changeExec()));

    connect(ui->btCompileNRun, SIGNAL(clicked()), this, SLOT(buildNRun()));
    connect(ui->btAbort, SIGNAL(clicked()), this, SLOT(abortProcess()));

    connect(ui->sourceEditor, SIGNAL(textChanged()), this, SLOT(sourceChanged()));

    connect(ui->btAbout, SIGNAL(clicked(bool)), this, SLOT(openAboutForm()));
    connect(ui->btSettings, SIGNAL(clicked(bool)), this, SLOT(openSettingsForm()));

    connect(&buildProcess, SIGNAL(readyRead()), this, SLOT(executionOutput()));
    connect(&buildProcess, SIGNAL(readyReadStandardError()), this, SLOT(builderError()));
    connect(&buildProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(builderOutput()));
    connect(&buildProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(builderExited(int,QProcess::ExitStatus)));

    connect(&compileProcess, SIGNAL(readyRead()), this, SLOT(executionOutput()));
    connect(&compileProcess, SIGNAL(readyReadStandardError()), this, SLOT(compilerError()));
    connect(&compileProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(compilerOutput()));
    connect(&compileProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(compilerExited(int,QProcess::ExitStatus)));

    connect(&executeProcess, SIGNAL(readyRead()), this, SLOT(executionOutput()));
    connect(&executeProcess, SIGNAL(readyReadStandardError()), this, SLOT(executionError()));
    connect(&executeProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(executionOutput()));
    connect(&executeProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(executionExited(int,QProcess::ExitStatus)));

    // Hides output console
    ui->btConsoleView->setChecked(false);

    // Sets focus on sourceEditor
    ui->sourceEditor->setFocus();
}

EditorWindow::~EditorWindow()
{
    delete highlighter;

    for (QShortcut *shortcut : shortCuts) delete shortcut;
    shortCuts.clear();

    delete ui;
}

void EditorWindow::openAboutForm(){
    if (frmAbout) delete frmAbout;

    frmAbout = new AboutWindow();
    frmAbout->show();
}

void EditorWindow::openSettingsForm(){
    if (frmSettings) delete frmSettings;

    frmSettings = new SettingsWindow();
    frmSettings->show();
}

void EditorWindow::setupEditor(){
    QFont font;
    QSettings settings("Nintersoft", "PIF IDE");
    if (settings.childGroups().contains("editor")){
        settings.beginGroup("editor");
        font.setFamily(settings.value("font_family", "Segoe UI").toString());
        font.setPointSize(settings.value("font_size", 12).toInt());
        settings.endGroup();
    }
    else {
        font.setFamily("Segoe UI");
        font.setPointSize(12);
    }
    font.setFixedPitch(true);

    ui->sourceEditor->setFont(font);
    highlighter = new Highlighter(ui->sourceEditor->document());
}

void EditorWindow::setupEnvVars(){
    QSettings settings("Nintersoft", "PIF IDE");
    if (settings.childGroups().contains("environment variables")){
        settings.beginGroup("environment variables");

        cPath = settings.value(settings.value("c_uses_cpp", false).toBool() ? "cpp_path" : "c_path", "").toString();
        cppPath = settings.value("cpp_path", "").toString();
        javaPath = settings.value("java_path", "").toString();
        pifcPath = settings.value("pifc_path", "").toString();
        javacPath = settings.value("javac_path", "").toString();
    }
    else {
        settings.beginGroup("environment variables");

        cPath = QStandardPaths::findExecutable("gcc");
        cppPath = QStandardPaths::findExecutable("g++");
        javaPath = QStandardPaths::findExecutable("java");
        pifcPath = QStandardPaths::findExecutable("pifc");
        javacPath = QStandardPaths::findExecutable("javac");

        QStringList aPaths;
        if (pifcPath.isEmpty() &&
                !(aPaths = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)).isEmpty())
            pifcPath = QStandardPaths::findExecutable("pifc", QStringList() << aPaths[0] + QDir::separator() + "Nintersoft" + QDir::separator() + "PIF" + QDir::separator());

        settings.setValue("c_path", cPath);
        settings.setValue("cpp_path", cppPath);
        settings.setValue("java_path", javaPath);
        settings.setValue("pifc_path", pifcPath);
        settings.setValue("javac_path", javacPath);

        if (cPath.isEmpty() && !cppPath.isEmpty()){
            cPath = cppPath;
            settings.setValue("c_uses_cpp", true);
        }
    }
    settings.endGroup();

    if (pifcPath.isEmpty()){
        ui->btCompileNRun->setEnabled(false);

        settings.beginGroup("warnings");
        if (!settings.value("no_pifc", false).toBool()){
            QCheckBox *cb = new QCheckBox(this);
            cb->setText(tr("Do not show this message again!"));

            QMessageBox message;
            message.setIcon(QMessageBox::Warning);
            message.setText(tr("It was not possible to find a PIF compiler in this machine."
                               " Any execution operation will be available until a valid path to a PIF compiler is set."));
            message.setStandardButtons(QMessageBox::Ok);
            message.setCheckBox(cb);

            connect(cb, &QCheckBox::toggled, [this](bool checked){
                QSettings settings("Nintersoft", "PIF IDE");
                settings.beginGroup("warnings");
                settings.setValue("no_pifc", checked);
                settings.endGroup();
            });

            message.exec();
            delete cb;
        }
        settings.endGroup();
    }

    if (settings.childGroups().contains("execution")){
        settings.beginGroup("execution");
        curExec = (Executor)settings.value("standard exec", Executor::NONE).toInt();
        settings.endGroup();
    }
    else {
        if (!cPath.isEmpty()) curExec = Executor::C;
        else if (!cppPath.isEmpty()) curExec = Executor::CPP;
        else if (!(javaPath.isEmpty() || javacPath.isEmpty())) curExec = Executor::JAVA;
        else curExec = Executor::NONE;

        settings.beginGroup("execution");
        settings.setValue("standard exec", curExec);
        settings.endGroup();
    }

    ui->btChangeExec->setEnabled(!(javaPath.isEmpty() || javacPath.isEmpty())
                                 || !cPath.isEmpty() || !cppPath.isEmpty());
}

void EditorWindow::createShortcuts(){
    shortCuts.append(new QShortcut(QKeySequence(tr("Ctrl+O", "File|Open")), this));
    shortCuts.append(new QShortcut(QKeySequence(tr("Ctrl+S", "File|Save")), this));
    shortCuts.append(new QShortcut(QKeySequence(tr("Ctrl+N", "File|New")), this));
    shortCuts.append(new QShortcut(QKeySequence(tr("Ctrl++", "Zoom|In")), this));
    shortCuts.append(new QShortcut(QKeySequence(tr("Ctrl+-", "Zoom|Out")), this));
    shortCuts.append(new QShortcut(QKeySequence(tr("Ctrl+Shift+C", "Console|Toggle")), this));
    shortCuts.append(new QShortcut(QKeySequence(tr("Ctrl+R", "Project|Run")), this));
    shortCuts.append(new QShortcut(QKeySequence(tr("Ctrl+Shift+E", "Project|Abort")), this));

    connect(shortCuts[0], SIGNAL(activated()), this, SLOT(openFile()));
    connect(shortCuts[1], SIGNAL(activated()), this, SLOT(saveFile()));
    connect(shortCuts[2], SIGNAL(activated()), this, SLOT(newFile()));

    connect(shortCuts[3], SIGNAL(activated()), this, SLOT(increaseFontSize()));
    connect(shortCuts[4], SIGNAL(activated()), this, SLOT(reduceFontSize()));

    connect(shortCuts[5], SIGNAL(activated()), ui->btConsoleView, SLOT(toggle()));

    if (!pifcPath.isEmpty()){
        connect(shortCuts[6], SIGNAL(activated()), this, SLOT(buildNRun()));
        connect(shortCuts[7], SIGNAL(activated()), this, SLOT(abortProcess()));
    }
}

void EditorWindow::sourceChanged(){
    hasChanged = true;
    if (currentFile.isOpen())
        this->setWindowTitle(tr("%1.pifc* | PIF IDE").arg(QFileInfo(currentFile).baseName()));
    else this->setWindowTitle(tr("NewFile.pifc* | PIF IDE"));

    disconnect(ui->sourceEditor, SIGNAL(textChanged()), this, SLOT(sourceChanged()));
}

void EditorWindow::openFile(){
    int alt = -1;
    if (hasChanged && (alt = QMessageBox::question(this, tr("Save current | PIF IDE"),
                                     tr("The current project has unsaved changed. Would you like to save them"
                                        " before continuing?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel))
                        == QMessageBox::Cancel) return;

    if (alt > 0 && alt == QMessageBox::Yes)
        while (!saveFile() &&
                QMessageBox::question(this, tr("Confirmation | PIF IDE"),
                                      tr("Seems that you have aborted the operation of saving the current project."
                                         " Would you like to reconsider and try to save it again?"),
                                      QMessageBox::Yes, QMessageBox::No)
                    == QMessageBox::Yes);

    if (currentFile.isOpen()) currentFile.close();

    QFileDialog openDialog(this, tr("Open PIF file - PIF IDE"), QDir::homePath(), tr("PIF source file (%1)").arg("*.pifc"));
    openDialog.setFileMode(QFileDialog::ExistingFile);
    openDialog.setAcceptMode(QFileDialog::AcceptOpen);
    openDialog.setDefaultSuffix(".pifc");
    if (openDialog.exec()){
        currentFile.setFileName(openDialog.selectedFiles()[0]);
        if (!currentFile.open(QIODevice::ReadWrite | QIODevice::Text)){
            QMessageBox::critical(this, tr("Error | PIF IDE"),
                                  tr("It was not possible to open the specified file. Please, make sure the file %1 "
                                     "exists and is readable/writeable.").arg(currentFile.fileName()),
                                  QMessageBox::Ok, QMessageBox::NoButton);
            return;
        }

        hasChanged = false;
        disconnect(ui->sourceEditor, SIGNAL(textChanged()), this, SLOT(sourceChanged()));
        this->setWindowTitle(tr("%1.pifc | PIF IDE").arg(QFileInfo(currentFile).baseName()));
        ui->sourceEditor->setText(currentFile.readAll());
        connect(ui->sourceEditor, SIGNAL(textChanged()), this, SLOT(sourceChanged()));
    }
}

bool EditorWindow::saveFile(){
    if (!hasChanged) return true;

    if (currentFile.isOpen()){
        currentFile.close();
        if (currentFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)){
            QTextStream stream(&currentFile);
            stream.setCodec("UTF-8");
            stream.setGenerateByteOrderMark(true);
            stream << ui->sourceEditor->toPlainText().toUtf8();
            currentFile.close();

            while (!currentFile.open(QIODevice::ReadWrite | QIODevice::Text) &&
                QMessageBox::warning(this, tr("Warning | PIF IDE"),
                                     tr("It was not possible to stablish a file lock (open with read/write permissions)."
                                        " This may cause your current project to be overwritten by other programs."
                                        " Would you like us to try again?"), QMessageBox::Yes, QMessageBox::No)
                   == QMessageBox::Yes);

            hasChanged = false;
            this->setWindowTitle(tr("%1.pifc | PIF IDE").arg(QFileInfo(currentFile).baseName()));
            connect(ui->sourceEditor, SIGNAL(textChanged()), this, SLOT(sourceChanged()));
            return true;
        }
        else {
            QMessageBox::critical(this, tr("Error | PIF IDE"),
                                  tr("It was not possible to truncate the specified filename so as to save the"
                                     " current state of the project (It was not possible to stablish the file"
                                     " lock as well). Please, try again later."), QMessageBox::Ok);
            return false;
        }
    }

    QFileDialog saveDialog(this, tr("Save PIF file - PIF IDE"), QDir::homePath(), tr("PIF source file (%1)").arg("*.pifc"));
    saveDialog.setAcceptMode(QFileDialog::AcceptSave);
    saveDialog.setDefaultSuffix(".pifc");
    if (saveDialog.exec()){
        currentFile.setFileName(saveDialog.selectedFiles()[0]);
        if (!currentFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)){
            QMessageBox::critical(this, tr("Error | PIF IDE"),
                                  tr("It was not possible to create the specified file. Please, make sure the file %1 "
                                     "is readable/writeable.").arg(currentFile.fileName()),
                                  QMessageBox::Ok, QMessageBox::NoButton);
            return false;
        }

        QTextStream stream(&currentFile);
        stream.setCodec("UTF-8");
        stream.setGenerateByteOrderMark(true);
        stream << ui->sourceEditor->toPlainText().toUtf8();
        currentFile.close();

        while (!currentFile.open(QIODevice::ReadWrite | QIODevice::Text) &&
            QMessageBox::warning(this, tr("Warning | PIF IDE"),
                                 tr("It was not possible to stablish a file lock (open with read/write permissions)."
                                    " This may cause your current project to be overwritten by other programs."
                                    " Would you like us to try again?"), QMessageBox::Yes, QMessageBox::No)
               == QMessageBox::Yes);

        hasChanged = false;
        this->setWindowTitle(tr("%1.pifc | PIF IDE").arg(QFileInfo(currentFile).baseName()));
        connect(ui->sourceEditor, SIGNAL(textChanged()), this, SLOT(sourceChanged()));
        return true;
    }
    else return false;
}

void EditorWindow::newFile(){
    int alt = -1;
    if (hasChanged && (alt = QMessageBox::question(this, tr("Save current | PIF IDE"),
                                     tr("The current project has unsaved changed. Would you like to save them"
                                        " before continuing?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel))
                        == QMessageBox::Cancel) return;

    if (alt > 0 && alt == QMessageBox::Yes)
        while (!saveFile() &&
                QMessageBox::question(this, tr("Confirmation | PIF IDE"),
                                      tr("Seems that you have aborted the operation of saving the current project."
                                         " Would you like to reconsider and try to save it again?"),
                                      QMessageBox::Yes, QMessageBox::No)
                    == QMessageBox::Yes);

    if (currentFile.isOpen()) currentFile.close();

    hasChanged = false;
    disconnect(ui->sourceEditor, SIGNAL(textChanged()), this, SLOT(sourceChanged()));
    this->setWindowTitle(tr("PIF IDE"));
    ui->sourceEditor->clear();
    connect(ui->sourceEditor, SIGNAL(textChanged()), this, SLOT(sourceChanged()));
}

void EditorWindow::compileProject(){
    int alt = -1;
    if (hasChanged && (alt = QMessageBox::question(this, tr("Save current | PIF IDE"),
                                     tr("The current project has unsaved changed. Would you like to save them"
                                        " before continuing? (if you choose 'no', the project is not going to be built)"),
                                                   QMessageBox::Yes | QMessageBox::No))
                        == QMessageBox::Cancel) return;
    if (alt > 0 && alt == QMessageBox::Yes)
        while (!saveFile() &&
                (alt = QMessageBox::question(this, tr("Confirmation | PIF IDE"),
                                      tr("Seems that you have aborted the operation of saving the current project."
                                         " Would you like to reconsider and try to save it again?"),
                                      QMessageBox::Yes, QMessageBox::No))
                    == QMessageBox::Yes);
    if (alt == QMessageBox::No) return;

    ui->btAbort->setEnabled(true);
    ui->btCompileNRun->setEnabled(false);
    if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();

    ui->cOut->append(QString("<font color=\"black\" font-weight=\"bold\">[ %1 ] :</font>").arg(tr("PIF COMPILER OUTPUT")));

    buildProcess.setProgram(pifcPath);
    switch (curExec) {
        case C:
            buildProcess.setArguments(QStringList() << "-f" << "-l" << "c" << currentFile.fileName());
            break;
        case CPP:
            buildProcess.setArguments(QStringList() << "-f" << "-l" << "cpp" << currentFile.fileName());
            break;
        default:
            buildProcess.setArguments(QStringList() << "-f" << "-l" << "java" << currentFile.fileName());
            break;
    }
    buildProcess.start();
}

void EditorWindow::compileObject(){
    ui->cOut->append(QString("<font color=\"black\" font-weight=\"bold\">[ %1 ] :</font>").arg(tr("COMPILER OUTPUT")));

    QFileInfo currentFileInfo(currentFile.fileName());
    QString objectPath = currentFileInfo.absolutePath() + QDir::separator() + currentFileInfo.baseName();
#ifdef Q_OS_WINDOWS
    QString compileExt = ".exe";
#else
    QString compileExt = "";
#endif

    switch (curExec) {
        case C:
            if (cPath.isEmpty()) {
                if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
                ui->cOut->append(QString("<font color=\"red\" font-weight=\"bold\">%1</font>").arg(tr("%1 compiler path is not set.").arg("C")));
                abortProcess();
                return;
            }
            compileProcess.setProgram(cPath);
            compileProcess.setArguments(QStringList() << (objectPath + ".c")
                                        << "-o" << (objectPath + compileExt));
            break;
        case CPP:
            if (cppPath.isEmpty()) {
                if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
                ui->cOut->append(QString("<font color=\"red\" font-weight=\"bold\">%1</font>").arg(tr("%1 compiler path is not set.").arg("C++")));
                abortProcess();
                return;
            }
            compileProcess.setProgram(javaPath);
            compileProcess.setArguments(QStringList() << (objectPath + ".cpp")
                                        << "-o" << (objectPath + compileExt));
            break;
        case JAVA:
            if (javacPath.isEmpty()) {
                if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
                ui->cOut->append(QString("<font color=\"red\" font-weight=\"bold\">%1</font>").arg(tr("%1 compiler path is not set.").arg("Java")));
                abortProcess();
                return;
            }
            compileProcess.setProgram(javacPath);
            compileProcess.setArguments(QStringList() << (objectPath + ".java"));
            break;
        default:
            if (javacPath.isEmpty()) {
                if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
                ui->cOut->append(QString("<font color=\"red\" font-weight=\"bold\">%1</font>").arg(tr("There is no final compiler defined!")));
                abortProcess();
                return;
            }
    }
    compileProcess.start();
}

void EditorWindow::runProject(){
    ui->btAbort->setEnabled(true);
    ui->btCompileNRun->setEnabled(false);
    if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();

    QFileInfo currentFileInfo(currentFile.fileName());
    QString objectPath = currentFileInfo.absolutePath() + QDir::separator() + currentFileInfo.baseName();
#ifdef Q_OS_WINDOWS
    objectPath += ".exe";
#endif

    if (!QFile::exists(objectPath)) {
        if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
        ui->cOut->append(QString("<font color=\"red\" font-weight=\"bold\">%1</font>").arg(tr("There is no output executable to run. Expected : '%1'").arg(objectPath)));
        abortProcess();
        return;
    }

    switch (curExec) {
        case JAVA:
            if (javaPath.isEmpty()) {
                if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
                ui->cOut->append(QString("<font color=\"red\" font-weight=\"bold\">%1</font>").arg(tr("There is no JVM set. Execution aborted.")));
                abortProcess();
                return;
            }
            executeProcess.setProgram(javaPath);
            executeProcess.setArguments(QStringList() << "-cp" << currentFileInfo.absolutePath()
                                            << currentFileInfo.baseName());
            break;
        default:
            executeProcess.setProgram(objectPath);
            executeProcess.setArguments(QStringList());
            break;
    }
    executeProcess.start();
}

void EditorWindow::reduceFontSize(){
    QFont oldF = ui->sourceEditor->font();
    int old = oldF.pointSize();
    oldF.setPointSize(old > 10 ? --old : old);

    ui->sourceEditor->setFont(oldF);
    ui->statusBar->showMessage(tr("Font size set : %1").arg(old), 5000);
}

void EditorWindow::increaseFontSize(){
    QFont oldF = ui->sourceEditor->font();
    int old = oldF.pointSize();
    oldF.setPointSize(++old);

    ui->sourceEditor->setFont(oldF);
    ui->statusBar->showMessage(tr("Font size set : %1").arg(old), 5000);
}

void EditorWindow::sendUserInput(){
    if (executeProcess.state() == QProcess::NotRunning)
        ui->cOut->append(QString("<font color=\"red\">%1</font>").arg(ui->cIn->text()));
    else {
        ui->cOut->append(QString("<font color=\"blue\">%1</font>").arg(ui->cIn->text()));
        executeProcess.write(ui->cIn->text().toUtf8());
    }
    ui->cIn->clear();
}

void EditorWindow::changeExec(){
    QString c = tr("C compiler (executable)"),
            cpp = tr("C++ compiler (executable)"),
            java = tr("Java (Java binary)");

    int checked = 0;
    QStringList itens;
    if (!cPath.isEmpty()) itens << c;
    if (!cppPath.isEmpty()){
        itens << cpp;
        if (curExec == Executor::CPP) checked = itens.size() - 1;
    }
    if (!(javaPath.isEmpty() || javacPath.isEmpty())){
        itens << java;
        if (curExec == Executor::JAVA) checked = itens.size() - 1;
    }

    QString selected = QInputDialog::getItem(this, tr("Choose executor | PIF IDE"),
                                            tr("Please, choose the final compiler in which the application will"
                                               " be build with."),
                                            itens, checked, false);

    if (selected == c) curExec = Executor::C;
    else if (selected == cpp) curExec = Executor::CPP;
    else if (selected == java) curExec = Executor::JAVA;
    else curExec = Executor::NONE;

    QSettings settings("Nintersoft", "PIF IDE");
    settings.beginGroup("execution");
    settings.setValue("standard exec", curExec);
    settings.endGroup();
}

void EditorWindow::buildNRun(){
    ui->cOut->clear();
    compileProject();
}

void EditorWindow::abortProcess(){
    ui->btAbort->setEnabled(false);
    ui->btCompileNRun->setEnabled(true);

    if (buildProcess.state() != QProcess::NotRunning) buildProcess.kill();
    if (compileProcess.state() != QProcess::NotRunning) compileProcess.kill();
    if (executeProcess.state() != QProcess::NotRunning) executeProcess.kill();
}

void EditorWindow::builderError(){
    if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
    ui->cOut->append(QString("<font color=\"orange\" font-weight=\"bold\">%1</font>").arg(QString::fromUtf8(buildProcess.readAllStandardError())));
}

void EditorWindow::builderOutput(){
    if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
    ui->cOut->append(QString("<font color=\"black\" font-weight=\"bold\">%1</font>").arg(QString::fromUtf8(buildProcess.readAllStandardOutput())));
}

void EditorWindow::builderExited(int exitCode, QProcess::ExitStatus status){
    if (exitCode || status == QProcess::CrashExit){
        ui->cOut->append(QString("<font color=\"orange\" font-weight=\"bold\">%1</font>").arg(tr("PIF Compiler exited with code %1.").arg(exitCode)));
        abortProcess();
        return;
    }
    compileObject();
}

void EditorWindow::compilerError(){
    if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
    ui->cOut->append(QString("<font color=\"orange\" font-weight=\"bold\">%1</font>").arg(QString::fromUtf8(compileProcess.readAllStandardError())));
}

void EditorWindow::compilerOutput(){
    if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
    ui->cOut->append(QString("<font color=\"black\" font-weight=\"bold\">%1</font>").arg(QString::fromUtf8(compileProcess.readAllStandardOutput())));
}

void EditorWindow::compilerExited(int exitCode, QProcess::ExitStatus status){
    if (exitCode || status == QProcess::CrashExit){
        ui->cOut->append(QString("<font color=\"orange\" font-weight=\"bold\">%1</font>").arg(tr("Compiler exited with code %1.").arg(exitCode)));
        abortProcess();
        return;
    }
    runProject();
}

void EditorWindow::executionError(){
    if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
    ui->cOut->append(QString("<font color=\"black\" font-weight=\"bold\">%1</font>").arg(QString::fromUtf8(executeProcess.readAllStandardError())));
}

void EditorWindow::executionOutput(){
    if (!ui->btConsoleView->isChecked()) ui->btConsoleView->toggle();
    ui->cOut->append(QString("<font color=\"black\">%1</font>").arg(QString::fromUtf8(buildProcess.readAllStandardOutput())));
}

void EditorWindow::executionExited(int exitCode, QProcess::ExitStatus){
    ui->cOut->append(QString("<font color=\"black\" font-weight=\"bold\">%1</font>").arg(tr("\nProcess exited with code %1.").arg(exitCode)));
}
