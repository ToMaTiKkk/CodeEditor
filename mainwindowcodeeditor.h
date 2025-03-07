#ifndef MAINWINDOWCODEEDITOR_H
#define MAINWINDOWCODEEDITOR_H

#include "cursorwidget.h"
#include "linehighlightwidget.h"
#include "cpphighlighter.h"
#include <QMainWindow>
#include <QFileSystemModel>
#include <QtWebSockets/QWebSocket>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUuid>
#include <QMap>
#include <QScrollBar>

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

private slots: // функции, которые будут вызваны в ответ на определенные события
    bool eventFilter(QObject *obj, QEvent *event) override;

    void onOpenFileClicked(); // вызывается при вызове пункта мен дл открытия файлы
    void onSaveFileClicked(); // сохранение файла
    void onSaveAsFileClicked(); // сохрание под новым именем
    void onExitClicked(); // для выхода из приложения
    void onNewFileClicked(); // для создания нового файла (простое очищение редактора, файл надо будет сохранять вручную)
    void onOpenFolderClicked(); // для открытия папки и отображения ее в дереве

    void onFileSystemTreeViewDoubleClicked(const QModelIndex &index); // когда пользователь дважды кликает по файлу в дереве, то оно открывается в редакторе

    void onContentsChange(int position, int charsRemoved, int charsAdded);
    void onTextMessageReceived(const QString &message);
    void onDisconnected();

    void onCursorPositionChanged();

    void onVerticalScrollBarValueChanged(int value);
    void updateRemoteWidgetGeometry(QWidget* widget, int position);
    void updateLineHighlight(const QString& senderId, int position);

private:
    Ui::MainWindowCodeEditor *ui; // доступ к элементами интерфейса .ui
    QString currentFilePath; // хранение пути к текущему открытому файлу, используется, чтобы знать куда записывать изменения
    QFileSystemModel *fileSystemModel; // добавление указателя на QFileSystemmodel (древовидный вид файловый системы слева)
    QWebSocket *socket;
    bool loadingFile = false;
    QString m_clientId; // хранение уникального идентификатора клиента, пересоздается при каждом запуске программы
    QMap<QString, CursorWidget*> remoteCursors; // словарь с курсора клиентов, ключ - айди, значение - виджет курсора
    QString m_username;
    QMap<QString, LineHighlightWidget*> remoteLineHighlights; // хранение подсветки строки, где курсор пользователя, uuid - подсветка
    QList<QJsonObject> cursorUpdates; // хранение последних обновлений позиций курсора
    QMap<QString, int> lastCursorPositions; // хранение позиций всех курсоров других пользователей
    CppHighlighter *highlighter;
};
#endif // MAINWINDOWCODEEDITOR_H
