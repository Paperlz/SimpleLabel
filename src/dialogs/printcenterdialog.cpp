#include "printcenterdialog.h"
#include "ui_printcenterdialog.h"

#include "../graphics/labelscene.h"
#include "../printing/batchprintmanager.h"
#include "../printing/printengine.h"
#include "../core/labelelement.h"
#include "../core/datasource.h"
#include "../core/textelement.h"
#include "../core/barcodeelement.h"
#include "../core/qrcodeelement.h"

#include <QPrinter>
#include <QPrintDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPushButton>
#include <QComboBox>
#include <QVariant>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QImage>
#include <QPainter>
#include <QtCore/qscopeguard.h>
#include <QLatin1Char>
#include <QVector>
#include <QtGlobal>
#include <QtMath>
#include "layoutexportdialog.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

enum class ExportMode {
    Combined = 0,
    Separate = 1,
    AutoLayout = 2
};

constexpr int kExportModeRole = Qt::UserRole;

struct PreviewBinding
{
    TextElement *text = nullptr;
    BarcodeElement *barcode = nullptr;
    QRCodeElement *qrcode = nullptr;

    bool apply(int recordIndex) const
    {
        if (text) {
            return text->applyDataSourceRecord(recordIndex);
        }
        if (barcode) {
            return barcode->applyDataSourceRecord(recordIndex);
        }
        if (qrcode) {
            return qrcode->applyDataSourceRecord(recordIndex);
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

void restorePreviewBindings(const QVector<PreviewBinding> &bindings)
{
    for (const PreviewBinding &binding : bindings) {
        binding.restore();
    }
}

ExportMode exportModeForIndex(const QComboBox *combo, int index)
{
    if (!combo || index < 0 || index >= combo->count()) {
        return ExportMode::Combined;
    }
    const QVariant data = combo->itemData(index, kExportModeRole);
    if (data.isValid()) {
        return static_cast<ExportMode>(data.toInt());
    }
    return static_cast<ExportMode>(index);
}

ExportMode currentExportMode(const QComboBox *combo)
{
    if (!combo) {
        return ExportMode::Combined;
    }
    return exportModeForIndex(combo, combo->currentIndex());
}

bool applyPreviewRecord(const QList<labelelement*> &elements,
                        int recordIndex,
                        QVector<PreviewBinding> *bindings,
                        QString *errorMessage)
{
    if (!bindings) {
        return false;
    }

    bindings->clear();

    if (recordIndex < 0) {
        if (errorMessage) {
            *errorMessage = PrintCenterDialog::tr("记录索引无效");
        }
        return false;
    }

    QVector<PreviewBinding> applied;
    applied.reserve(elements.size());

    for (labelelement *element : elements) {
        if (!element || !element->isDataSourceEnabled()) {
            continue;
        }

        const std::shared_ptr<DataSource> source = element->dataSource();
        if (!source || !source->isValid()) {
            if (errorMessage) {
                *errorMessage = PrintCenterDialog::tr("数据源无效或未配置");
            }
            restorePreviewBindings(applied);
            return false;
        }

        if (recordIndex >= source->count()) {
            if (errorMessage) {
                *errorMessage = PrintCenterDialog::tr("记录索引超出数据源范围");
            }
            restorePreviewBindings(applied);
            return false;
        }

        PreviewBinding binding;
        binding.text = dynamic_cast<TextElement*>(element);
        binding.barcode = dynamic_cast<BarcodeElement*>(element);
        binding.qrcode = dynamic_cast<QRCodeElement*>(element);

        if (!binding.text && !binding.barcode && !binding.qrcode) {
            if (errorMessage) {
                *errorMessage = PrintCenterDialog::tr("元素类型 %1 暂不支持数据源预览")
                                         .arg(element->getType());
            }
            restorePreviewBindings(applied);
            return false;
        }

        if (!binding.apply(recordIndex)) {
            if (errorMessage) {
                *errorMessage = PrintCenterDialog::tr("应用第 %1 条数据失败")
                                         .arg(recordIndex + 1);
            }
            restorePreviewBindings(applied);
            return false;
        }

        applied.append(binding);
    }

    *bindings = std::move(applied);
    return true;
}

} // namespace

PrintCenterDialog::PrintCenterDialog(LabelScene* scene, PrintEngine* engine, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PrintCenterDialog)
    , m_scene(scene)
    , m_printEngine(engine)
    , m_batchManager(nullptr)
    , m_baseContext{}
    , m_renderScene(nullptr)
    , m_previewScene(new QGraphicsScene(this))
    , m_previewPixmapItem(nullptr)
    , m_zoomFactor(1.0)
    , m_previewDevicePixelRatio(1.0)
    , m_previewUpdateScheduled(false)
    , m_suppressSceneInvalidation(false)
    , m_currentBatchIndex(0)
    , m_totalBatchCount(0)
    , m_batchWindowStart(0)
    , m_previewBaseSize(1024, 768)
{
    ui->setupUi(this);

    if (ui->comboExportMode && ui->comboExportMode->count() >= 3) {
        ui->comboExportMode->setItemData(0, static_cast<int>(ExportMode::Combined), kExportModeRole);
        ui->comboExportMode->setItemData(1, static_cast<int>(ExportMode::Separate), kExportModeRole);
        ui->comboExportMode->setItemData(2, static_cast<int>(ExportMode::AutoLayout), kExportModeRole);
    }

    if (m_scene) {
        connect(m_scene, &QGraphicsScene::changed, this, [this](const QList<QRectF>&) {
            if (m_suppressSceneInvalidation) {
                return;
            }
            invalidatePreviewCache();
            rebuildRenderModel();
            if (!m_previewUpdateScheduled) {
                m_previewUpdateScheduled = true;
                QTimer::singleShot(0, this, [this]() {
                    m_previewUpdateScheduled = false;
                    updatePreview();
                });
            }
        });
    }

    if (ui->previewView) {
        ui->previewView->setScene(m_previewScene);
        ui->previewView->setAlignment(Qt::AlignCenter);
        ui->previewView->setRenderHint(QPainter::Antialiasing, true);
        ui->previewView->setRenderHint(QPainter::TextAntialiasing, true);
        ui->previewView->setRenderHint(QPainter::SmoothPixmapTransform, true);
        ui->previewView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        ui->previewView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    setupConnections();
    updateSizeLabel();
    updateCopySummary();
    updatePreviewControlsEnabled(true);
}

PrintCenterDialog::~PrintCenterDialog()
{
    if (m_batchManager) {
        m_batchManager->setElements({});
    }
    delete ui;
}

void PrintCenterDialog::setElements(const QList<labelelement*>& elements)
{
    invalidatePreviewCache();
    m_elements = elements;
    m_currentBatchIndex = 0;
    rebuildRenderModel();
    if (m_batchManager) {
        const QList<labelelement*> target = m_renderElements.isEmpty() ? m_elements : m_renderElements;
        m_batchManager->setElements(target);
    }
    refreshBatchControls();
    updateCopySummary();
    updatePreview();
}

void PrintCenterDialog::setBatchPrintManager(BatchPrintManager* manager)
{
    invalidatePreviewCache();
    m_batchManager = manager;
    m_currentBatchIndex = 0;
    m_batchWindowStart = 0;
    if (m_batchManager) {
        const QList<labelelement*> target = m_renderElements.isEmpty() ? m_elements : m_renderElements;
        m_batchManager->setElements(target);
    }
    updatePreviewControlsEnabled(true);
    refreshBatchControls();
    updateCopySummary();
    updatePreview();
}

void PrintCenterDialog::setBaseContext(const PrintContext& context)
{
    invalidatePreviewCache();
    m_baseContext = context;
    m_baseContext.printer = nullptr; // 保留布局信息即可
    if (!m_baseContext.sourceScene) {
        m_baseContext.sourceScene = m_scene;
    }
    m_zoomFactor = 1.0;
    updateSizeLabel();
    updatePreview();
}

void PrintCenterDialog::setupConnections()
{
    // 预览工具栏
    connect(ui->btnFitView, &QToolButton::clicked, this, &PrintCenterDialog::onFitView);
    connect(ui->btnZoomIn, &QToolButton::clicked, this, &PrintCenterDialog::onZoomIn);
    connect(ui->btnZoomOut, &QToolButton::clicked, this, &PrintCenterDialog::onZoomOut);
    
    // 批量导航
    connect(ui->btnPrevBatch, &QToolButton::clicked, this, &PrintCenterDialog::onPrevBatch);
    connect(ui->btnNextBatch, &QToolButton::clicked, this, &PrintCenterDialog::onNextBatch);
    const QList<QPushButton*> batchButtons = { ui->btnBatch1, ui->btnBatch2, ui->btnBatch3 };
    for (QPushButton *button : batchButtons) {
        connect(button, &QPushButton::clicked, this, [this, button]() {
            bool ok = false;
            const int recordIndex = button->property("recordIndex").toInt(&ok);
            if (ok) {
                onBatchSelected(recordIndex);
            }
        });
    }
    
    // 页面导航
    connect(ui->pageInput, &QLineEdit::returnPressed, this, &PrintCenterDialog::onPageInputChanged);
    
    // 下拉框
    connect(ui->comboFileType, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PrintCenterDialog::onFileTypeChanged);
    connect(ui->comboExportMode, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PrintCenterDialog::onExportModeChanged);
    
    // 操作按钮
    connect(ui->btnLayoutExport, &QPushButton::clicked, this, &PrintCenterDialog::onLayoutExport);
    connect(ui->btnPrintDirect, &QPushButton::clicked, this, &PrintCenterDialog::onPrintDirect);
    connect(ui->btnDownload, &QPushButton::clicked, this, &PrintCenterDialog::onDownload);

    connect(ui->spinCopies, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PrintCenterDialog::onCopiesChanged);

    connect(ui->spinRangeStart, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (!ui->rangeWidget->isVisible()) {
            return;
        }
        if (ui->spinRangeEnd->value() < value) {
            QSignalBlocker blocker(ui->spinRangeEnd);
            ui->spinRangeEnd->setValue(value);
        }
    });

    connect(ui->spinRangeEnd, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (!ui->rangeWidget->isVisible()) {
            return;
        }
        if (value < ui->spinRangeStart->value()) {
            QSignalBlocker blocker(ui->spinRangeStart);
            ui->spinRangeStart->setValue(value);
        }
    });
}

void PrintCenterDialog::updatePreview()
{
    if (!ui->previewView || !m_previewScene) {
        return;
    }

    m_previewScene->clear();
    m_previewPixmapItem = nullptr;
    m_previewDevicePixelRatio = 1.0;

    QGraphicsScene *renderScene = m_renderScene ? m_renderScene.get() : m_scene;
    const QList<labelelement*> &renderElements = m_renderElements.isEmpty() ? m_elements : m_renderElements;

    if (!renderScene || !m_printEngine || renderElements.isEmpty()) {
        m_currentPreview = QPixmap();
        m_previewScene->addText(tr("暂无可预览内容"));
        m_previewScene->setSceneRect(m_previewScene->itemsBoundingRect());
        applyZoom();
        updatePreviewStatus(false);
        return;
    }

    QPrinter dummyPrinter(QPrinter::HighResolution);
    if (m_baseContext.pageLayout.isValid()) {
        dummyPrinter.setPageLayout(m_baseContext.pageLayout);
    } else {
        dummyPrinter.setPageOrientation(QPageLayout::Portrait);
        dummyPrinter.setPageSize(QPageSize::A4);
    }
    dummyPrinter.setResolution(300);

    PrintContext context = m_baseContext;
    context.printer = &dummyPrinter;
    context.pageLayout = dummyPrinter.pageLayout();
    context.contentMargins = m_baseContext.contentMargins.isNull()
                                 ? context.pageLayout.margins(QPageLayout::Millimeter)
                                 : m_baseContext.contentMargins;
    context.sourceScene = renderScene;

    const QSize baseSize = previewImageSize();
    const qreal designWidthPx = m_baseContext.labelSizePixels.width() > 0.0
                                    ? m_baseContext.labelSizePixels.width()
                                    : baseSize.width();
    const qreal designHeightPx = m_baseContext.labelSizePixels.height() > 0.0
                                     ? m_baseContext.labelSizePixels.height()
                                     : baseSize.height();

    constexpr qreal kMinQualityScale = 1.75;
    constexpr qreal kMaxQualityScale = 4.0;
    qreal renderScale = std::max(m_zoomFactor, kMinQualityScale);
    renderScale = std::min(renderScale, kMaxQualityScale);

    const int renderWidth = qMax(1, static_cast<int>(std::ceil(designWidthPx * renderScale)));
    const int renderHeight = qMax(1, static_cast<int>(std::ceil(designHeightPx * renderScale)));
    const QSize renderSize(renderWidth, renderHeight);

    const int recordIndex = (m_batchManager && m_totalBatchCount > 0)
                                ? qBound(0, m_currentBatchIndex, m_totalBatchCount - 1)
                                : -1;

    const QString cacheKey = previewCacheKey(renderSize, recordIndex);
    if (const auto cachedIt = m_previewCache.constFind(cacheKey); cachedIt != m_previewCache.constEnd()) {
        const CachedPreview &cached = cachedIt.value();
        m_previewCacheOrder.removeAll(cacheKey);
        m_previewCacheOrder.append(cacheKey);
        m_currentPreview = cached.pixmap;
        m_previewPixmapItem = m_previewScene->addPixmap(m_currentPreview);
        const QSize logical = !cached.logicalSize.isEmpty() ? cached.logicalSize : baseSize;
        m_previewScene->setSceneRect(QRectF(QPointF(0.0, 0.0), QSizeF(logical)));
        m_previewDevicePixelRatio = cached.devicePixelRatio > 0.0
                                        ? cached.devicePixelRatio
                                        : m_currentPreview.devicePixelRatio();
        applyZoom();
        updatePreviewStatus(false);
        return;
    }

    updatePreviewStatus(true);

    QVector<PreviewBinding> previewBindings;
    auto restoreGuard = qScopeGuard([&previewBindings]() {
        restorePreviewBindings(previewBindings);
    });

    if (recordIndex >= 0) {
        QString dataError;
        if (!applyPreviewRecord(renderElements, recordIndex, &previewBindings, &dataError)) {
            m_currentPreview = QPixmap();
            m_previewScene->addText(dataError.isEmpty() ? tr("无法生成预览") : dataError);
            m_previewScene->setSceneRect(m_previewScene->itemsBoundingRect());
            applyZoom();
            updatePreviewStatus(false);
            return;
        }
    }

    const qreal widthRatio = (baseSize.width() > 0)
                                 ? static_cast<qreal>(renderWidth) / baseSize.width()
                                 : renderScale;
    const qreal heightRatio = (baseSize.height() > 0)
                                  ? static_cast<qreal>(renderHeight) / baseSize.height()
                                  : renderScale;
    const qreal devicePixelRatio = std::max(widthRatio, heightRatio);

    auto clampDpi = [](qreal value) {
        return std::clamp(value, 90.0, 600.0);
    };

    qreal dpiX = 0.0;
    qreal dpiY = 0.0;
    if (m_baseContext.labelSizeMM.width() > 0.0) {
        dpiX = clampDpi(static_cast<qreal>(renderWidth) * 25.4 / m_baseContext.labelSizeMM.width());
    }
    if (m_baseContext.labelSizeMM.height() > 0.0) {
        dpiY = clampDpi(static_cast<qreal>(renderHeight) * 25.4 / m_baseContext.labelSizeMM.height());
    }

    if (dpiX <= 0.0 && dpiY <= 0.0) {
        const qreal fallbackDpi = clampDpi(150.0 * devicePixelRatio);
        dpiX = dpiY = fallbackDpi;
    } else if (dpiX <= 0.0) {
        dpiX = dpiY;
    } else if (dpiY <= 0.0) {
        dpiY = dpiX;
    }

    QString errorMsg;
    QImage previewImage = m_printEngine->renderPreview(renderElements, context, renderSize, &errorMsg, dpiX, dpiY);
    if (previewImage.isNull()) {
        m_currentPreview = QPixmap();
        m_previewScene->addText(errorMsg.isEmpty() ? tr("无法生成预览") : errorMsg);
        m_previewScene->setSceneRect(m_previewScene->itemsBoundingRect());
        applyZoom();
        updatePreviewStatus(false);
        updatePreviewControlsEnabled(true);
        return;
    }

    previewImage.setDevicePixelRatio(devicePixelRatio);
    m_currentPreview = QPixmap::fromImage(previewImage);
    m_previewPixmapItem = m_previewScene->addPixmap(m_currentPreview);
    m_previewScene->setSceneRect(QRectF(QPointF(0.0, 0.0), QSizeF(baseSize)));
    m_previewDevicePixelRatio = devicePixelRatio;

    CachedPreview cached;
    cached.pixmap = m_currentPreview;
    cached.logicalSize = baseSize;
    cached.devicePixelRatio = devicePixelRatio;
    m_previewCache.insert(cacheKey, cached);
    m_previewCacheOrder.removeAll(cacheKey);
    m_previewCacheOrder.append(cacheKey);
    prunePreviewCache();

    applyZoom();
    updatePreviewStatus(false);
}

void PrintCenterDialog::updateSizeLabel()
{
    const double widthMm = m_baseContext.labelSizeMM.width();
    const double heightMm = m_baseContext.labelSizeMM.height();

    if (widthMm <= 0.0 || heightMm <= 0.0) {
        ui->labelSize->setText(tr("尺寸: --"));
        return;
    }

    ui->labelSize->setText(tr("尺寸: %1mm × %2mm")
                               .arg(widthMm, 0, 'f', 1)
                               .arg(heightMm, 0, 'f', 1));
}

void PrintCenterDialog::showBatchNavigator(bool show)
{
    ui->batchNavigator->setVisible(show);
}

void PrintCenterDialog::showPageNavigator(bool show)
{
    ui->pageNavigator->setVisible(show);
}

void PrintCenterDialog::updateBatchButtons()
{
    clampBatchIndex();

    const QList<QPushButton*> buttons = { ui->btnBatch1, ui->btnBatch2, ui->btnBatch3 };
    const int maxButtons = static_cast<int>(buttons.size());

    if (m_totalBatchCount <= 0) {
        for (QPushButton *button : buttons) {
            if (!button) {
                continue;
            }
            button->setVisible(false);
            button->setProperty("recordIndex", -1);
        }
        if (ui->labelEllipsis) {
            ui->labelEllipsis->setVisible(false);
        }
        if (ui->btnPrevBatch) {
            ui->btnPrevBatch->setEnabled(false);
        }
        if (ui->btnNextBatch) {
            ui->btnNextBatch->setEnabled(false);
        }
        return;
    }

    const int displayCount = std::min(maxButtons, m_totalBatchCount - m_batchWindowStart);

    for (int i = 0; i < maxButtons; ++i) {
        QPushButton *button = buttons[i];
        if (!button) {
            continue;
        }

        if (i < displayCount) {
            const int recordIndex = m_batchWindowStart + i;
            button->setVisible(true);
            button->setEnabled(true);
            button->setText(QString::number(recordIndex + 1));
            button->setChecked(recordIndex == m_currentBatchIndex);
            button->setProperty("recordIndex", recordIndex);
        } else {
            button->setVisible(false);
            button->setProperty("recordIndex", -1);
        }
    }

    if (ui->labelEllipsis) {
        ui->labelEllipsis->setVisible(m_batchWindowStart + displayCount < m_totalBatchCount);
    }

    if (ui->btnPrevBatch) {
        ui->btnPrevBatch->setEnabled(m_currentBatchIndex > 0);
    }
    if (ui->btnNextBatch) {
        ui->btnNextBatch->setEnabled(m_currentBatchIndex < m_totalBatchCount - 1);
    }
}

void PrintCenterDialog::ensureBatchWindowVisible()
{
    if (m_totalBatchCount <= 0) {
        m_batchWindowStart = 0;
        return;
    }

    const int maxStart = std::max(0, m_totalBatchCount - 3);

    if (m_currentBatchIndex < m_batchWindowStart) {
        m_batchWindowStart = m_currentBatchIndex;
    } else if (m_currentBatchIndex >= m_batchWindowStart + 3) {
        m_batchWindowStart = m_currentBatchIndex - 2;
    }

    m_batchWindowStart = qBound(0, m_batchWindowStart, maxStart);
}

void PrintCenterDialog::invalidatePreviewCache()
{
    m_previewCache.clear();
    m_previewDevicePixelRatio = 1.0;
    m_previewCacheOrder.clear();
}

QString PrintCenterDialog::previewCacheKey(const QSize& renderSize, int recordIndex) const
{
    return QStringLiteral("%1_%2x%3")
    .arg(recordIndex)
        .arg(renderSize.width())
        .arg(renderSize.height());
}

void PrintCenterDialog::prunePreviewCache()
{
    constexpr int kMaxEntries = 12;
    while (m_previewCacheOrder.size() > kMaxEntries) {
        const QString oldestKey = m_previewCacheOrder.takeFirst();
        m_previewCache.remove(oldestKey);
    }
}

void PrintCenterDialog::rebuildRenderModel()
{
    m_renderElements.clear();
    m_renderElementStorage.clear();

    if (m_elements.isEmpty()) {
        m_renderScene.reset();
        return;
    }

    auto newScene = std::make_unique<QGraphicsScene>();
    newScene->setItemIndexMethod(QGraphicsScene::NoIndex);
    if (m_scene) {
        newScene->setSceneRect(m_scene->sceneRect());
        newScene->setBackgroundBrush(m_scene->backgroundBrush());
    }

    for (labelelement *source : m_elements) {
        if (!source) {
            continue;
        }

        auto clone = labelelement::createFromType(source->getType());
        if (!clone) {
            continue;
        }

        clone->setData(source->getData());

        if (auto ds = source->dataSource()) {
            clone->setDataSource(ds);
            clone->setDataSourceEnabled(source->isDataSourceEnabled());
        } else {
            clone->clearDataSource();
            clone->setDataSourceEnabled(false);
        }

        clone->addToScene(newScene.get());
        clone->setPos(source->getPos());

        if (QGraphicsItem *sourceItem = source->getItem()) {
            if (QGraphicsItem *cloneItem = clone->getItem()) {
                cloneItem->setTransform(sourceItem->transform());
                cloneItem->setTransformOriginPoint(sourceItem->transformOriginPoint());
                cloneItem->setZValue(sourceItem->zValue());
            }
        }

        m_renderElements.append(clone.get());
        m_renderElementStorage.emplace_back(std::move(clone));
    }

    m_renderScene = std::move(newScene);

    if (m_batchManager) {
        const QList<labelelement*> target = m_renderElements.isEmpty() ? m_elements : m_renderElements;
        m_batchManager->setElements(target);
    }
}

void PrintCenterDialog::refreshBatchControls()
{
    if (!ui->batchNavigator || !ui->rangeWidget) {
        return;
    }

    QString errorMessage;
    const QList<labelelement*> &countElements = m_renderElements.isEmpty() ? m_elements : m_renderElements;

    if (!m_batchManager) {
        m_totalBatchCount = 0;
    } else {
        m_totalBatchCount = resolveRecordCount(countElements, &errorMessage);
    }

    if (!errorMessage.isEmpty()) {
        QMessageBox::warning(this, tr("数据源错误"), errorMessage);
        m_totalBatchCount = 0;
    }

    clampBatchIndex();

    const bool hasBatch = m_batchManager && m_totalBatchCount > 0;
    showBatchNavigator(hasBatch);
    showPageNavigator(hasBatch);
    ui->rangeWidget->setVisible(hasBatch);

    if (ui->pageInput) {
        ui->pageInput->setEnabled(hasBatch);
        if (hasBatch) {
            const int currentRecord = qBound(0, m_currentBatchIndex, m_totalBatchCount - 1) + 1;
            ui->pageInput->setText(QString::number(currentRecord));
        } else {
            ui->pageInput->clear();
        }
    }

    if (hasBatch) {
        ui->spinRangeStart->setMinimum(1);
        ui->spinRangeEnd->setMinimum(1);
        ui->spinRangeStart->setMaximum(m_totalBatchCount);
        ui->spinRangeEnd->setMaximum(m_totalBatchCount);

        {
            QSignalBlocker blockerStart(ui->spinRangeStart);
            QSignalBlocker blockerEnd(ui->spinRangeEnd);
            if (ui->spinRangeStart->value() < 1 || ui->spinRangeStart->value() > m_totalBatchCount) {
                ui->spinRangeStart->setValue(1);
            }
            if (ui->spinRangeEnd->value() < ui->spinRangeStart->value() || ui->spinRangeEnd->value() > m_totalBatchCount) {
                ui->spinRangeEnd->setValue(m_totalBatchCount);
            }
        }
    } else {
        QSignalBlocker blockerStart(ui->spinRangeStart);
        QSignalBlocker blockerEnd(ui->spinRangeEnd);
        ui->spinRangeStart->setMinimum(1);
        ui->spinRangeEnd->setMinimum(1);
        ui->spinRangeStart->setMaximum(1);
        ui->spinRangeEnd->setMaximum(1);
        ui->spinRangeStart->setValue(1);
        ui->spinRangeEnd->setValue(1);
    }

    updateBatchButtons();
    updateCopySummary();
}

void PrintCenterDialog::clampBatchIndex()
{
    if (m_totalBatchCount <= 0) {
        m_currentBatchIndex = 0;
        m_batchWindowStart = 0;
        return;
    }

    m_currentBatchIndex = qBound(0, m_currentBatchIndex, m_totalBatchCount - 1);
    ensureBatchWindowVisible();
}

int PrintCenterDialog::resolveRecordCount(const QList<labelelement*>& elements, QString *errorMessage) const
{
    int recordCount = -1;
    bool hasBoundElement = false;

    for (labelelement *element : elements) {
        if (!element || !element->isDataSourceEnabled()) {
            continue;
        }

        hasBoundElement = true;

        const std::shared_ptr<DataSource> source = element->dataSource();
        if (!source || !source->isValid()) {
            if (errorMessage) {
                *errorMessage = tr("数据源无效或未正确配置");
            }
            return 0;
        }

        const int count = source->count();
        if (count <= 0) {
            if (errorMessage) {
                *errorMessage = tr("数据源不包含可用记录");
            }
            return 0;
        }

        if (recordCount < 0) {
            recordCount = count;
        } else if (recordCount != count) {
            if (errorMessage) {
                *errorMessage = tr("多个数据源的记录数不一致");
            }
            return 0;
        }
    }

    if (!hasBoundElement) {
        return 0;
    }

    return recordCount < 0 ? 0 : recordCount;
}

QSize PrintCenterDialog::previewImageSize() const
{
    const double widthPx = m_baseContext.labelSizePixels.width();
    const double heightPx = m_baseContext.labelSizePixels.height();

    if (widthPx <= 0.0 || heightPx <= 0.0) {
        return m_previewBaseSize;
    }

    const double aspect = widthPx / heightPx;

    int width = m_previewBaseSize.width();
    int height = m_previewBaseSize.height();

    if (aspect >= 1.0) {
        height = qMax(1, qRound(width / aspect));
        if (height > m_previewBaseSize.height()) {
            height = m_previewBaseSize.height();
            width = qMax(1, qRound(height * aspect));
        }
    } else {
        width = qMax(1, qRound(height * aspect));
        if (width > m_previewBaseSize.width()) {
            width = m_previewBaseSize.width();
            height = qMax(1, qRound(width / aspect));
        }
    }

    return QSize(qMax(1, width), qMax(1, height));
}

void PrintCenterDialog::updateCopySummary()
{
    if (!ui->labelCopyCount) {
        return;
    }

    if (m_batchManager && m_totalBatchCount > 0) {
        const int currentRecord = qBound(0, m_currentBatchIndex, m_totalBatchCount - 1) + 1;
        ui->labelCopyCount->setText(tr("记录: %1 / %2").arg(currentRecord).arg(m_totalBatchCount));
    } else if (ui->spinCopies) {
        ui->labelCopyCount->setText(tr("份数: %1").arg(ui->spinCopies->value()));
    } else {
        ui->labelCopyCount->clear();
    }
}

void PrintCenterDialog::updatePreviewStatus(bool busy)
{
    if (!ui->labelPreviewStatus) {
        return;
    }

    if (busy) {
        ui->labelPreviewStatus->setText(tr("正在生成预览…"));
        updatePreviewControlsEnabled(false);
    } else {
        ui->labelPreviewStatus->clear();
        updatePreviewControlsEnabled(true);
    }
}

void PrintCenterDialog::updatePreviewControlsEnabled(bool enabled)
{
    const QList<QAbstractButton*> buttons = {
        ui->btnFitView,
        ui->btnZoomIn,
        ui->btnZoomOut
    };
    for (QAbstractButton *button : buttons) {
        if (button) {
            button->setEnabled(enabled);
        }
    }
}

void PrintCenterDialog::applyZoom()
{
    if (!ui->previewView) {
        return;
    }

    const qreal clampedFactor = qBound(0.05, m_zoomFactor, 8.0);
    if (!qFuzzyCompare(clampedFactor, m_zoomFactor)) {
        m_zoomFactor = clampedFactor;
    }

    ui->previewView->resetTransform();

    if (m_previewScene && !m_previewScene->sceneRect().isEmpty()) {
        ui->previewView->scale(m_zoomFactor, m_zoomFactor);
        ui->previewView->centerOn(m_previewScene->sceneRect().center());
    }
}

void PrintCenterDialog::onFitView()
{
    if (!ui->previewView || !m_previewScene || m_previewScene->sceneRect().isEmpty()) {
        return;
    }

    const QRectF sceneRect = m_previewScene->sceneRect();
    const QSize viewportSize = ui->previewView->viewport()->size();
    if (sceneRect.width() <= 0.0 || sceneRect.height() <= 0.0 ||
        viewportSize.width() <= 0 || viewportSize.height() <= 0) {
        return;
    }

    const qreal factorX = viewportSize.width() / sceneRect.width();
    const qreal factorY = viewportSize.height() / sceneRect.height();
    const qreal newFactor = qBound(0.05, std::min(factorX, factorY), 8.0);
    if (!qFuzzyCompare(newFactor, m_zoomFactor)) {
        m_zoomFactor = newFactor;
        updatePreview();
    } else {
        applyZoom();
    }
}

void PrintCenterDialog::onZoomIn()
{
    const qreal newFactor = qBound(0.05, m_zoomFactor * 1.2, 8.0);
    if (!qFuzzyCompare(newFactor, m_zoomFactor)) {
        m_zoomFactor = newFactor;
        updatePreview();
    } else {
        applyZoom();
    }
}

void PrintCenterDialog::onZoomOut()
{
    const qreal newFactor = qBound(0.05, m_zoomFactor / 1.2, 8.0);
    if (!qFuzzyCompare(newFactor, m_zoomFactor)) {
        m_zoomFactor = newFactor;
        updatePreview();
    } else {
        applyZoom();
    }
}

void PrintCenterDialog::onPrevBatch()
{
    if (m_totalBatchCount <= 0) {
        return;
    }

    if (m_currentBatchIndex > 0) {
        --m_currentBatchIndex;
        updateBatchButtons();
        updateCopySummary();
        updatePreview();
    }
}

void PrintCenterDialog::onNextBatch()
{
    if (m_totalBatchCount <= 0) {
        return;
    }

    if (m_currentBatchIndex < m_totalBatchCount - 1) {
        ++m_currentBatchIndex;
        updateBatchButtons();
        updateCopySummary();
        updatePreview();
    }
}

void PrintCenterDialog::onBatchSelected(int index)
{
    if (m_totalBatchCount <= 0) {
        return;
    }

    if (index < 0 || index >= m_totalBatchCount) {
        return;
    }

    m_currentBatchIndex = index;
    updateBatchButtons();
    updateCopySummary();
    updatePreview();
}

void PrintCenterDialog::onPageInputChanged()
{
    if (!ui->pageInput) {
        return;
    }

    if (!m_batchManager || m_totalBatchCount <= 0) {
        // 非批量模式下直接回退为当前索引显示
        const int current = m_currentBatchIndex + 1;
        if (current > 0) {
            ui->pageInput->setText(QString::number(current));
        }
        return;
    }

    const int currentIndex = m_currentBatchIndex;
    bool ok = false;
    const int requestedPage = ui->pageInput->text().toInt(&ok);

    if (!ok) {
        ui->pageInput->setText(QString::number(currentIndex + 1));
        return;
    }

    int clampedPage = qBound(1, requestedPage, m_totalBatchCount);
    if (clampedPage != requestedPage) {
        ui->pageInput->setText(QString::number(clampedPage));
    }

    const int targetIndex = clampedPage - 1;
    if (targetIndex == currentIndex) {
        return;
    }

    m_currentBatchIndex = targetIndex;
    ensureBatchWindowVisible();
    updateBatchButtons();
    updateCopySummary();
    updatePreview();

    ui->pageInput->setText(QString::number(m_currentBatchIndex + 1));
}

void PrintCenterDialog::onFileTypeChanged(int index)
{
    Q_UNUSED(index);
    ui->comboPageSize->setEnabled(true);
}

void PrintCenterDialog::onExportModeChanged(int index)
{
    const ExportMode mode = exportModeForIndex(ui->comboExportMode, index);

    // 自动排版模式启用排版导出按钮
    ui->btnLayoutExport->setEnabled(mode == ExportMode::AutoLayout);
}

void PrintCenterDialog::onLayoutExport()
{
    if (!m_scene || m_elements.isEmpty()) {
        QMessageBox::warning(this, tr("排版导出"), tr("缺少可导出的内容"));
        return;
    }

    // 弹出排版导出参数对话框
    LayoutExportDialog dlg(this);
    // 传入标签物理尺寸用于自动计算行列
    if (m_baseContext.labelSizeMM.width() > 0.0 && m_baseContext.labelSizeMM.height() > 0.0) {
        dlg.setLabelSizeMM(m_baseContext.labelSizeMM);
    }
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    // 根据当前文件类型提供合适的保存筛选
    QString fileType = ui->comboFileType ? ui->comboFileType->currentText() : QStringLiteral("PDF");
    QString filter;
    QString defaultExt;
    if (fileType == QStringLiteral("PNG")) {
        filter = tr("PNG图片 (*.png)");
        defaultExt = QStringLiteral(".png");
    } else if (fileType == QStringLiteral("JPEG")) {
        filter = tr("JPEG图片 (*.jpg *.jpeg)");
        defaultExt = QStringLiteral(".jpg");
    } else {
        filter = tr("PDF 文件 (*.pdf)");
        defaultExt = QStringLiteral(".pdf");
        fileType = QStringLiteral("PDF");
    }

    QString suggested = QStringLiteral("layout%1").arg(defaultExt);
    const QString fileName = QFileDialog::getSaveFileName(this, tr("保存排版文件"), suggested, filter);
    if (fileName.isEmpty()) {
        return;
    }

    // 选择渲染场景与元素（优先使用克隆的渲染场景）
    QGraphicsScene *renderScene = m_renderScene ? m_renderScene.get() : m_scene;
    const QList<labelelement*> &exportElements = m_renderElements.isEmpty() ? m_elements : m_renderElements;
    if (!renderScene || exportElements.isEmpty()) {
        QMessageBox::warning(this, tr("排版导出"), tr("缺少可导出的内容"));
        return;
    }

    // 决定导出份数与范围：若存在批量数据源，按范围导出；否则仅导出一份
    int exportStartIndex = 0; // 包含，基于0
    int exportEndIndex = -1;  // 包含，-1表示无数据源
    if (m_batchManager) {
        QString err;
        const int recordCount = resolveRecordCount(exportElements, &err);
        if (!err.isEmpty()) {
            QMessageBox::warning(this, tr("排版导出"), err);
            return;
        }

        if (recordCount > 0) {
            // 读取界面上的范围（1-based），转换为0-based并钳制
            int start1 = ui->spinRangeStart ? ui->spinRangeStart->value() : 1;
            int end1 = ui->spinRangeEnd ? ui->spinRangeEnd->value() : recordCount;
            if (start1 > end1) std::swap(start1, end1);
            start1 = qBound(1, start1, recordCount);
            end1 = qBound(1, end1, recordCount);
            exportStartIndex = start1 - 1;
            exportEndIndex = end1 - 1;
        }
    }

    // 计算设计区域（用于 scene->render 源矩形）：
    // 优先使用标签像素尺寸以保持与非排版导出一致的物理比例；
    // 若未提供，则退回到场景矩形或元素联合边界。
    QRectF designRect;
    if (m_baseContext.labelSizePixels.width() > 0.0 && m_baseContext.labelSizePixels.height() > 0.0) {
        designRect = QRectF(0, 0,
                            m_baseContext.labelSizePixels.width(),
                            m_baseContext.labelSizePixels.height());
    } else {
        bool first = true;
        for (labelelement *el : exportElements) {
            if (!el) continue;
            if (QGraphicsItem *it = el->getItem()) {
                const QRectF r = it->sceneBoundingRect();
                if (first) { designRect = r; first = false; }
                else { designRect = designRect.united(r); }
            }
        }
        if (designRect.isEmpty()) {
            designRect = renderScene->sceneRect();
        }
        if (designRect.width() <= 0 || designRect.height() <= 0) {
            designRect = QRectF(0,0,100,100);
        }
    }

    // 页面与尺寸参数（物理毫米）
    const QSizeF pageMM = dlg.pageSizeMM();
    const bool landscape = dlg.landscape();

    const bool horizontalOrder = dlg.horizontalOrder();
    const QMarginsF marginsMM = dlg.marginsMM();
    const double hSpacingMM = dlg.hSpacingMM();
    const double vSpacingMM = dlg.vSpacingMM();

    // 使用毫米域计算最大可容纳行列，避免像素取整导致少一列/行
    QSizeF orientedPageMM = dlg.pageSizeMM();
    if (dlg.landscape()) {
        orientedPageMM = QSizeF(orientedPageMM.height(), orientedPageMM.width());
    }
    const double innerWmm = std::max(0.0, orientedPageMM.width() - (marginsMM.left() + marginsMM.right()));
    const double innerHmm = std::max(0.0, orientedPageMM.height() - (marginsMM.top() + marginsMM.bottom()));
    const double labelWmm = std::max(0.0, m_baseContext.labelSizeMM.width());
    const double labelHmm = std::max(0.0, m_baseContext.labelSizeMM.height());
    const int maxCols_mm = (labelWmm > 0.0)
        ? std::max(1, static_cast<int>(std::floor((innerWmm + hSpacingMM) / (labelWmm + hSpacingMM))))
        : 1;
    const int maxRows_mm = (labelHmm > 0.0)
        ? std::max(1, static_cast<int>(std::floor((innerHmm + vSpacingMM) / (labelHmm + vSpacingMM))))
        : 1;

    // 公用：毫米转像素
    auto mmToDevice = [](double mm, double dpi){ return (dpi / 25.4) * mm; };

    // 计算导出 DPI（图像导出使用固定/推算的 DPI；PDF 依赖打印机 DPI）
    qreal dpiX = 300.0;
    qreal dpiY = 300.0;
    if (fileType == QStringLiteral("PDF")) {
        // 使用打印机的逻辑 DPI
        // will be assigned later after printer created
    }
    // 目标行列：以毫米域的最大容量为上限，避免像素取整带来的-1误差
    const int targetCols = qBound(1, dlg.columns(), maxCols_mm);
    const int targetRows = qBound(1, dlg.rows(), maxRows_mm);
    const int cellsPerPage = targetRows * targetCols;

    auto indexToRowCol = [targetRows, targetCols, horizontalOrder](int index)->QPair<int,int>{
        if (horizontalOrder) {
            const int r = index / targetCols;
            const int c = index % targetCols;
            return {r,c};
        } else {
            const int c = index / targetRows;
            const int r = index % targetRows;
            return {r,c};
        }
    };

    // 导出过程中隐藏选择框（若直接使用原场景）
    QGraphicsItem *selFrame = nullptr;
    bool selFrameVisible = false;
    if (renderScene == m_scene) {
        if (auto labelScene = qobject_cast<LabelScene*>(m_scene)) {
            selFrame = labelScene->selectionFrame();
            if (selFrame) {
                selFrameVisible = selFrame->isVisible();
                selFrame->setVisible(false);
            }
        }
    }

    int printed = 0;
    const bool hasBatch = (exportEndIndex >= exportStartIndex);
    int totalCount = hasBatch ? (exportEndIndex - exportStartIndex + 1) : 1;
    if (!hasBatch) { // 无数据源时，按导出份数 N 排布 N 份
        const int copies = ui->spinCopies ? std::max(1, ui->spinCopies->value()) : 1;
        totalCount = copies;
    }

    // 起始偏移（仅第一页生效）
    int startRow0 = qBound(0, dlg.startRow() - 1, targetRows - 1);   // 0-based
    int startCol0 = qBound(0, dlg.startColumn() - 1, targetCols - 1); // 0-based
    const int linearStartIndex = horizontalOrder
        ? (startRow0 * targetCols + startCol0)
        : (startCol0 * targetRows + startRow0);
    const int firstPageCapacityAfterOffset = std::max(0, cellsPerPage - linearStartIndex);

    if (fileType == QStringLiteral("PDF")) {
        // PDF：沿用原有打印机路径
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        const QPageSize pageSize(QSize(int(pageMM.width()), int(pageMM.height())), QPageSize::Millimeter);
        const QPageLayout pageLayout(pageSize, landscape ? QPageLayout::Landscape : QPageLayout::Portrait, QMarginsF());
        printer.setPageLayout(pageLayout);

        // 设备像素参数
        const QRectF pageRectPx = printer.paperRect(QPrinter::DevicePixel);
        dpiX = printer.logicalDpiX();
        dpiY = printer.logicalDpiY();
        const qreal labelPxW = std::max<qreal>(1.0, mmToDevice(m_baseContext.labelSizeMM.width(), dpiX));
        const qreal labelPxH = std::max<qreal>(1.0, mmToDevice(m_baseContext.labelSizeMM.height(), dpiY));
        const qreal leftPx = std::max<qreal>(0.0, mmToDevice(marginsMM.left(), dpiX));
        const qreal topPx = std::max<qreal>(0.0, mmToDevice(marginsMM.top(), dpiY));
        const qreal hGapPx = std::max<qreal>(0.0, mmToDevice(hSpacingMM, dpiX));
        const qreal vGapPx = std::max<qreal>(0.0, mmToDevice(vSpacingMM, dpiY));

        QPainter painter(&printer);
        painter.setRenderHint(QPainter::Antialiasing,true);
        painter.setRenderHint(QPainter::TextAntialiasing,true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform,true);

        int prevPageNumber = 0;
        for (int i = 0; i < totalCount; ++i) {
            int pageNumber = 0;
            int cellIndexInPage = 0;
            if (i < firstPageCapacityAfterOffset) {
                pageNumber = 0;
                cellIndexInPage = linearStartIndex + i;
            } else {
                const int remaining = i - firstPageCapacityAfterOffset;
                pageNumber = 1 + remaining / cellsPerPage;
                cellIndexInPage = remaining % cellsPerPage;
            }
            if (i > 0 && pageNumber != prevPageNumber) {
                printer.newPage();
            }
            prevPageNumber = pageNumber;

            QVector<PreviewBinding> bindings;
            auto restoreGuard = qScopeGuard([&bindings]() { restorePreviewBindings(bindings); });
            if (hasBatch) {
                const int recordIndex = exportStartIndex + i;
                QString dataError;
                if (!applyPreviewRecord(exportElements, recordIndex, &bindings, &dataError)) {
                    QMessageBox::warning(this, tr("排版导出"), dataError.isEmpty() ? tr("无法应用数据源记录") : dataError);
                    break;
                }
            }

            const auto rc = indexToRowCol(cellIndexInPage);
            const int r = rc.first;
            const int c = rc.second;
            painter.save();
            const qreal x = pageRectPx.left() + leftPx + c * (labelPxW + hGapPx);
            const qreal y = pageRectPx.top() + topPx + r * (labelPxH + vGapPx);
            painter.translate(x, y);
            const QRectF targetCell(0, 0, labelPxW, labelPxH);
            renderScene->render(&painter, targetCell, designRect, Qt::IgnoreAspectRatio);
            painter.restore();
            ++printed;
        }

        painter.end();

        if (selFrame) selFrame->setVisible(selFrameVisible);
        QMessageBox::information(this, tr("完成"), tr("已导出 %1 份到 PDF:\n%2")
                                 .arg(printed)
                                 .arg(QDir::toNativeSeparators(fileName)));
        return;
    }

    // 图像导出：PNG/JPEG
    // 计算整页像素尺寸（考虑横竖方向）
    const QSizeF orientedMM = landscape ? QSizeF(pageMM.height(), pageMM.width()) : pageMM;
    const int pageWidthPx = std::max(1, static_cast<int>(std::ceil(mmToDevice(orientedMM.width(), dpiX))));
    const int pageHeightPx = std::max(1, static_cast<int>(std::ceil(mmToDevice(orientedMM.height(), dpiY))));
    const qreal labelPxW = std::max<qreal>(1.0, mmToDevice(m_baseContext.labelSizeMM.width(), dpiX));
    const qreal labelPxH = std::max<qreal>(1.0, mmToDevice(m_baseContext.labelSizeMM.height(), dpiY));
    const qreal leftPx = std::max<qreal>(0.0, mmToDevice(marginsMM.left(), dpiX));
    const qreal topPx = std::max<qreal>(0.0, mmToDevice(marginsMM.top(), dpiY));
    const qreal hGapPx = std::max<qreal>(0.0, mmToDevice(hSpacingMM, dpiX));
    const qreal vGapPx = std::max<qreal>(0.0, mmToDevice(vSpacingMM, dpiY));

    const QFileInfo baseInfo(fileName);
    const QDir targetDir = baseInfo.dir();
    const QString baseName = baseInfo.completeBaseName();
    const QString suffix = baseInfo.completeSuffix().isEmpty() ? (fileType == QLatin1String("JPEG") ? QStringLiteral("jpg") : QStringLiteral("png")) : baseInfo.completeSuffix();

    int prevPageNumber = -1;
    int pageCounter = -1;
    std::unique_ptr<QImage> pageImage;
    std::unique_ptr<QPainter> painter;

    auto startNewPage = [&](int pageNumber){
        pageImage = std::make_unique<QImage>(pageWidthPx, pageHeightPx,
                                             fileType == QStringLiteral("JPEG") ? QImage::Format_RGB32 : QImage::Format_ARGB32_Premultiplied);
        if (fileType == QStringLiteral("JPEG")) {
            pageImage->fill(Qt::white);
        } else {
            pageImage->fill(Qt::white);
        }
        painter = std::make_unique<QPainter>(pageImage.get());
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        prevPageNumber = pageNumber;
        pageCounter = pageNumber;
    };

    auto flushPage = [&](){
        if (!pageImage) return true;
        const QString outPath = targetDir.filePath(QStringLiteral("%1_%2.%3")
                                                   .arg(baseName)
                                                   .arg(QString::number(pageCounter + 1).rightJustified(3, QLatin1Char('0')))
                                                   .arg(suffix));
        bool ok = pageImage->save(outPath);
        if (!ok) {
            QMessageBox::warning(this, tr("导出失败"), tr("无法写入文件 %1").arg(QDir::toNativeSeparators(outPath)));
        }
        return ok;
    };

    for (int i = 0; i < totalCount; ++i) {
        int pageNumber = 0;
        int cellIndexInPage = 0;
        if (i < firstPageCapacityAfterOffset) {
            pageNumber = 0;
            cellIndexInPage = linearStartIndex + i;
        } else {
            const int remaining = i - firstPageCapacityAfterOffset;
            pageNumber = 1 + remaining / cellsPerPage;
            cellIndexInPage = remaining % cellsPerPage;
        }

        if (prevPageNumber != pageNumber) {
            if (pageImage && painter) {
                painter->end();
                if (!flushPage()) {
                    if (selFrame) selFrame->setVisible(selFrameVisible);
                    return;
                }
            }
            startNewPage(pageNumber);
        }

        QVector<PreviewBinding> bindings;
        auto restoreGuard = qScopeGuard([&bindings]() { restorePreviewBindings(bindings); });
        if (hasBatch) {
            const int recordIndex = exportStartIndex + i;
            QString dataError;
            if (!applyPreviewRecord(exportElements, recordIndex, &bindings, &dataError)) {
                QMessageBox::warning(this, tr("排版导出"), dataError.isEmpty() ? tr("无法应用数据源记录") : dataError);
                break;
            }
        }

        const auto rc = indexToRowCol(cellIndexInPage);
        const int r = rc.first;
        const int c = rc.second;
        painter->save();
        const qreal x = leftPx + c * (labelPxW + hGapPx);
        const qreal y = topPx + r * (labelPxH + vGapPx);
        painter->translate(x, y);
        const QRectF targetCell(0, 0, labelPxW, labelPxH);
        renderScene->render(painter.get(), targetCell, designRect, Qt::IgnoreAspectRatio);
        painter->restore();
        ++printed;
    }

    if (painter) painter->end();
    if (!flushPage()) {
        if (selFrame) selFrame->setVisible(selFrameVisible);
        return;
    }

    if (selFrame) selFrame->setVisible(selFrameVisible);

    QMessageBox::information(this, tr("完成"), tr("已导出 %1 份到 %2 图像，保存于:\n%3")
                             .arg(printed)
                             .arg(fileType)
                             .arg(QDir::toNativeSeparators(baseInfo.dir().absolutePath())));
}

void PrintCenterDialog::onPrintDirect()
{
    if (!m_scene || !m_printEngine || m_elements.isEmpty()) return;
    
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::NativeFormat);
    if (m_baseContext.pageLayout.isValid()) {
        printer.setPageLayout(m_baseContext.pageLayout);
    } else {
        printer.setPageOrientation(QPageLayout::Portrait);
        printer.setPageSize(QPageSize::A4);
    }
    // 允许用户调整打印机与页面设置
    {
        QPrintDialog dlg(&printer, this);
        dlg.setWindowTitle(tr("打印"));
        if (dlg.exec() != QDialog::Accepted) {
            return; // 用户取消
        }
        // 同步用户选择回基础上下文，保持后续导出/预览一致
        m_baseContext.pageLayout = printer.pageLayout();
        if (m_baseContext.contentMargins.isNull()) {
            m_baseContext.contentMargins = m_baseContext.pageLayout.margins(QPageLayout::Millimeter);
        }
        updatePreview();
    }
    
    // 根据批量模式决定打印方式
    if (m_batchManager) {
        if (m_totalBatchCount <= 0) {
            QMessageBox::warning(this, tr("打印失败"), tr("当前数据源没有可用记录"));
            return;
        }

        int start = ui->spinRangeStart->value();
        int end = ui->spinRangeEnd->value();
        if (start > end) {
            std::swap(start, end);
        }

        QGraphicsScene *renderScene = m_renderScene ? m_renderScene.get() : m_scene;
        if (!renderScene) {
            QMessageBox::warning(this, tr("打印失败"), tr("缺少可用的打印场景"));
            return;
        }

        const QList<labelelement*> &printElements = m_renderElements.isEmpty() ? m_elements : m_renderElements;
        if (printElements.isEmpty()) {
            QMessageBox::warning(this, tr("打印失败"), tr("没有可打印的元素"));
            return;
        }

        if (m_batchManager) {
            m_batchManager->setElements(printElements);
        }

        PrintContext context = m_baseContext;
        context.printer = &printer;
        context.pageLayout = printer.pageLayout();
        context.contentMargins = m_baseContext.contentMargins.isNull()
                                 ? context.pageLayout.margins(QPageLayout::Millimeter)
                                 : m_baseContext.contentMargins;
        context.sourceScene = renderScene;
        
        QString errorMsg;
        const int maxIndex = m_totalBatchCount - 1;
        const int firstIndex = qBound(0, start - 1, maxIndex);
        const int lastIndex = qBound(firstIndex, end - 1, maxIndex);
        bool ok = m_batchManager->execute(context, firstIndex, lastIndex, &errorMsg);
        if (!ok) {
            QMessageBox::warning(this, "打印失败", errorMsg);
            return;
        }
        updatePreview();
    } else {
        // 单页打印
    int copies = ui->spinCopies->value();
        printer.setCopyCount(copies);
        
        PrintContext context = m_baseContext;
        context.printer = &printer;
        context.pageLayout = printer.pageLayout();
        context.contentMargins = m_baseContext.contentMargins.isNull()
                                 ? context.pageLayout.margins(QPageLayout::Millimeter)
                                 : m_baseContext.contentMargins;
        context.sourceScene = m_renderScene ? m_renderScene.get() : m_scene;
        
        QString errorMsg;
        const QList<labelelement*> &printElements = m_renderElements.isEmpty() ? m_elements : m_renderElements;
        if (printElements.isEmpty()) {
            QMessageBox::warning(this, "打印失败", tr("没有可打印的元素"));
            return;
        }

        if (!m_printEngine->printOnce(printElements, context, &errorMsg)) {
            QMessageBox::warning(this, "打印失败", errorMsg.isEmpty() ? tr("无法启动打印任务") : errorMsg);
            return;
        }
    }
    
    QMessageBox::information(this, "完成", "打印任务已提交");
}

void PrintCenterDialog::onDownload()
{
    if (!m_scene || !m_printEngine || m_elements.isEmpty()) {
        return;
    }

    QString fileType = ui->comboFileType->currentText();
    const ExportMode exportMode = currentExportMode(ui->comboExportMode);
    QString filter;
    QString defaultExt;

    if (fileType == "PDF") {
        filter = QStringLiteral("PDF文件 (*.pdf)");
        defaultExt = QStringLiteral(".pdf");
    } else if (fileType == "PNG") {
        filter = QStringLiteral("PNG图片 (*.png)");
        defaultExt = QStringLiteral(".png");
    } else if (fileType == "JPEG") {
        filter = QStringLiteral("JPEG图片 (*.jpg *.jpeg)");
        defaultExt = QStringLiteral(".jpg");
    } else {
        // 如果遇到未知类型，回退到 PDF 以保证流程可用
        filter = QStringLiteral("PDF文件 (*.pdf)");
        defaultExt = QStringLiteral(".pdf");
    }

    const QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("保存文件"),
        QStringLiteral("label%1").arg(defaultExt),
        filter
    );

    if (fileName.isEmpty()) {
        return;
    }

    QGraphicsScene *renderScene = m_renderScene ? m_renderScene.get() : m_scene;
    const QList<labelelement*> &exportElements = m_renderElements.isEmpty() ? m_elements : m_renderElements;

    if (!renderScene || exportElements.isEmpty()) {
        QMessageBox::warning(this, tr("导出失败"), tr("缺少可导出的内容"));
        return;
    }

    const bool hasBatch = m_batchManager && m_totalBatchCount > 0;
    int firstIndex = 0;
    int lastIndex = -1;
    if (hasBatch) {
        int start = ui->spinRangeStart->value();
        int end = ui->spinRangeEnd->value();
        if (start > end) {
            std::swap(start, end);
        }
        const int maxIndex = m_totalBatchCount - 1;
        firstIndex = qBound(0, start - 1, maxIndex);
        lastIndex = qBound(firstIndex, end - 1, maxIndex);
    }

    QString successMessage;

    if (fileType == "PDF") {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        if (m_baseContext.pageLayout.isValid()) {
            printer.setPageLayout(m_baseContext.pageLayout);
        } else {
            printer.setPageOrientation(QPageLayout::Portrait);
            printer.setPageSize(QPageSize::A4);
        }

        PrintContext context = m_baseContext;
        context.pageLayout = printer.pageLayout();
        context.contentMargins = m_baseContext.contentMargins.isNull()
                                 ? context.pageLayout.margins(QPageLayout::Millimeter)
                                 : m_baseContext.contentMargins;
        context.sourceScene = renderScene;

        // 分别导出：每条记录一个 PDF 文件
    if (hasBatch && exportMode == ExportMode::Separate) {
            const QFileInfo baseInfo(fileName);
            const QDir targetDir = baseInfo.dir();
            const QString baseName = baseInfo.completeBaseName();
            const QString suffix = baseInfo.suffix().isEmpty() ? QStringLiteral("pdf") : baseInfo.suffix();
            const int count = lastIndex - firstIndex + 1;
            const int digits = std::max(3, static_cast<int>(QString::number(count).length()));

            int exported = 0;
            for (int idx = 0; idx < count; ++idx) {
                const int recordIndex = firstIndex + idx;

                QVector<PreviewBinding> bindings;
                auto restoreGuard = qScopeGuard([&bindings]() { restorePreviewBindings(bindings); });

                QString dataError;
                if (!applyPreviewRecord(exportElements, recordIndex, &bindings, &dataError)) {
                    QMessageBox::warning(this, tr("导出失败"), dataError.isEmpty() ? tr("无法应用数据源记录") : dataError);
                    return;
                }

                const QString indexString = QString::number(idx + 1).rightJustified(digits, QLatin1Char('0'));
                const QString targetPath = targetDir.filePath(QStringLiteral("%1_%2.%3").arg(baseName, indexString, suffix));

                printer.setOutputFileName(targetPath);
                context.printer = &printer;
                context.pageLayout = printer.pageLayout();

                QString errorMsg;
                if (!m_printEngine->printOnce(exportElements, context, &errorMsg)) {
                    QMessageBox::warning(this, tr("导出失败"), errorMsg.isEmpty() ? tr("无法生成PDF文件") : errorMsg);
                    return;
                }

                ++exported;
            }

            successMessage = tr("已分别导出 %1 个 PDF 文件至目录:\n%2")
                                 .arg(exported)
                                 .arg(QDir::toNativeSeparators(targetDir.absolutePath()));
    } else if (!hasBatch && exportMode == ExportMode::Separate) {
            // 无数据源且选择分别导出：按“导出份数”生成多个单页 PDF
            const QFileInfo baseInfo(fileName);
            const QDir targetDir = baseInfo.dir();
            const QString baseName = baseInfo.completeBaseName();
            const QString suffix = baseInfo.suffix().isEmpty() ? QStringLiteral("pdf") : baseInfo.suffix();

            const int copies = ui->spinCopies ? std::max(1, ui->spinCopies->value()) : 1;
            const int digits = std::max(3, static_cast<int>(QString::number(copies).length()));

            int exported = 0;
            for (int i = 0; i < copies; ++i) {
                const QString indexString = QString::number(i + 1).rightJustified(digits, QLatin1Char('0'));
                const QString targetPath = targetDir.filePath(QStringLiteral("%1_%2.%3").arg(baseName, indexString, suffix));

                printer.setOutputFileName(targetPath);
                context.printer = &printer;
                context.pageLayout = printer.pageLayout();

                QString errorMsg;
                if (!m_printEngine->printOnce(exportElements, context, &errorMsg)) {
                    QMessageBox::warning(this, tr("导出失败"), errorMsg.isEmpty() ? tr("无法生成PDF文件") : errorMsg);
                    return;
                }
                ++exported;
            }

            successMessage = tr("已分别导出 %1 个 PDF 文件至目录:\n%2")
                                 .arg(exported)
                                 .arg(QDir::toNativeSeparators(targetDir.absolutePath()));
        } else {
            // 合并导出：单个 PDF
            printer.setOutputFileName(fileName);
            context.printer = &printer;
            context.pageLayout = printer.pageLayout();

            QString errorMsg;
            bool ok = false;
            if (hasBatch) {
                if (!m_batchManager) {
                    QMessageBox::warning(this, tr("导出失败"), tr("当前会话未配置批量导出"));
                    return;
                }
                m_batchManager->setElements(exportElements);
                ok = m_batchManager->execute(context, firstIndex, lastIndex, &errorMsg);
            } else {
                ok = m_printEngine->printOnce(exportElements, context, &errorMsg);
            }

            if (!ok) {
                QMessageBox::warning(this, tr("导出失败"), errorMsg.isEmpty() ? tr("无法生成PDF文件") : errorMsg);
                return;
            }

            const int recordCount = hasBatch ? (lastIndex - firstIndex + 1) : 1;
            successMessage = hasBatch
                ? tr("已导出包含 %1 条记录的 PDF:\n%2").arg(recordCount).arg(QDir::toNativeSeparators(fileName))
                : tr("文件已保存到:\n%1").arg(QDir::toNativeSeparators(fileName));
        }
    } else if (fileType == "PNG" || fileType == "JPEG") {
        QPrinter dummyPrinter(QPrinter::HighResolution);
        if (m_baseContext.pageLayout.isValid()) {
            dummyPrinter.setPageLayout(m_baseContext.pageLayout);
        }
        PrintContext context = m_baseContext;
        context.printer = &dummyPrinter;
        context.pageLayout = dummyPrinter.pageLayout();
        context.contentMargins = m_baseContext.contentMargins.isNull()
                                 ? context.pageLayout.margins(QPageLayout::Millimeter)
                                 : m_baseContext.contentMargins;
        context.sourceScene = renderScene;

        const qreal qualityScale = 3.0;
        const qreal designWidthPx = m_baseContext.labelSizePixels.width() > 0.0
                                        ? m_baseContext.labelSizePixels.width()
                                        : previewImageSize().width();
        const qreal designHeightPx = m_baseContext.labelSizePixels.height() > 0.0
                                         ? m_baseContext.labelSizePixels.height()
                                         : previewImageSize().height();

        const int exportWidth = qMax(1, static_cast<int>(std::ceil(designWidthPx * qualityScale)));
        const int exportHeight = qMax(1, static_cast<int>(std::ceil(designHeightPx * qualityScale)));
        const QSize exportSize(exportWidth, exportHeight);

        auto clampDpi = [](qreal value) {
            return std::clamp(value, 150.0, 600.0);
        };

        qreal dpiX = 0.0;
        qreal dpiY = 0.0;
        if (m_baseContext.labelSizeMM.width() > 0.0) {
            dpiX = clampDpi(static_cast<qreal>(exportWidth) * 25.4 / m_baseContext.labelSizeMM.width());
        }
        if (m_baseContext.labelSizeMM.height() > 0.0) {
            dpiY = clampDpi(static_cast<qreal>(exportHeight) * 25.4 / m_baseContext.labelSizeMM.height());
        }
        if (dpiX <= 0.0 && dpiY <= 0.0) {
            dpiX = dpiY = 300.0;
        } else if (dpiX <= 0.0) {
            dpiX = dpiY;
        } else if (dpiY <= 0.0) {
            dpiY = dpiX;
        }

        QVector<int> recordIndices;
        if (hasBatch) {
            for (int i = firstIndex; i <= lastIndex; ++i) {
                recordIndices.append(i);
            }
        }
        if (recordIndices.isEmpty()) {
            recordIndices.append(-1);
        }

        const QFileInfo fileInfo(fileName);
        const QDir targetDir = fileInfo.dir();
        const QString baseName = fileInfo.completeBaseName();
        const QString suffix = fileInfo.completeSuffix();
        const int digits = recordIndices.size() > 1 ? QString::number(recordIndices.size()).length() : 0;

        QStringList savedFiles;
        for (int idx = 0; idx < recordIndices.size(); ++idx) {
            const int recordIndex = recordIndices.at(idx);

            QVector<PreviewBinding> bindings;
            auto restoreGuard = qScopeGuard([&bindings]() {
                restorePreviewBindings(bindings);
            });

            if (recordIndex >= 0) {
                QString dataError;
                if (!applyPreviewRecord(exportElements, recordIndex, &bindings, &dataError)) {
                    QMessageBox::warning(this, tr("导出失败"), dataError.isEmpty() ? tr("无法应用数据源记录") : dataError);
                    return;
                }
            }

            QString errorMsg;
            QImage image = m_printEngine->renderPreview(exportElements, context, exportSize, &errorMsg, dpiX, dpiY);
            if (image.isNull()) {
                QMessageBox::warning(this, tr("导出失败"), errorMsg.isEmpty() ? tr("无法生成图像文件") : errorMsg);
                return;
            }

            if (fileType == "JPEG" && image.format() != QImage::Format_RGB32) {
                image = image.convertToFormat(QImage::Format_RGB32);
            }

            QString targetPath;
            if (recordIndices.size() == 1) {
                targetPath = fileName;
            } else {
                const QString indexString = QString::number(idx + 1).rightJustified(std::max(3, digits), QLatin1Char('0'));
                targetPath = targetDir.filePath(QStringLiteral("%1_%2.%3").arg(baseName, indexString, suffix));
            }

            if (!image.save(targetPath)) {
                QMessageBox::warning(this, tr("导出失败"), tr("无法写入文件 %1").arg(QDir::toNativeSeparators(targetPath)));
                return;
            }

            savedFiles.append(QDir::toNativeSeparators(targetPath));
        }

        if (savedFiles.size() == 1) {
            successMessage = tr("文件已保存到:\n%1").arg(savedFiles.first());
        } else {
            successMessage = tr("已导出 %1 个图像文件至目录:\n%2")
                                 .arg(savedFiles.size())
                                 .arg(QDir::toNativeSeparators(targetDir.absolutePath()));
        }
    }

    if (!successMessage.isEmpty()) {
        QMessageBox::information(this, tr("完成"), successMessage);
    }
}

void PrintCenterDialog::onCopiesChanged(int value)
{
    Q_UNUSED(value);
    updateCopySummary();
}
