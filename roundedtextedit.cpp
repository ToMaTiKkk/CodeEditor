#include "roundedtextedit.h"

RoundedTextEdit::RoundedTextEdit(QWidget *parent)
    : QTextEdit(parent) {
    setReadOnly(true);
}
void RoundedTextEdit::setUsername(const QString &username) {
    m_username = username;
}
void RoundedTextEdit::paintEvent(QPaintEvent *event) {
    QTextEdit::paintEvent(event); // Сначала стандартная отрисовка текста

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);

    QTextFrame::iterator it;
    for (it = document()->rootFrame()->begin(); !it.atEnd(); ++it) {
        QTextFrame *frame = it.currentFrame();
        if (!frame) continue;

        QRectF frameRect = document()->documentLayout()->frameBoundingRect(frame);
        QRectF visibleRect = frameRect.translated(-horizontalScrollBar()->value(),
                                                  -verticalScrollBar()->value());

        // Получаем имя пользователя из свойств фрейма
        QString username = frame->frameFormat().property(QTextFormat::UserProperty).toString();
        bool isCurrentUser = (username == m_username);

        painter.save();
        QPainterPath path;
        path.addRoundedRect(visibleRect, 20, 20);

        painter.fillPath(path, isCurrentUser
                                   ? QColor(139, 0, 139, 60)  // Фиолетовый для ваших сообщений
                                   : QColor(60, 60, 60, 60));  // Серый для других

        painter.setPen(QPen(isCurrentUser ? Qt::white : Qt::lightGray, 1));
        painter.drawPath(path);
        painter.restore();
    }
}
