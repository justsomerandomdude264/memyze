#ifndef MEMORYBAR_H
#define MEMORYBAR_H

#include <QWidget>

class MemoryBar : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryBar(QWidget *parent = nullptr);

    void setValues(int privateKB, int stackKB, int imageKB, int mappedKB);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_privateKB = 0;
    int m_stackKB = 0;
    int m_imageKB = 0;
    int m_mappedKB = 0;

    QString formatMemory(qint64 kb) const;
};

#endif // MEMORYBAR_H
