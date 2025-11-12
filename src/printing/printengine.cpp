#include "printengine.h"

#include "printrenderer.h"
#include "printcontext.h"
#include "../core/labelelement.h"

#include <QtGui/QPainter>
#include <QtCore/QSize>
#include <QtGui/QImage>
#include <QtGui/QPageLayout>
#include <QtCore/QMarginsF>
#include <QtCore/QObject>

#include <cmath>

PrintEngine::PrintEngine() = default;

PrintEngine::PrintEngine(std::unique_ptr<PrintRenderer> renderer)
    : m_renderer(std::move(renderer))
{
}

PrintEngine::~PrintEngine() = default;

void PrintEngine::setRenderer(std::unique_ptr<PrintRenderer> renderer)
{
    m_renderer = std::move(renderer);
}

PrintRenderer *PrintEngine::renderer() const
{
    return m_renderer.get();
}

bool PrintEngine::printOnce(const QList<labelelement*> &elements,
                            const PrintContext &context,
                            QString *errorMessage)
{
    if (!m_renderer || !context.printer) {
        if (errorMessage) {
            *errorMessage = QObject::tr("打印引擎尚未配置渲染器或打印机");
        }
        return false;
    }

    QPainter painter(context.printer);
    if (!painter.isActive()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("无法在打印机上初始化绘制");
        }
        return false;
    }

    return m_renderer->render(painter, elements, context, errorMessage);
}

namespace {
constexpr double kMetersPerInch = 0.0254;

int dpiToDotsPerMeter(qreal dpi)
{
    if (dpi <= 0.0) {
        return 0;
    }
    return static_cast<int>(std::lround(dpi / kMetersPerInch));
}
}

QImage PrintEngine::renderPreview(const QList<labelelement*> &elements,
                                  const PrintContext &context,
                                  const QSize &targetSize,
                                  QString *errorMessage,
                                  qreal dpiX,
                                  qreal dpiY)
{
    if (!m_renderer) {
        if (errorMessage) {
            *errorMessage = QObject::tr("打印引擎尚未配置渲染器");
        }
        return QImage();
    }

    QImage preview(targetSize, QImage::Format_ARGB32_Premultiplied);
    const int dotsPerMeterX = dpiToDotsPerMeter(dpiX);
    const int dotsPerMeterY = dpiToDotsPerMeter(dpiY);
    if (dotsPerMeterX > 0) {
        preview.setDotsPerMeterX(dotsPerMeterX);
    }
    if (dotsPerMeterY > 0) {
        preview.setDotsPerMeterY(dotsPerMeterY);
    }
    if (dotsPerMeterX > 0 && dotsPerMeterY <= 0) {
        preview.setDotsPerMeterY(dotsPerMeterX);
    } else if (dotsPerMeterY > 0 && dotsPerMeterX <= 0) {
        preview.setDotsPerMeterX(dotsPerMeterY);
    } else if (dotsPerMeterX <= 0 && dotsPerMeterY <= 0) {
        constexpr qreal kDefaultDpi = 144.0;
        const int defaultDotsPerMeter = dpiToDotsPerMeter(kDefaultDpi);
        preview.setDotsPerMeterX(defaultDotsPerMeter);
        preview.setDotsPerMeterY(defaultDotsPerMeter);
    }
    preview.fill(Qt::white);

    QPainter painter(&preview);
    if (!painter.isActive()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("无法创建预览画布");
        }
        return QImage();
    }

    if (!m_renderer->render(painter, elements, context, errorMessage)) {
        return QImage();
    }

    return preview;
}
