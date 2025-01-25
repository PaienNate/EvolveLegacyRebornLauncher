
#ifndef EVOLVELEGACYREBORNLAUNCHER_DRAGGABLELABEL_H
#define EVOLVELEGACYREBORNLAUNCHER_DRAGGABLELABEL_H


#include <QLabel>
#include <QMouseEvent>

class DraggableLabel : public QLabel {
Q_OBJECT

public:
    explicit DraggableLabel(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool m_dragging;
    QPoint m_dragStartPosition;
};


#endif //EVOLVELEGACYREBORNLAUNCHER_DRAGGABLELABEL_H
