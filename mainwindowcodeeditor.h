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

private:
    Ui::MainWindowCodeEditor *ui;
};
#endif // MAINWINDOWCODEEDITOR_H
