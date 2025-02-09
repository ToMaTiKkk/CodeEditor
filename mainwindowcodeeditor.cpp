#include "mainwindowcodeeditor.h"
#include "./ui_mainwindowcodeeditor.h"

MainWindowCodeEditor::MainWindowCodeEditor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowCodeEditor)
{
    ui->setupUi(this);
}

MainWindowCodeEditor::~MainWindowCodeEditor()
{
    delete ui;
}
