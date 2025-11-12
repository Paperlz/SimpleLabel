#ifndef BATCHPRINTMANAGER_H
#define BATCHPRINTMANAGER_H

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QList>
#include <memory>

class QGraphicsItem;
class labelelement;
class PrintEngine;
struct PrintContext;

class BatchPrintManager : public QObject
{
    Q_OBJECT

public:
    explicit BatchPrintManager(PrintEngine *engine, QObject *parent = nullptr);

    void setEngine(PrintEngine *engine);
    PrintEngine *engine() const;

    void setElements(const QList<labelelement*> &elements);
    const QList<labelelement*> &elements() const;

    bool execute(const PrintContext &context, QString *errorMessage = nullptr);
    bool execute(const PrintContext &context, int firstRecord, int lastRecord, QString *errorMessage);

signals:
    void recordPrinted(int index);

private:
    PrintEngine *m_engine = nullptr;
    QList<labelelement*> m_elements;
};

#endif // BATCHPRINTMANAGER_H
