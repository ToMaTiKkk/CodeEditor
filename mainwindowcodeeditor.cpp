#include "mainwindowcodeeditor.h"
#include "./ui_mainwindowcodeeditor.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDebug>
#include <QDir>

MainWindowCodeEditor::MainWindowCodeEditor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindowCodeEditor)
{
    ui->setupUi(this);

    // подклчение сигналов от нажатий по пунктам меню к соответствующим функциям
    connect(ui->actionNew_File, &QAction::triggered, this, &MainWindowCodeEditor::onNewFileClicked);
    connect(ui->actionOpen_File, &QAction::triggered, this, &MainWindowCodeEditor::onOpenFileClicked);
    connect(ui->actionOpen_Folder, &QAction::triggered, this, &MainWindowCodeEditor::onOpenFolderClicked);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindowCodeEditor::onSaveFileClicked);
    connect(ui->actionSave_As, &QAction::triggered, this, &MainWindowCodeEditor::onSaveAsFileClicked);
    connect(ui->actionExit, &QAction::triggered, this, &MainWindowCodeEditor::onExitClicked);

    // установка центрального виджета, чтобы сохранить весь интерфейс
    setCentralWidget(ui->centralwidget);

    // Инициализация QFileSystemModel (древовидный вид файловой системы слева)
    fileSystemModel = new QFileSystemModel(this); // инициализация модели файловой системы
    fileSystemModel->setRootPath(QDir::homePath()); // задаем корневой путь как домашнюю папку пользователя
    ui->fileSystemTreeView->setModel(fileSystemModel); // привязываем модель к дереву файловой системы
    ui->fileSystemTreeView->setRootIndex(fileSystemModel->index(QDir::homePath())); // устанавливаем корневой индекс (начало отображения) для дерева
    // скрываем лишние столбцы, чтобы отображадась только первая колонка (имена файлов), время, размер и тд скрываются
    ui->fileSystemTreeView->hideColumn(1);
    ui->fileSystemTreeView->hideColumn(2);
    ui->fileSystemTreeView->hideColumn(3);
    // подключаем сигнал двойного клика по элементу дерева к функции, которая будет открывать файл
    connect(ui->fileSystemTreeView, &QTreeView::doubleClicked, this, &MainWindowCodeEditor::onFileSystemTreeViewDoubleClicked);
}

MainWindowCodeEditor::~MainWindowCodeEditor()
{
    delete ui; // освобождает память, выделенную под интерфейс
}

void MainWindowCodeEditor::onOpenFileClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open File"); // открытие диалогового окна для выбора файла
    if (!fileName.isEmpty()) {
        QFile file(fileName); // при непустом файле создается объект для работы с файлом
        if (file.open(QFile::ReadOnly | QFile::Text)) { // открытие файла для чтения в текстовом режиме
            // считывание всего текста из файла и устанавливание в редактор CodeEditor
            QTextStream in(&file);
            ui->codeEditor->setPlainText(in.readAll());
            // закрытие файла и записи пути в переменную
            file.close();
            currentFilePath = fileName;
        } else {
            QMessageBox::critical(this, "Error", "Could not open file");
        }
    }
}

void MainWindowCodeEditor::onSaveFileClicked()
{
    // если путь к файлу пустой, то есть файл ещё не сохранили, то вызывается функция сохранения файла под именем
    if (currentFilePath.isEmpty()) {
        onSaveAsFileClicked();
        return;
    }
    // открытие файла для запаси
    QFile file(currentFilePath);
    if (file.open(QFile::WriteOnly | QFile::Text)) {
        // запись содержимог CodeEditor в файл
        QTextStream out(&file);
        out << ui->codeEditor->toPlainText();
        file.close();
    } else {
        QMessageBox::critical(this, "Error", "Could not save file");
    }
}

void MainWindowCodeEditor::onSaveAsFileClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save As"); // открытие диалогового окна для сохранения файла под другим именем
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
    // очищения поля редактирование и очищение пути к текущему файлу
    ui->codeEditor->clear();
    currentFilePath.clear();
}

void MainWindowCodeEditor::onOpenFolderClicked()
{
    QString folderPath = QFileDialog::getExistingDirectory(this, "Open Folder"); // диалоговое окно для выбора папки
    if (!folderPath.isEmpty()) {
        QDir dir(folderPath);
        // проверка существует ли папка
        if (dir.exists()) {
            // обновление древовидной модели файловой системы, чтобы ее корневой каталог стал выбранным
            QModelIndex rootIndex = fileSystemModel->setRootPath(folderPath);
            // отображение содержимого выбранной папки
            ui->fileSystemTreeView->setRootIndex(rootIndex);
            // вывод сообщения об открытие папки в строку состояния и в консоль
            statusBar()->showMessage("Opened folder: " + folderPath);
            qDebug() << "Opened folder:" << folderPath;
        } else {
            qDebug() << "Error: выбрана невалидная папка или папка не существует: " << folderPath;
            QMessageBox::warning(this, "Warning", "Выбранная папка несуществует или не является папкой.");
        }
    }

}

void MainWindowCodeEditor::onFileSystemTreeViewDoubleClicked(const QModelIndex &index)
{
    QFileInfo fileInfo = fileSystemModel->fileInfo(index); // опредление, что было выбрано: файл или папка
    if (fileInfo.isFile()) {
        QString filePath = fileInfo.absoluteFilePath(); // если это файл, вы получаем полный путь
        QFile file(filePath);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream in(&file);
            ui->codeEditor->setPlainText(in.readAll());
            file.close();
            currentFilePath = filePath;
            statusBar()->showMessage("Opened file: " + filePath);
        } else {
            QMessageBox::critical(this, "Error", "Could not open file: " + filePath);
        }
    }
}
