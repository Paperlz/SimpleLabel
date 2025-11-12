#ifndef PRINTENGINE_H
#define PRINTENGINE_H

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSize>
#include <QtGui/QImage>
#include <memory>

class labelelement;
class PrintRenderer;
struct PrintContext;
class PrintEngine
{
public:
    PrintEngine();
    explicit PrintEngine(std::unique_ptr<PrintRenderer> renderer);
    ~PrintEngine();

    void setRenderer(std::unique_ptr<PrintRenderer> renderer);
    PrintRenderer *renderer() const;

    bool printOnce(const QList<labelelement*> &elements,
                   const PrintContext &context,
                   QString *errorMessage = nullptr);

    QImage renderPreview(const QList<labelelement*> &elements,
                         const PrintContext &context,
                         const QSize &targetSize,
                         QString *errorMessage = nullptr,
                         qreal dpiX = 0.0,
                         qreal dpiY = 0.0);

private:
    std::unique_ptr<PrintRenderer> m_renderer;
};

#endif // PRINTENGINE_H
