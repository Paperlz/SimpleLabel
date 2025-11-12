#include "batchprintmanager.h"

#include "printengine.h"
#include "printcontext.h"
#include "printrenderer.h"
#include "../core/labelelement.h"
#include "../core/datasource.h"
#include "../core/textelement.h"
#include "../core/barcodeelement.h"
#include "../core/qrcodeelement.h"

#include <QtCore/QVector>
#include <QtGui/QPainter>

namespace {

struct DataBinding
{
    labelelement *element = nullptr;
    std::shared_ptr<DataSource> source;
    TextElement *text = nullptr;
    BarcodeElement *barcode = nullptr;
    QRCodeElement *qrcode = nullptr;

    bool applyRecord(int index) const
    {
        if (text) {
            return text->applyDataSourceRecord(index);
        }
        if (barcode) {
            return barcode->applyDataSourceRecord(index);
        }
        if (qrcode) {
            return qrcode->applyDataSourceRecord(index);
        }
        return true;
    }

    void restore() const
    {
        if (text) {
            text->restoreOriginalData();
        } else if (barcode) {
            barcode->restoreOriginalData();
        } else if (qrcode) {
            qrcode->restoreOriginalData();
        }
    }
};

}

BatchPrintManager::BatchPrintManager(PrintEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

void BatchPrintManager::setEngine(PrintEngine *engine)
{
    m_engine = engine;
}

PrintEngine *BatchPrintManager::engine() const
{
    return m_engine;
}

void BatchPrintManager::setElements(const QList<labelelement*> &elements)
{
    m_elements = elements;
}

const QList<labelelement*> &BatchPrintManager::elements() const
{
    return m_elements;
}

bool BatchPrintManager::execute(const PrintContext &context, QString *errorMessage)
{
    return execute(context, 0, -1, errorMessage);
}

bool BatchPrintManager::execute(const PrintContext &context, int firstRecord, int lastRecord, QString *errorMessage)
{
    if (!m_engine) {
        if (errorMessage) {
            *errorMessage = tr("批量打印缺少打印引擎实例");
        }
        return false;
    }

    if (m_elements.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("没有可打印的元素");
        }
        return false;
    }

    if (!context.printer) {
        if (errorMessage) {
            *errorMessage = tr("批量打印需要有效的打印机实例");
        }
        return false;
    }

    QList<labelelement*> printable;
    printable.reserve(m_elements.size());
    for (labelelement *element : m_elements) {
        if (element) {
            printable.append(element);
        }
    }

    if (printable.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("没有可打印的元素");
        }
        return false;
    }

    QVector<DataBinding> bindings;
    bindings.reserve(printable.size());

    int recordCount = -1;
    for (labelelement *element : printable) {
        if (!element || !element->isDataSourceEnabled()) {
            continue;
        }

        auto source = element->dataSource();
        if (!source || !source->isValid()) {
            if (errorMessage) {
                *errorMessage = tr("数据源无效或未配置");
            }
            return false;
        }

        const int count = source->count();
        if (count <= 0) {
            if (errorMessage) {
                *errorMessage = tr("数据源不包含可用记录");
            }
            return false;
        }

        if (recordCount < 0) {
            recordCount = count;
        } else if (recordCount != count) {
            if (errorMessage) {
                *errorMessage = tr("数据源记录数不一致，无法批量打印");
            }
            return false;
        }

        DataBinding binding;
        binding.element = element;
        binding.source = source;
        binding.text = dynamic_cast<TextElement*>(element);
        binding.barcode = dynamic_cast<BarcodeElement*>(element);
        binding.qrcode = dynamic_cast<QRCodeElement*>(element);

        if (!binding.text && !binding.barcode && !binding.qrcode) {
            if (errorMessage) {
                *errorMessage = tr("元素类型 %1 不支持数据源应用")
                                    .arg(element->getType());
            }
            return false;
        }

        bindings.append(binding);
    }

    if (bindings.isEmpty()) {
        // 没有绑定数据源，直接打印一次
        return m_engine->printOnce(printable, context, errorMessage);
    }

    if (recordCount <= 0) {
        if (errorMessage) {
            *errorMessage = tr("数据源不包含可用记录");
        }
        return false;
    }

    int startIndex = firstRecord;
    int endIndex = lastRecord;

    if (startIndex < 0) {
        startIndex = 0;
    }

    if (endIndex < 0 || endIndex >= recordCount) {
        endIndex = recordCount - 1;
    }

    if (startIndex >= recordCount || endIndex < 0 || startIndex > endIndex) {
        if (errorMessage) {
            *errorMessage = tr("选择的记录范围无效");
        }
        return false;
    }

    PrintRenderer *renderer = m_engine->renderer();
    if (!renderer) {
        if (errorMessage) {
            *errorMessage = tr("打印渲染器未配置");
        }
        return false;
    }

    QPainter painter(context.printer);
    if (!painter.isActive()) {
        if (errorMessage) {
            *errorMessage = tr("无法在打印机上开始绘制");
        }
        return false;
    }

    bool success = true;
    QString lastError;

    for (int index = startIndex; index <= endIndex && success; ++index) {
        for (const DataBinding &binding : bindings) {
            if (!binding.applyRecord(index)) {
                success = false;
                lastError = tr("应用第 %1 条数据到元素时失败")
                                .arg(index + 1);
                break;
            }
        }

        if (!success) {
            break;
        }

        QString engineError;
        if (!renderer->render(painter, printable, context, &engineError)) {
            success = false;
            lastError = engineError;
            break;
        }

        emit recordPrinted(index);

        if (index < endIndex) {
            if (!context.printer->newPage()) {
                success = false;
                lastError = tr("无法创建新的打印页");
                break;
            }
        }
    }

    for (const DataBinding &binding : bindings) {
        binding.restore();
    }

    if (!success && errorMessage) {
        *errorMessage = lastError.isEmpty()
                             ? tr("批量打印过程中发生未知错误")
                             : lastError;
    }

    return success;
}
