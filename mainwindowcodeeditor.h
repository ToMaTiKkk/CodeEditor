#ifndef MAINWINDOWCODEEDITOR_H
#define MAINWINDOWCODEEDITOR_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindowCodeEditor;
}
QT_END_NAMESPACE

class MainWindowCodeEditor : public QMainWindow
{
    Q_OBJECT

public:
    MainWindowCodeEditor(QWidget *parent = nullptr);
    ~MainWindowCodeEditor();

private slots:
    void onOpenFileClicked();
    void onSaveFileClicked();
    void onSaveAsFileClicked();
    void onExitClicked();
    void onNewFileClicked();
    void onOpenFolderClicked();

private:
    Ui::MainWindowCodeEditor *ui;
    QString currentFilePath;
};
#endif // MAINWINDOWCODEEDITOR_H
