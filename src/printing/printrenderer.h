#ifndef PRINTRENDERER_H
#define PRINTRENDERER_H

#include <QtCore/QList>
#include <QtCore/QString>

class QPainter;
class labelelement;
struct PrintContext;

class PrintRenderer
{
public:
    virtual ~PrintRenderer() = default;

    virtual bool render(QPainter &painter,
                        const QList<labelelement*> &elements,
                        const PrintContext &context,
                        QString *errorMessage) = 0;
};

#endif // PRINTRENDERER_H
