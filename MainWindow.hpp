#pragma once

#include <QWidget>
#include <QLocale>

#include <optional>

class QTableWidget;
class TitleBar;
class DpsLogic;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(DpsLogic &dpsLogic);
    ~MainWindow();

private:
    void dpsLogicUpdate();

    void updateTitle();

    void setHeight();

    void setStayOnTop(bool onTop);

signals:
    void packetCaptureReset();

private:
    const QString m_constantTitle;

    DpsLogic &m_dpsLogic;

    TitleBar *const m_titleBar;
    QTableWidget *const m_players;

    QLocale m_cLocale;

    std::optional<uint32_t> m_time;
    uint32_t m_worldId = 0;

    std::array<QColor, 9> m_colors;
};

