#ifndef LSPSETTINGSDIALOG_H
#define LSPSETTINGSDIALOG_H

#include <QDialog>
#include <QMap>

class QFormLayout;
class QLineEdit;

class LspSettingsDialog : public QDialog {
    Q_OBJECT
public:
    // принимает мапы виды language->displayName, язык->человеко читаемый (cpp->"C/C++")
    explicit LspSettingsDialog(QWidget *parent, const QMap<QString, QString>& languages, const QMap<QString, QString>& paths); 
    QMap<QString, QString> selectedPaths() const; // вовзращает обновленные пути
    
private slots:
    void onBrowse();
    
private:
    QFormLayout *m_form; // язык: paths (...)
    QMap<QString, QLineEdit*> m_lineEdits; // хранит поле ввода пути для каждого языка 
};

#endif