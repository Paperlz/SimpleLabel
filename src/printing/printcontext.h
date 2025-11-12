#ifndef PRINTCONTEXT_H
#define PRINTCONTEXT_H

#include <QtPrintSupport/QPrinter>
#include <QtGui/QPageLayout>
#include <QtCore/QMarginsF>
#include <QtCore/QSizeF>

class QGraphicsScene;

struct PrintContext {
    QPrinter *printer = nullptr;
    QPageLayout pageLayout;
    QMarginsF contentMargins;
    QSizeF labelSizeMM;
    QSizeF labelSizePixels;
    QGraphicsScene *sourceScene = nullptr;
    double labelCornerRadiusPixels = 0.0;
};

#endif // PRINTCONTEXT_H
