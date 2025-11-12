#include "defaultprintrenderer.h"

#include "printcontext.h"
#include "../core/labelelement.h"

#include <QtCore/QObject>
#include <QtCore/QRectF>
#include <QtCore/QMarginsF>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtWidgets/QGraphicsScene>

#include <algorithm>

namespace {
constexpr double kMillimetrePerInch = 25.4;

double mmToDevice(double mm, double dpi)
{
    return (dpi / kMillimetrePerInch) * mm;
}

QRectF resolveDesignRect(const QList<labelelement*> &elements,
                         const PrintContext &context)
{
    if (context.labelSizePixels.width() > 0.0 && context.labelSizePixels.height() > 0.0) {
        return QRectF(0, 0, context.labelSizePixels.width(), context.labelSizePixels.height());
    }

    if (context.sourceScene) {
        return context.sourceScene->sceneRect();
    }

    QRectF bounds;
    bool first = true;
    for (labelelement *element : elements) {
        if (!element) {
            continue;
        }
        if (QGraphicsItem *item = element->getItem()) {
            const QRectF rect = item->sceneBoundingRect();
            if (first) {
                bounds = rect;
                first = false;
            } else {
                bounds = bounds.united(rect);
            }
        }
    }

    if (!first) {
        return bounds;
    }

    return QRectF(0, 0, 100, 100);
}
}

bool DefaultPrintRenderer::render(QPainter &painter,
                                  const QList<labelelement*> &elements,
                                  const PrintContext &context,
                                  QString *errorMessage)
{
    if (!context.sourceScene) {
        if (errorMessage) {
            *errorMessage = QObject::tr("打印上下文缺少源场景");
        }
        return false;
    }

    const QRectF designRect = resolveDesignRect(elements, context);
    if (designRect.width() <= 0 || designRect.height() <= 0) {
        if (errorMessage) {
            *errorMessage = QObject::tr("无效的打印区域");
        }
        return false;
    }

    painter.save();

    const double dpiX = painter.device()->logicalDpiX();
    const double dpiY = painter.device()->logicalDpiY();

    const QMarginsF marginsMM = context.contentMargins;
    const double offsetX = mmToDevice(marginsMM.left(), dpiX);
    const double offsetY = mmToDevice(marginsMM.top(), dpiY);
    painter.translate(offsetX, offsetY);

    double scaleX = 1.0;
    double scaleY = 1.0;
    if (context.labelSizeMM.width() > 0 && context.labelSizePixels.width() > 0) {
        scaleX = mmToDevice(context.labelSizeMM.width(), dpiX) / context.labelSizePixels.width();
    }
    if (context.labelSizeMM.height() > 0 && context.labelSizePixels.height() > 0) {
        scaleY = mmToDevice(context.labelSizeMM.height(), dpiY) / context.labelSizePixels.height();
    }

    painter.scale(scaleX, scaleY);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF targetRect(0, 0, designRect.width(), designRect.height());

    const double clipRadius = std::min({context.labelCornerRadiusPixels,
                                        targetRect.width() / 2.0,
                                        targetRect.height() / 2.0});
    if (clipRadius > 0.0) {
        QPainterPath clipPath;
        clipPath.addRoundedRect(targetRect, clipRadius, clipRadius);
        painter.setClipPath(clipPath, Qt::ReplaceClip);
    }

    painter.fillRect(targetRect, Qt::white);

    context.sourceScene->render(&painter, targetRect, designRect, Qt::IgnoreAspectRatio);

    painter.restore();
    return true;
}
