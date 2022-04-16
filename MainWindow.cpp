#include "MainWindow.hpp"
#include "TitleBar.hpp"
#include "DpsLogic.hpp"

#include <QGuiApplication>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QItemDelegate>
#include <QBoxLayout>
#include <QWindow>
#include <QFontMetrics>
#include <QMenu>
#include <QTime>
#include <QLinearGradient>
#include <QPainter>
#include <QDebug>

using namespace std;

constexpr auto g_nCols = 10;

class ItemDelegate : public QItemDelegate
{
    void drawDisplay(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect, const QString &text) const override
    {
        // Draw text shadow
        QStyleOptionViewItem optionShadow = option;
        optionShadow.palette.setColor(QPalette::Text, optionShadow.palette.color(QPalette::Base));
        painter->save();
        painter->translate(1.0, 1.0);
        QItemDelegate::drawDisplay(painter, optionShadow, rect, text);
        painter->restore();

        // Draw text
        QItemDelegate::drawDisplay(painter, option, rect, text);
    }
    void drawFocus(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect) const override
    {
        Q_UNUSED(painter)
        Q_UNUSED(option)
        Q_UNUSED(rect)
    }
};

MainWindow::MainWindow(DpsLogic &dpsLogic)
    : m_constantTitle(QGuiApplication::applicationDisplayName() + " v" + QGuiApplication::applicationVersion())
    , m_dpsLogic(dpsLogic)
    , m_titleBar(new TitleBar(this))
    , m_players(new QTableWidget(this))
    , m_cLocale(QLocale::C)
{
    const QStringList labels {
        tr("NAME"),
        tr("DPS"),
        tr("DMG%"),
        tr("DMG"),
        tr("DMG RCV"),
        tr("HIT/s"),
        tr("COMBO"),
        tr("MISS%"),
        tr("CRIT%"),
        tr("SS%"),
    };

    const int fontMaxWidth = QFontMetrics(font()).maxWidth();

    const int minNameWidth = fontMaxWidth * 12;
    const int minPercentWidth = fontMaxWidth * 6;
    const int minHitPerSecWidth = fontMaxWidth * 5;
    const int minComboWidth = fontMaxWidth * 4;

    setStayOnTop(true);

    auto menu = new QMenu(this);
    auto stayOnTopAction = menu->addAction(tr("Stay on top"));
    stayOnTopAction->setCheckable(true);
    stayOnTopAction->setChecked(true);
    auto resizeAction = menu->addAction(tr("Resize"));
    menu->addSeparator();
    auto suspendAction = menu->addAction(tr("Suspend"));
    auto resumeAction = menu->addAction(tr("Resume"));
    auto resetAction = menu->addAction(tr("Reset"));
    menu->addSeparator();
    menu->addAction(tr("Close"), this, &MainWindow::close);

    m_players->setItemDelegate(new ItemDelegate);

    m_players->setWordWrap(false);
    m_players->setTextElideMode(Qt::ElideNone);
    m_players->setColumnCount(g_nCols);

    m_players->verticalHeader()->hide();
    m_players->setVerticalScrollMode(QTableWidget::ScrollPerPixel);
    m_players->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto horizontalHeader = m_players->horizontalHeader();
    horizontalHeader->setCascadingSectionResizes(true);
    m_players->setHorizontalScrollMode(QTableWidget::ScrollPerPixel);
    m_players->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_players->setHorizontalHeaderLabels(labels);

    auto setFixedColumnWidth = [&](int logicalIdx, int minContentWidth) {
        horizontalHeader->setSectionResizeMode(logicalIdx, QHeaderView::Fixed);
        horizontalHeader->resizeSection(logicalIdx, max<int>(minContentWidth, labels[logicalIdx].length() * fontMaxWidth) + 8);
    };
    setFixedColumnWidth(0, minNameWidth);
    setFixedColumnWidth(5, minHitPerSecWidth);
    setFixedColumnWidth(6, minComboWidth);
    for (int logicalIdx : {2, 7, 8, 9})
        setFixedColumnWidth(logicalIdx, minPercentWidth);

    m_cLocale.setNumberOptions(QLocale::DefaultNumberOptions);

    m_colors = {
        QColor(153, 153, 153),
        QColor(247, 142, 59),
        QColor(59, 147, 247),
        QColor(247, 59, 156),
        QColor(247, 190, 59),
        QColor(161, 59, 247),
        QColor(223, 1, 1),
        QColor(138, 2, 4),
        QColor(118, 206, 158),
    };

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_titleBar);
    layout->addWidget(m_players);

    connect(stayOnTopAction, &QAction::triggered,
            this, [=](bool checked) {
        setStayOnTop(checked);
#ifndef Q_OS_WIN
        resizeAction->setEnabled(!checked);
#endif
        show();
    });
    connect(resizeAction, &QAction::triggered,
            this, [=](bool checked) {
        Q_UNUSED(checked)
        window()->windowHandle()->startSystemResize(Qt::RightEdge);
    });
    connect(suspendAction, &QAction::triggered,
            this, [=](bool checked) {
        Q_UNUSED(checked)
        m_dpsLogic.suspend(false);
    });
    connect(resumeAction, &QAction::triggered,
            this, [=](bool checked) {
        Q_UNUSED(checked)
        m_dpsLogic.resume();
    });
    connect(resetAction, &QAction::triggered,
            this, [=](bool checked) {
        Q_UNUSED(checked)
        emit packetCaptureReset();
        m_dpsLogic.reset();
        dpsLogicUpdate();
    });

    connect(menu, &QMenu::aboutToShow,
            this, [=] {
        suspendAction->setVisible(m_dpsLogic.isValid() && (!m_dpsLogic.isSuspended() || m_dpsLogic.isAutoResume()));
        resumeAction->setVisible(m_dpsLogic.isValid() && m_dpsLogic.isSuspended());
    });

    connect(m_titleBar, &TitleBar::customContextMenuRequested,
            this, [=](const QPoint &pos) {
        menu->popup(m_titleBar->mapToGlobal(pos));
    });

    connect(&dpsLogic, &DpsLogic::update,
            this, &MainWindow::dpsLogicUpdate);

    connect(horizontalHeader, &QHeaderView::sectionResized,
            this, [this](int logicalIndex, int oldSize, int newSize) {
        Q_UNUSED(logicalIndex)
        Q_UNUSED(newSize)
        Q_UNUSED(oldSize)
        // To update graph
        dpsLogicUpdate();
    });

    updateTitle();

    int width = 4;
    for (int i = 0; i < g_nCols; ++i)
        width += m_players->columnWidth(i);

    setHeight();
    resize(width, height());
}
MainWindow::~MainWindow()
{
}

void MainWindow::dpsLogicUpdate()
{
    bool doUpdateTitle = false;

    double time = m_dpsLogic.getTime();
    if (!qIsNaN(time))
    {
        const uint32_t timeInt = time;
        if (m_time != timeInt)
        {
            m_time = timeInt;
            doUpdateTitle = true;
        }
    }
    else
    {
        if (m_time.has_value())
        {
            m_time.reset();
            doUpdateTitle = true;
        }
        time = 1.0;
    }

    const uint32_t worldId = m_dpsLogic.getWorldId();
    if (m_worldId != worldId)
    {
        m_worldId = worldId;
        doUpdateTitle = true;
    }

    if (doUpdateTitle)
        updateTitle();

    const int nRows = m_dpsLogic.getNumPlayers();
    if (m_players->rowCount() != nRows)
    {
        m_players->setRowCount(nRows);
        for (int r = 0; r < nRows; ++r)
        {
            for (int c = 0; c < g_nCols; ++c)
            {
                if (m_players->item(r, c))
                    continue;

                auto item = new QTableWidgetItem;
                item->setFlags(Qt::ItemIsEnabled);
                m_players->setItem(r, c, item);
            }
        }
        m_players->resizeRowsToContents();
        setHeight();
    }

    struct RowWidthInfo
    {
        int cellWidth;
        int widthTo;
    };

    vector<QTableWidgetItem *> cellItem(g_nCols);

    vector<RowWidthInfo> rowsWidthInfo(g_nCols);
    int rowWidth = 0;
    for (int c = 0; c < g_nCols; ++c)
    {
        const int colW = m_players->columnWidth(c);
        rowWidth += colW;
        rowsWidthInfo[c] = {colW, rowWidth};
    }

    double firstRowDamage = 0.0;

    m_dpsLogic.iterate([&](uint32_t row, const QString &playerName, uint8_t characterClass, uint64_t totalDamage, const DpsLogic::PlayerStats &playerStats) {
        const auto teamDamage = static_cast<double>(playerStats.damage) / totalDamage;

        if (row == 0)
            firstRowDamage = teamDamage;

        const auto widthToFill = rowWidth * teamDamage / firstRowDamage;

        for (int c = 0; c < g_nCols; ++c)
        {
            auto widthFillRatio = 1.0 - (rowsWidthInfo[c].widthTo - widthToFill) / rowsWidthInfo[c].cellWidth;
            static_assert(is_floating_point_v<decltype(widthFillRatio)>);

            cellItem[c] = m_players->item(row, c);

            if (nRows == 1 && characterClass == 0)
                widthFillRatio = 0.0; // Don't use color for single unknown character

            if (widthFillRatio >= 1.0)
            {
                cellItem[c]->setBackground(m_colors[characterClass]);
            }
            else if (widthFillRatio > 0.0)
            {
                QLinearGradient grad(0.0, 0.0, rowsWidthInfo[c].cellWidth, 0.0);
                grad.setColorAt(widthFillRatio, m_colors[characterClass]);
                grad.setColorAt(widthFillRatio + numeric_limits<qreal>::epsilon(), QColor());
                cellItem[c]->setBackground(grad);
            }
            else
            {
                cellItem[c]->setBackground(QBrush());
            }
        }

        cellItem[0]->setText(playerName);
        cellItem[1]->setText(QString("%1K").arg(m_cLocale.toString(playerStats.damage / time / 1e3, 'f', 0)));
        cellItem[2]->setText(QString("%1%").arg(teamDamage * 100.0, 0, 'f', 1));
        cellItem[3]->setText(QString("%1K").arg(m_cLocale.toString(playerStats.damage / 1e3, 'f', 0)));
        cellItem[4]->setText(QString("%1").arg(m_cLocale.toString(playerStats.damageReceived)));
        cellItem[5]->setText(QString("%1").arg(m_cLocale.toString(playerStats.hits / time, 'f', 2)));
        cellItem[6]->setText(QString("%1").arg(playerStats.maxCombo));
        cellItem[7]->setText(QString("%1%").arg(playerStats.misses * 100.0 / playerStats.hits, 0, 'f', 1));
        cellItem[8]->setText(QString("%1%").arg(playerStats.crits * 100.0 / playerStats.hits, 0, 'f', 1));
        cellItem[9]->setText(QString("%1%").arg(playerStats.soulstones * 100.0 / playerStats.hits, 0, 'f', 1));
    });
}

void MainWindow::updateTitle()
{
    QString title;
    if (m_worldId > 0)
    {
        title += QString("%1 - ").arg(m_worldId);
    }
    if (m_time.has_value())
    {
        title += QString("%1 - ").arg(QTime(0, 0).addSecs(m_time.value()).toString("mm:ss"));
    }
    title += m_constantTitle;
    m_titleBar->setText(title);
}

void MainWindow::setHeight()
{
    const int nRows = m_titleBar->height() + m_players->rowCount();
    int height = m_players->horizontalHeader()->height() + 3;
    for (int i = 0; i < nRows; ++i)
        height += m_players->rowHeight(i) + 1;
    setFixedHeight(height);
}

void MainWindow::setStayOnTop(bool onTop)
{
    if (onTop)
    {
        if (QGuiApplication::platformName() == "xcb")
            setWindowFlags(Qt::BypassWindowManagerHint);
        else
            setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    }
    else
    {
        setWindowFlags(Qt::FramelessWindowHint);
    }
}
