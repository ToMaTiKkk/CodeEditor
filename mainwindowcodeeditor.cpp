#include "mainwindowcodeeditor.h"
#include "./ui_mainwindowcodeeditor.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDebug>

MainWindowCodeEditor::MainWindowCodeEditor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowCodeEditor)
{
    ui->setupUi(this);

    connect(ui->actionNew_File, &QAction::triggered, this, &MainWindowCodeEditor::onNewFileClicked);
    connect(ui->actionOpen_File, &QAction::triggered, this, &MainWindowCodeEditor::onOpenFileClicked);
    connect(ui->actionOpen_Folder, &QAction::triggered, this, &MainWindowCodeEditor::onOpenFolderClicked);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindowCodeEditor::onSaveFileClicked);
    connect(ui->actionSave_As, &QAction::triggered, this, &MainWindowCodeEditor::onSaveAsFileClicked);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindowCodeEditor::onExitClicked);
    setCentralWidget(ui->codeEditor);
}

MainWindowCodeEditor::~MainWindowCodeEditor()
{
    delete ui;
}

void MainWindowCodeEditor::onOpenFileClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open File");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream in(&file);
            ui->codeEditor->setPlainText(in.readAll());
            file.close();
            currentFilePath = fileName;
        } else {
            QMessageBox::critical(this, "Error", "Could not open file");
        }
    }
}

void MainWindowCodeEditor::onSaveFileClicked()
{
    if (currentFilePath.isEmpty()) {
        onSaveAsFileClicked();
        return;
    }
    QFile file(currentFilePath);
    if (file.open(QFile::WriteOnly | QFile::Text)) {
        QTextStream out(&file);
        out << ui->codeEditor->toPlainText();
        file.close();
    } else {
        QMessageBox::critical(this, "Error", "Could not save file");
    }
}

void MainWindowCodeEditor::onSaveAsFileClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save As");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QFile::WriteOnly | QFile::Text)) {
            QTextStream out(&file);
            out << ui->codeEditor->toPlainText();
            file.close();
            currentFilePath = fileName;
        }
    } else {
        QMessageBox::critical(this, "Error", "Could not save file");
    }
}

void MainWindowCodeEditor::onExitClicked()
{
    QApplication::quit();
}

void MainWindowCodeEditor::onNewFileClicked()
{
    ui->codeEditor->clear();
    currentFilePath.clear();
}

void MainWindowCodeEditor::onOpenFolderClicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, "Open Folder");
    if (!folderPath.isEmpty()) {
        statusBar()->showMessage("Opened folder: " + folderPath);
        qDebug() << "Opened folder:" << folderPath;
    }
}
