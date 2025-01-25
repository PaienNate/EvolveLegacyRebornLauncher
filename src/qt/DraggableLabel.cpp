#include "DraggableLabel.h"

DraggableLabel::DraggableLabel(QWidget *parent)
        : QLabel(parent), m_dragging(false) {}

void DraggableLabel::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStartPosition = QPoint(event->globalPosition().x() - parentWidget()->geometry().topLeft().x(),
                                     event->globalPosition().y() - parentWidget()->geometry().topLeft().y());
        event->accept();
    }
}

void DraggableLabel::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        QWidget *parent = parentWidget();
        if (parent) {
            parent->move(event->globalPosition().x() - m_dragStartPosition.x(),
                         event->globalPosition().y() - m_dragStartPosition.y());
        }
        event->accept();
    }
}

void DraggableLabel::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
}