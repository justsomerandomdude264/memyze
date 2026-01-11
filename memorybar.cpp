#include "memorybar.h"
#include <QPainter>
#include <QFontMetrics>

// Constructor to set the min height to 300 and size policy accordingly
MemoryBar::MemoryBar(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

// Setter (this is called whenever we have new memory data from memory analyzer)
void MemoryBar::setValues(int privateKB, int stackKB, int imageKB, int mappedKB)
{
    m_privateKB = privateKB;
    m_stackKB   = stackKB;
    m_imageKB   = imageKB;
    m_mappedKB  = mappedKB;
    update();
}

// Convert KB to MB if large enough and MB to GB if large enough
QString MemoryBar::formatMemory(qint64 kb) const
{
    if (kb >= 1024LL * 1024LL)
        return QString::number(kb / 1024.0 / 1024.0, 'f', 2) + " GB";
    if (kb >= 1024)
        return QString::number(kb / 1024.0, 'f', 1) + " MB";
    return QString::number(kb) + " KB";
}

// Main paint event
void MemoryBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Styling constants
    const int margin = 10;
    const int barHeight = 20;
    const int rowHeight = 30; // Height for each individual bar row
    const int textWidth = 80;  // Space for the label (like Private)
    const int valueWidth = 100; // Space for "5 MB (5%)"

    static const QRgb colorGreen  = 0x66BB6A;
    static const QRgb colorBlue   = 0x42A5F5;
    static const QRgb colorYellow = 0xFFCA28;
    static const QRgb colorPurple = 0xAB47BC;

    qint64 total = m_privateKB + m_stackKB + m_imageKB + m_mappedKB;
    if (total <= 0) return;

    // Define the data for easier looping
    struct MemType { QString name; int value; QColor color; };
    QList<MemType> types = {
        {"Image",   m_imageKB,   QColor(colorGreen)},
        {"Private", m_privateKB, QColor(colorBlue)},
        {"Stack",   m_stackKB,   QColor(colorYellow)},
        {"Mapped",  m_mappedKB,  QColor(colorPurple)}
    };

    int currentY = margin;

    // ### PROPORTIONAL SUMMARY BAR ###
    // Background rect
    int availableWidth = width() - (margin * 2);
    QRectF totalTrack(margin, margin, availableWidth, barHeight);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(45, 45, 45));
    p.drawRoundedRect(totalTrack, barHeight / 2, barHeight / 2);

    // Filled rects
    qreal currentX = totalTrack.left();
    for (const auto &type : types) {
        if (type.value <= 0) continue;
        qreal segW = (qreal(type.value) / total) * totalTrack.width();
        p.setBrush(type.color);
        p.drawRoundedRect(QRectF(currentX, currentY, segW, barHeight), barHeight / 2, barHeight / 2);
        currentX += segW;
    }

    currentY += barHeight + 25; // Gap bw total and individual bars

    // ### INDIVIDUAL TYPE BARS ###
    QFont labelFont = font();
    labelFont.setPointSizeF(9);

    for (const auto &type : types) {
        double percent = (qreal(type.value) / total) * 100.0;

        // Label
        p.setFont(labelFont);
        p.setPen(QColor(200, 200, 200));
        p.drawText(margin, currentY, textWidth, rowHeight, Qt::AlignVCenter, type.name);

        // Individual bar rect (track)
        int barStartX = margin + textWidth;
        int barAvailableWidth = width() - barStartX - valueWidth - margin;
        QRectF indTrack(barStartX, currentY + (rowHeight / 2) - (barHeight / 4), barAvailableWidth, barHeight / 2);

        p.setBrush(QColor(45, 45, 45));
        p.drawRoundedRect(indTrack, 2, 2);

        // Individual bar fill
        qreal fillW = (qreal(type.value) / total) * indTrack.width();
        p.setBrush(type.color);
        p.drawRoundedRect(QRectF(indTrack.left(), indTrack.top(), fillW, indTrack.height()), 2, 2);

        // Put value and percentage
        QString valStr = formatMemory(type.value) + QString(" (%1%)").arg(percent, 0, 'f', 1);
        p.setPen(Qt::white);
        p.drawText(indTrack.right() + 10, currentY, valueWidth, rowHeight, Qt::AlignVCenter | Qt::AlignLeft, valStr);

        currentY += rowHeight;
    }

    // ### TOTAL'S FOOTER ###
    currentY += 10;
    p.setPen(Qt::gray);
    QFont footerFont = font();
    footerFont.setBold(true);
    p.setFont(footerFont);
    p.drawText(margin, currentY, width() - (margin * 2), rowHeight, Qt::AlignCenter, "TOTAL USAGE: " + formatMemory(total));
}
