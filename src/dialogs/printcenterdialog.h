#ifndef PRINTCENTERDIALOG_H
#define PRINTCENTERDIALOG_H

#include <QDialog>
#include <QPixmap>
#include <QSize>
#include <QHash>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

#include "../printing/printcontext.h"

namespace Ui {
class PrintCenterDialog;
}

class LabelScene;
class BatchPrintManager;
class PrintEngine;
class labelelement;
class QGraphicsScene;
class QGraphicsPixmapItem;

class PrintCenterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PrintCenterDialog(LabelScene* scene, PrintEngine* engine, QWidget *parent = nullptr);
    ~PrintCenterDialog();

    // 设置批量打印管理器
    void setBatchPrintManager(BatchPrintManager* manager);
    // 设置元素列表
    void setElements(const QList<labelelement*>& elements);
    // 设置打印上下文基线信息
    void setBaseContext(const PrintContext& context);

private slots:
    // 预览工具栏
    void onFitView();
    void onZoomIn();
    void onZoomOut();

    // 批量导航
    void onPrevBatch();
    void onNextBatch();
    void onBatchSelected(int index);

    // 页面导航
    void onPageInputChanged();

    // 文件类型切换
    void onFileTypeChanged(int index);

    // 导出方式切换
    void onExportModeChanged(int index);

    // 操作按钮
    void onLayoutExport();
    void onPrintDirect();
    void onDownload();
    void onCopiesChanged(int value);

private:
    struct CachedPreview {
        QPixmap pixmap;
        QSize logicalSize;
        qreal devicePixelRatio = 1.0;
    };

    void setupConnections();
    void updatePreview();
    void updateSizeLabel();
    void showBatchNavigator(bool show);
    void showPageNavigator(bool show);
    void updateBatchButtons();
    void refreshBatchControls();
    void clampBatchIndex();
    int resolveRecordCount(const QList<labelelement*>& elements, QString *errorMessage) const;
    QSize previewImageSize() const;
    void updateCopySummary();
    void updatePreviewStatus(bool busy);
    void updatePreviewControlsEnabled(bool enabled);
    void applyZoom();
    void ensureBatchWindowVisible();
    void invalidatePreviewCache();
    QString previewCacheKey(const QSize& renderSize, int recordIndex) const;
    void prunePreviewCache();
    void rebuildRenderModel();

    Ui::PrintCenterDialog *ui;
    LabelScene* m_scene;
    PrintEngine* m_printEngine;
    BatchPrintManager* m_batchManager;
    PrintContext m_baseContext;
    QList<labelelement*> m_elements;
    std::vector<std::unique_ptr<labelelement>> m_renderElementStorage;
    QList<labelelement*> m_renderElements;
    std::unique_ptr<QGraphicsScene> m_renderScene;
    QPixmap m_currentPreview;
    QGraphicsScene* m_previewScene;
    QGraphicsPixmapItem* m_previewPixmapItem;
    qreal m_zoomFactor;
    qreal m_previewDevicePixelRatio;
    QHash<QString, CachedPreview> m_previewCache;
    QStringList m_previewCacheOrder;
    bool m_previewUpdateScheduled;
    bool m_suppressSceneInvalidation;
    int m_currentBatchIndex;
    int m_totalBatchCount;
    int m_batchWindowStart;
    QSize m_previewBaseSize;
};

#endif // PRINTCENTERDIALOG_H
