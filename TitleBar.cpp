#include "TitleBar.hpp"

#include <QDebug>
#include <qevent.h>

TitleBar::TitleBar(QWidget *parent)
    : QLabel(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);

    setAutoFillBackground(true);
    setPalette(Qt::black);

    setIndent(3);
    setFixedHeight(18);
}
TitleBar::~TitleBar()
{
}

void TitleBar::mousePressEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::MouseButton::LeftButton)
    {
        m_pressed = true;
        m_pressPos = e->position();
        setCursor(Qt::SizeAllCursor);
    }
    QWidget::mousePressEvent(e);
}
void TitleBar::mouseMoveEvent(QMouseEvent *e)
{
    if (m_pressed)
    {
        auto rootWin = window();
        auto frameTopLeft = rootWin->geometry().topLeft() - rootWin->frameGeometry().topLeft();
        rootWin->move(mapToGlobal(e->position() - m_pressPos - frameTopLeft).toPoint());
    }
    QWidget::mouseMoveEvent(e);
}
void TitleBar::mouseReleaseEvent(QMouseEvent *e)
{
    if (!(e->buttons() & Qt::MouseButton::LeftButton))
    {
        m_pressed = false;
        unsetCursor();
    }
    QWidget::mouseReleaseEvent(e);
}
