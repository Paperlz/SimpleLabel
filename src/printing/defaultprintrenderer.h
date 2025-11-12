#ifndef DEFAULTPRINTRENDERER_H
#define DEFAULTPRINTRENDERER_H

#include "printrenderer.h"

class DefaultPrintRenderer : public PrintRenderer
{
public:
    bool render(QPainter &painter,
                const QList<labelelement*> &elements,
                const PrintContext &context,
                QString *errorMessage) override;
};

#endif // DEFAULTPRINTRENDERER_H
