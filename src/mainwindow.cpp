#include "mainwindow.h"
#include "graphics/qrcodeitem.h"
#include "graphics/barcodeitem.h"
#include "graphics/textitem.h"
#include "graphics/lineitem.h"
#include "graphics/rectangleitem.h"
#include "graphics/circleitem.h"
#include "graphics/staritem.h"
#include "graphics/arrowitem.h"
#include "graphics/polygonitem.h"
#include "graphics/tableitem.h"
#include "graphics/alignableitem.h"
#include "dialogs/printcenterdialog.h"
#include <QGraphicsRectItem>
#include <QGraphicsProxyWidget>
#include <QFileDialog>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QApplication>
#include <QScreen>
#include <QPrinter>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPageSize>
#include "dialogs/layoutexportdialog.h"
#include "dialogs/assetbrowserdialog.h"
#include <QPainter>
#include <QSettings>
#include <QSignalBlocker>
#include <QBuffer>
#include <cmath>
#include <algorithm>
#include <memory>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "core/barcodeelement.h"
#include "core/imageelement.h"
#include "core/datasource.h"
#include "core/qrcodeelement.h"
#include "core/textelement.h"
#include "core/lineelement.h"
#include "core/rectangleelement.h"
#include "core/circleelement.h"
#include "core/starelement.h"
#include "core/arrowelement.h"
#include "core/polygonelement.h"
#include "core/tableelement.h"
#include "printing/printengine.h"
#include "printing/defaultprintrenderer.h"
#include "printing/printcontext.h"
#include "printing/batchprintmanager.h"
#include "panels/labelpropswidget.h"
#include "panels/databaseprintwidget.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("SimpleLabel"));

    // 获取屏幕尺寸并设置窗口大小为屏幕的80%
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int defaultWidth = screenGeometry.width() * 0.8;
    int defaultHeight = screenGeometry.height() * 0.8;

    // 只设置初始大小，不再保存初始窗口大小用于重置
    resize(defaultWidth, defaultHeight);

    // 设置最小窗口尺寸，防止窗口被拖得过小
    setMinimumSize(640, 480);    // 初始化场景和视图
    scene = new LabelScene(this);
    view = new QGraphicsView(scene);
    view->setRenderHint(QPainter::Antialiasing);
    view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    view->setBackgroundBrush(QColor("#E0E0E0"));
    view->viewport()->installEventFilter(this);
    view->setDragMode(QGraphicsView::RubberBandDrag);

    // 初始化内联文本编辑器
    textEditor = new InlineTextEditor(this);
    connect(textEditor, &InlineTextEditor::editingFinished, 
            this, &MainWindow::onTextEditingFinished);
    connect(textEditor, &InlineTextEditor::editingCancelled,
            this, &MainWindow::onTextEditingCancelled);

    // 初始化撤销管理器
    undoManager = new UndoManager(scene, this);
    
    // 设置场景的撤销管理器
    scene->setUndoManager(undoManager);
        initializePrinting(); // Initialize printing functionality

    // 创建其他组件
    createActions();
    createCentralWidget();
    createDockWindows();
    createMenus();
    createToolbars();
    createStatusBar();
    setInitialStyles();
    connectSignals();

    // 读取上次的窗口设置
    //readSettings();
}

MainWindow::~MainWindow() = default;

void MainWindow::insertTable()
{
    if (!scene) return;

    // 基于当前标签背景尺寸或场景尺寸选择一个合理的初始大小
    QSizeF labelSize(300, 200); // fallback 默认值
    QRectF bgRect;
    {
        QGraphicsPathItem *labelItem = nullptr;
        const QList<QGraphicsItem*> existingItems = scene->items(Qt::AscendingOrder);
        for (QGraphicsItem *it : existingItems) {
            if (it && it->data(0).toString() == "labelBackground") {
                labelItem = qgraphicsitem_cast<QGraphicsPathItem*>(it);
                break;
            }
        }
        if (labelItem) {
            bgRect = labelItem->sceneBoundingRect();
            labelSize = bgRect.size();
        } else {
            // 如果没有背景，用 sceneRect 的中心附近区域估算
            QRectF sr = scene->sceneRect();
            labelSize = sr.size() * 0.4; // 取 40% 作为一个中等大小
        }
    }

    const int rows = 3;
    const int cols = 3;
    // 给列/行留两边内边距(padding)，避免贴边
    qreal cellW = labelSize.width() / (cols + 2);
    qreal cellH = labelSize.height() / (rows + 2);
    cellW = std::max<qreal>(20.0, cellW);
    cellH = std::max<qreal>(15.0, cellH);

    auto *tableItem = new TableItem(rows, cols, cellW, cellH);
    tableItem->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);

    // 居中放置
    QRectF targetRect = bgRect.isValid() ? bgRect : scene->sceneRect();
    QRectF br = tableItem->boundingRect();
    QPointF pos(targetRect.center().x() - br.width()/2.0,
                targetRect.center().y() - br.height()/2.0);
    tableItem->setPos(pos);

    scene->addItem(tableItem);
    scene->clearSelection();
    tableItem->setSelected(true);
    // 标记文档被修改
    isModified = true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    //writeSettings();
    event->accept();
}

bool MainWindow::maybeSave()
{
    // 若撤销栈存在且为干净状态，直接允许关闭
    if (undoManager) {
        if (undoManager->isClean()) {
            return true;
        }
    } else {
        // 无撤销管理器时，使用 isModified 作为兜底判断
        if (!isModified) {
            return true;
        }
    }

    const QMessageBox::StandardButton ret
    = QMessageBox::warning(this, tr("SimpleLabel"),
                             tr("文档已被修改。\n"
                                "是否保存更改？"),
                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    switch (ret) {
        case QMessageBox::Save:
            return saveFile();
        case QMessageBox::Cancel:
            return false;
        case QMessageBox::Discard:
        default:
            return true;
    }
}

// 文件操作相关的槽函数实现
void MainWindow::newFile()
{
    TemplateCenterDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // 关键修复：新建开始时清空当前文件路径，避免后续保存覆盖旧文件
        currentFile.clear();
        if (dialog.isTemplateSelected()) {
            // 用户选择了模板，直接加载模板文件
            QString templatePath = dialog.selectedTemplatePath();
            
            if (loadFile(templatePath)) {
                // 模板加载后清除当前文件路径，强制另存为
                m_templateSourcePath = templatePath;
                currentFile.clear();
                setWindowTitle(tr("SimpleLabel - 未命名"));
                isModified = false;
                if (undoManager) {
                    undoManager->setClean();
                }
            } else {
                // 加载失败，创建默认标签
                widthLineEdit->setText(QString::number(100));
                heightLineEdit->setText(QString::number(75));
                paperSizeComboBox->setCurrentText(tr("自定义尺寸"));
                updateLabelSize();
                setWindowTitle(tr("SimpleLabel - 未命名"));
                m_templateSourcePath.clear();
                // 确保清空当前文件路径
                currentFile.clear();
            }
        } else {
            // 用户选择创建空白标签
            widthLineEdit->setText(QString::number(dialog.labelWidth()));
            heightLineEdit->setText(QString::number(dialog.labelHeight()));
            paperSizeComboBox->setCurrentText(tr("自定义尺寸"));
            updateLabelSize();
            setWindowTitle(tr("SimpleLabel - %1").arg(dialog.labelName()));
            m_templateSourcePath.clear();
            // 清空当前文件路径，防止覆盖旧文件
            currentFile.clear();
        }

        // 重置工具按钮状态
        selectAction->setChecked(true);
        if (scene) {
            scene->setMode(LabelScene::SelectMode);
        }

        // 标记为未修改状态（新建或从模板创建）
        isModified = false;
        if (undoManager) {
            undoManager->setClean();
        }
    }
}

void MainWindow::openFile()
{
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this,
            tr("打开标签文件"), "",
            tr("标签文件 (*.lbl);;所有文件 (*)"));

        if (!fileName.isEmpty()) {
            loadFile(fileName);
        }
    }
}

void MainWindow::clearScene()
{
    if (!scene) {
        return;
    }

    QGraphicsItem* selectionFrame = scene->selectionFrame();
    foreach (QGraphicsItem* item, scene->items()) {
        if (!item || item == selectionFrame) {
            continue;
        }
        // 不删除背景
        if (item->data(0).toString() != "labelBackground") {
            scene->removeItem(item);
            delete item;
        }
    }

    // 清空撤销栈，避免后续撤销操作访问已删除的对象
    if (undoManager) {
        undoManager->clear();
    }

    m_itemDataSources.clear();
}

bool MainWindow::loadFile(const QString &fileName)
{
    m_templateSourcePath.clear();

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
    QMessageBox::warning(this, tr("SimpleLabel"),
                            tr("无法读取文件 %1:\n%2.")
                            .arg(QDir::toNativeSeparators(fileName),
                                file.errorString()));
        return false;
    }

    QByteArray fileData = file.readAll();
    QJsonDocument document = QJsonDocument::fromJson(fileData);
    QJsonObject rootObject = document.object();

    // 清除当前场景
    clearScene();

    // 读取标签尺寸信息
    QJsonObject labelInfo = rootObject["labelInfo"].toObject();
    double width = labelInfo["width"].toDouble();
    double height = labelInfo["height"].toDouble();

    widthLineEdit->setText(QString::number(width));
    heightLineEdit->setText(QString::number(height));
    updateLabelSize();

    // 加载所有元素
    QJsonArray itemsArray = rootObject["items"].toArray();
    for (const auto &itemValue : itemsArray) {
        QJsonObject itemObject = itemValue.toObject();

        // 使用工厂方法从JSON创建元素
        auto element = labelelement::createFromJson(itemObject);
        if (element) {
            // 将元素添加到场景
            element->addToScene(scene);

            // 从JSON获取位置并设置
            QPointF pos(itemObject["x"].toDouble(), itemObject["y"].toDouble());
            element->setPos(pos);

            if (element->hasDataSource()) {
                if (QGraphicsItem* addedItem = element->getItem()) {
                    DataSourceBinding binding;
                    binding.source = element->dataSource();
                    binding.enabled = element->isDataSourceEnabled();
                    if (binding.source) {
                        m_itemDataSources.insert(addedItem, binding);
                    }
                }
            }
        }
    }

    m_originalLabelWidthMM = width;
    m_originalLabelHeightMM = height;
    currentFile = fileName;
    setWindowTitle(tr("SimpleLabel - %1").arg(QFileInfo(currentFile).fileName()));
    isModified = false;
    
    // 设置撤销管理器的clean状态
    if (undoManager) {
        undoManager->setClean();
    }

    return true;
}

bool MainWindow::saveFile()
{
    if (currentFile.isEmpty()) {
        return saveFileAs();
    }
    return saveFile(currentFile);
}

bool MainWindow::saveFileAs()
{
    QString defaultPath;
    if (!currentFile.isEmpty()) {
        defaultPath = currentFile;
    } else if (!m_templateSourcePath.isEmpty()) {
        QString baseName = QFileInfo(m_templateSourcePath).completeBaseName();
        if (baseName.isEmpty()) {
            baseName = tr("未命名");
        }
        QDir homeDir(QDir::homePath());
        defaultPath = homeDir.filePath(baseName + ".lbl");
    }

    QString fileName = QFileDialog::getSaveFileName(this,
        tr("保存标签文件"), defaultPath,
        tr("标签文件 (*.lbl);;所有文件 (*)"));

    if (fileName.isEmpty())
        return false;

    return saveFile(fileName);
}

bool MainWindow::saveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
    QMessageBox::warning(this, tr("SimpleLabel"),
                           tr("无法写入文件 %1:\n%2.")
                           .arg(QDir::toNativeSeparators(fileName),
                               file.errorString()));
        return false;
    }

    QJsonObject rootObject;

    // 保存文档基本信息
    rootObject["version"] = "1.0";
    rootObject["createTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObject["lastModified"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 保存标签尺寸信息
    QJsonObject labelInfo;
    labelInfo["width"] = widthLineEdit->text().toDouble();
    labelInfo["height"] = heightLineEdit->text().toDouble();
    rootObject["labelInfo"] = labelInfo;

    // 保存场景属性
    QJsonObject sceneProperties;
    sceneProperties["width"] = scene->width();
    sceneProperties["height"] = scene->height();
    rootObject["sceneProperties"] = sceneProperties;    // 保存所有图形项
    QJsonArray itemsArray;
    auto applyBindingToElement = [this](labelelement& element, QGraphicsItem* itemPtr) {
        auto it = m_itemDataSources.constFind(itemPtr);
        if (it != m_itemDataSources.constEnd() && it.value().source) {
            element.setDataSource(it.value().source);
            element.setDataSourceEnabled(it.value().enabled);
        }
    };
    const QList<QGraphicsItem*> orderedItems = scene->items(Qt::AscendingOrder);
    QGraphicsItem* selectionFrame = scene->selectionFrame();
    for (QGraphicsItem *item : orderedItems) {
        if (!item || item == selectionFrame) {
            continue;
        }
        // 跳过背景标签项
        if (item->data(0).toString() == "labelBackground") {
            continue;
        }

        // 处理二维码项
        if (auto qrItem = dynamic_cast<QRCodeItem*>(item)) {
            QRCodeElement element(qrItem);
            applyBindingToElement(element, qrItem);
            // 使用 labelelement 的 toJson 方法获取完整的 JSON 对象
            itemsArray.append(element.toJson());
        }        // 处理条形码项
        else if (auto barcodeItem = dynamic_cast<BarcodeItem*>(item)) {
            BarcodeElement element(barcodeItem);
            applyBindingToElement(element, barcodeItem);
            // 使用 labelelement 的 toJson 方法获取完整的 JSON 对象
            itemsArray.append(element.toJson());
        }
        // 处理文本项
        else if (auto textItem = dynamic_cast<TextItem*>(item)) {
            TextElement element(textItem);
            applyBindingToElement(element, textItem);
            // 使用 labelelement 的 toJson 方法获取完整的 JSON 对象
            itemsArray.append(element.toJson());
        }
        // 处理图像项
        else if (auto imageItem = dynamic_cast<ImageItem*>(item)) {
            ImageElement element(imageItem);
            // 使用 labelelement 的 toJson 方法获取完整的 JSON 对象
            itemsArray.append(element.toJson());
        }
        else if (auto lineItem = dynamic_cast<LineItem*>(item)) {
            LineElement element(lineItem);
            applyBindingToElement(element, lineItem);
            itemsArray.append(element.toJson());
        }
        else if (auto rectangleItem = dynamic_cast<RectangleItem*>(item)) {
            RectangleElement element(rectangleItem);
            applyBindingToElement(element, rectangleItem);
            itemsArray.append(element.toJson());
        }
        else if (auto circleItem = dynamic_cast<CircleItem*>(item)) {
            CircleElement element(circleItem);
            applyBindingToElement(element, circleItem);
            itemsArray.append(element.toJson());
        }
        else if (auto starItem = dynamic_cast<StarItem*>(item)) {
            StarElement element(starItem);
            applyBindingToElement(element, starItem);
            itemsArray.append(element.toJson());
        }
        else if (auto arrowItem = dynamic_cast<ArrowItem*>(item)) {
            ArrowElement element(arrowItem);
            applyBindingToElement(element, arrowItem);
            itemsArray.append(element.toJson());
        }
        else if (auto polyItem = dynamic_cast<PolygonItem*>(item)) {
            PolygonElement element(polyItem);
            applyBindingToElement(element, polyItem);
            itemsArray.append(element.toJson());
        }
        else if (auto tableItem = dynamic_cast<TableItem*>(item)) {
            TableElement element(tableItem);
            itemsArray.append(element.toJson());
        }
        // 后续可以添加其他类型的元素处理
    }
    rootObject["items"] = itemsArray;

    // 将JSON对象写入文件
    QJsonDocument document(rootObject);
    file.write(document.toJson(QJsonDocument::Indented));

    currentFile = fileName;
    m_templateSourcePath.clear();
    setWindowTitle(tr("SimpleLabel - %1").arg(QFileInfo(currentFile).fileName()));
    isModified = false;
    
    // 设置撤销管理器的clean状态
    if (undoManager) {
        undoManager->setClean();
    }

    return true;
}

void MainWindow::openPrintCenter()
{
    if (!scene) {
        QMessageBox::warning(this, tr("打印/导出"), tr("没有可打印的内容"));
        return;
    }

    if (!m_printEngine) {
        initializePrinting();
    }

    const QList<labelelement*> elements = collectElements(false);
    if (elements.isEmpty()) {
        QMessageBox::warning(this, tr("打印/导出"), tr("没有可打印的元素"));
        return;
    }

    // 检查是否有启用数据源的元素（用于批量打印）
    const bool hasDataSource = std::any_of(elements.cbegin(), elements.cend(), [](labelelement *element) {
        return element && element->isDataSourceEnabled();
    });

    // 创建打印中心对话框
    PrintCenterDialog dialog(scene, m_printEngine.get(), this);

    QPrinter previewPrinter(QPrinter::HighResolution);
    PrintContext baseContext = buildPrintContext(&previewPrinter);
    baseContext.printer = nullptr;
    dialog.setBaseContext(baseContext);

    dialog.setElements(elements);
    
    // 如果有数据源，设置批量打印管理器
    if (hasDataSource) {
        if (!m_batchPrintManager) {
            m_batchPrintManager = std::make_unique<BatchPrintManager>(m_printEngine.get());
            connect(m_batchPrintManager.get(), &BatchPrintManager::recordPrinted, this, [this](int index) {
                if (statusBar()) {
                    statusBar()->showMessage(tr("正在打印第 %1 条记录").arg(index + 1), 1000);
                }
            });
        } else {
            m_batchPrintManager->setEngine(m_printEngine.get());
        }
        m_batchPrintManager->setElements(elements);
        dialog.setBatchPrintManager(m_batchPrintManager.get());
    }
    
    // 显示对话框
    dialog.exec();
}

// 排版导出：将当前设计按网格排列到整页并导出为 PDF
/*
void MainWindow::openLayoutExportDialog()
{
    if (!scene) {
        QMessageBox::warning(this, tr("排版导出"), tr("当前没有内容用于排版导出"));
        return;
    }

    LayoutExportDialog dlg(this);
    // 传入标签物理尺寸用于自动计算行列
    if (widthLineEdit && heightLineEdit) {
        bool okW=false, okH=false;
        const double w = widthLineEdit->text().toDouble(&okW);
        const double h = heightLineEdit->text().toDouble(&okH);
        if (okW && okH && w>0.0 && h>0.0) {
            dlg.setLabelSizeMM(QSizeF(w,h));
        }
    }
    if (dlg.exec() != QDialog::Accepted) {
        return; // 用户取消
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("保存排版文件"), QString(), tr("PDF 文件 (*.pdf)"));
    if (fileName.isEmpty()) {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(fileName);

    QSizeF pageMM = dlg.pageSizeMM();
    QPageSize pageSize(QSize(int(pageMM.width()), int(pageMM.height())), QPageSize::Millimeter);
    QPageLayout layout(pageSize, dlg.landscape() ? QPageLayout::Landscape : QPageLayout::Portrait, QMarginsF());
    printer.setPageLayout(layout);

    const bool horizontalOrder = dlg.horizontalOrder();
    QList<labelelement*> elements = collectElements(false);
    if (elements.isEmpty()) {
        QMessageBox::warning(this, tr("排版导出"), tr("没有可导出的元素"));
        return;
    }

    QRectF designRect;
    bool first = true;
    for (labelelement *el : elements) {
        if (!el) continue;
        if (QGraphicsItem *it = el->getItem()) {
            QRectF r = it->sceneBoundingRect();
            if (first) { designRect = r; first = false; }
            else { designRect = designRect.united(r); }
        }
    }
    if (first) {
        designRect = scene->sceneRect();
    }
    if (designRect.width() <= 0 || designRect.height() <= 0) {
        designRect = QRectF(0,0,100,100);
    }

    // 导出期间暂时隐藏选择框，避免出现在成品中
    QGraphicsItem* selFrame = scene->selectionFrame();
    bool frameVisible = selFrame ? selFrame->isVisible() : false;
    if (selFrame) selFrame->setVisible(false);

    QPainter painter(&printer);
    painter.setRenderHint(QPainter::Antialiasing,true);
    painter.setRenderHint(QPainter::TextAntialiasing,true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform,true);

    const QRectF pageRectPx = printer.pageRect(QPrinter::DevicePixel);

    // 使用物理尺寸确保大小一致
    auto mmToDevice = [](double mm, double dpi){ return (dpi / 25.4) * mm; };
    const qreal dpiX = printer.logicalDpiX();
    const qreal dpiY = printer.logicalDpiY();
    // 从当前标签尺寸（属性面板）获取尺寸
    bool okW=false, okH=false;
    double labelWmm = widthLineEdit ? widthLineEdit->text().toDouble(&okW) : 0.0;
    double labelHmm = heightLineEdit ? heightLineEdit->text().toDouble(&okH) : 0.0;
    if (!okW || !okH || labelWmm<=0.0 || labelHmm<=0.0) {
        labelWmm = DEFAULT_LABEL_WIDTH; labelHmm = DEFAULT_LABEL_HEIGHT;
    }
    const qreal labelPxW = std::max<qreal>(1.0, mmToDevice(labelWmm, dpiX));
    const qreal labelPxH = std::max<qreal>(1.0, mmToDevice(labelHmm, dpiY));
    const int useCols = std::max(1, static_cast<int>(std::floor(pageRectPx.width()/labelPxW)));
    const int useRows = std::max(1, static_cast<int>(std::floor(pageRectPx.height()/labelPxH)));
    const int cellsPerPage = useRows * useCols;

    auto indexToRowCol = [useRows, useCols, horizontalOrder](int index)->QPair<int,int>{
        if (horizontalOrder) {
            int r = index / useCols;
            int c = index % useCols;
            return {r,c};
        } else {
            int c = index / useRows;
            int r = index % useRows;
            return {r,c};
        }
    };

    const int totalCells = cellsPerPage;
    for (int i = 0; i < totalCells; ++i) {
        auto rc = indexToRowCol(i);
        int r = rc.first;
        int c = rc.second;
        painter.save();
        qreal x = pageRectPx.left() + c * labelPxW;
        qreal y = pageRectPx.top() + r * labelPxH;
        painter.translate(x, y);
        QRectF targetCell(0,0,labelPxW,labelPxH);
        scene->render(&painter, targetCell, designRect, Qt::IgnoreAspectRatio);
        painter.restore();
    }

    painter.end();

    // 恢复选择框可见性
    if (selFrame) selFrame->setVisible(frameVisible);
    statusBar()->showMessage(tr("排版导出完成: %1").arg(QFileInfo(fileName).fileName()), 3000);
}
*/

void MainWindow::createActions()
{
    // 文件操作动作
    newFileAction = new QAction(style()->standardIcon(QStyle::SP_FileIcon), tr("新建(&N)"), this);
    newFileAction->setShortcut(QKeySequence::New);
    connect(newFileAction, SIGNAL(triggered()), this, SLOT(newFile()));

    openFileAction = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("打开(&O)"), this);
    openFileAction->setShortcut(QKeySequence::Open);
    connect(openFileAction, SIGNAL(triggered()), this, SLOT(openFile()));

    saveAction = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("保存(&S)"), this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, SIGNAL(triggered()), this, SLOT(saveFile()));

    saveAsAction = new QAction(tr("另存为(&A)..."), this);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, SIGNAL(triggered()), this, SLOT(saveFileAs()));

    // 统一打印中心动作
    printCenterAction = new QAction(QIcon(":/icons/print-export.svg"), tr("打印/导出(&P)"), this);
    printCenterAction->setShortcut(QKeySequence::Print);
    connect(printCenterAction, &QAction::triggered, this, &MainWindow::openPrintCenter);

    // 素材库入口
    assetBrowserAction = new QAction(QIcon(":/icons/assets.svg"), tr("素材(&M)"), this);
    connect(assetBrowserAction, &QAction::triggered, this, &MainWindow::openAssetBrowser);

    // 排版导出动作
    //layoutExportAction = new QAction(tr("排版导出"), this);
    //connect(layoutExportAction, &QAction::triggered, this, &MainWindow::openLayoutExportDialog);

    exitAction = new QAction(tr("退出(&X)"), this);
    exitAction->setShortcut(tr("Ctrl+Q"));
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));

    // 编辑操作动作 - 使用撤销管理器提供的动作
    undoAction = undoManager->undoAction();
    redoAction = undoManager->redoAction();

    cutAction = new QAction(style()->standardIcon(QStyle::SP_DialogDiscardButton), tr("剪切(&T)"), this);
    cutAction->setShortcut(QKeySequence::Cut);
    cutAction->setEnabled(false);
    connect(cutAction,&QAction::triggered,this,&MainWindow::cutSelectedItem);

    copyAction = new QAction(style()->standardIcon(QStyle::SP_DialogApplyButton), tr("复制(&C)"), this);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setEnabled(false);
    connect(copyAction,&QAction::triggered,this,&MainWindow::copySelectedItem);

    pasteAction = new QAction(style()->standardIcon(QStyle::SP_DialogOkButton), tr("粘贴(&P)"), this);
    pasteAction->setShortcut(QKeySequence::Paste);
    pasteAction->setEnabled(false);    // 工具动作
    connect(pasteAction,&QAction::triggered,this,&MainWindow::pasteSelectedItem);
    selectAction = new QAction(QIcon(":/icons/select.svg"), tr("选择"), this);
    selectAction->setCheckable(true);
    selectAction->setChecked(true);
    QObject::connect(selectAction, &QAction::triggered, this, [this]() {
        if (scene) {
            scene->setMode(LabelScene::SelectMode);
        }
    });

    textAction = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("文本"), this);
    textAction->setCheckable(true);
    QObject::connect(textAction, &QAction::triggered, this, [this]() {
        if (scene) {
            scene->setMode(LabelScene::TextMode);
        }
    });

    barcodeAction = new QAction(QIcon(":/icons/barcode.svg"), tr("条形码"), this);
    barcodeAction->setCheckable(true);
    QObject::connect(barcodeAction, &QAction::triggered, this, [this]() {
        if (scene) {
            scene->setMode(LabelScene::BarcodeMode);
        }
    });

    qrCodeAction = new QAction(QIcon(":/icons/qrcode.svg"), tr("二维码"), this);
    qrCodeAction->setCheckable(true);
    QObject::connect(qrCodeAction, &QAction::triggered, this, [this]() {
        if (scene) {
            scene->setMode(LabelScene::QRCodeMode);
        }
    });

    imageAction = new QAction(QIcon(":/icons/image.svg"), tr("图像"), this);
    imageAction->setCheckable(true);
    QObject::connect(imageAction, &QAction::triggered, this, [this]() {
        if (scene) {
            scene->setMode(LabelScene::ImageMode);
        }
    });

    lineAction = new QAction(QIcon(":/icons/line.svg"), tr("直线"), this);
    lineAction->setCheckable(true);
    QObject::connect(lineAction, &QAction::triggered, this, [this]() {
        if (scene) {
            scene->setMode(LabelScene::LineMode);
        }
    });

    rectangleAction = new QAction(QIcon(":/icons/rectangle.svg"), tr("矩形"), this);
    rectangleAction->setCheckable(true);
    QObject::connect(rectangleAction, &QAction::triggered, this, [this]() {
        if (scene) {
            scene->setMode(LabelScene::RectangleMode);
        }
    });

    circleAction = new QAction(QIcon(":/icons/circle.svg"), tr("圆形"), this);
    circleAction->setCheckable(true);
    QObject::connect(circleAction, &QAction::triggered, this, [this]() {
        if (scene) {
            scene->setMode(LabelScene::CircleMode);
        }
    });

    // 新增：星形、箭头、多边形工具动作
    starAction = new QAction(QIcon(":/icons/star.svg"), tr("星形"), this);
    starAction->setCheckable(true);
    QObject::connect(starAction, &QAction::triggered, this, [this]() {
        if (scene) {
            scene->setMode(LabelScene::StarMode);
        }
    });

    arrowAction = new QAction(QIcon(":/icons/arrow.svg"), tr("箭头"), this);
    arrowAction->setCheckable(true);
    QObject::connect(arrowAction, &QAction::triggered, this, [this]() {
        if (scene) {
            scene->setMode(LabelScene::ArrowMode);
        }
    });

    polygonAction = new QAction(QIcon(":/icons/polygon.svg"), tr("多边形"), this);
    polygonAction->setCheckable(true);
    QObject::connect(polygonAction, &QAction::triggered, this, [this]() {
        if (scene) {
            scene->setMode(LabelScene::PolygonMode);
        }
    });

    // 表格动作：直接插入一个默认表格
    tableAction = new QAction(QIcon(":/icons/table.svg"), tr("表格"), this);
    connect(tableAction, &QAction::triggered, this, &MainWindow::insertTable);

    // 创建工具动作组
    toolsGroup = new QActionGroup(this);
    toolsGroup->addAction(selectAction);
    toolsGroup->addAction(textAction);
    toolsGroup->addAction(barcodeAction);
    toolsGroup->addAction(qrCodeAction);
    toolsGroup->addAction(imageAction);
    toolsGroup->addAction(lineAction);
    toolsGroup->addAction(rectangleAction);
    toolsGroup->addAction(circleAction);
    toolsGroup->addAction(starAction);
    toolsGroup->addAction(arrowAction);
    toolsGroup->addAction(polygonAction);
    toolsGroup->addAction(tableAction);
    toolsGroup->setExclusive(true);
    selectAction->setChecked(true);

    // 连接场景的模式改变信号
    QObject::connect(scene, &LabelScene::modeChanged, this, [this](LabelScene::Mode mode) {
        // 当模式改变时，更新所有按钮状态
        selectAction->setChecked(mode == LabelScene::SelectMode);
        textAction->setChecked(mode == LabelScene::TextMode);
        barcodeAction->setChecked(mode == LabelScene::BarcodeMode);
        qrCodeAction->setChecked(mode == LabelScene::QRCodeMode);
        imageAction->setChecked(mode == LabelScene::ImageMode);
        lineAction->setChecked(mode == LabelScene::LineMode);
        rectangleAction->setChecked(mode == LabelScene::RectangleMode);
        circleAction->setChecked(mode == LabelScene::CircleMode);
        if (starAction) starAction->setChecked(mode == LabelScene::StarMode);
        if (arrowAction) arrowAction->setChecked(mode == LabelScene::ArrowMode);
        if (polygonAction) polygonAction->setChecked(mode == LabelScene::PolygonMode);
    });    // 文本格式动作

    alignLeftAction = new QAction(style()->standardIcon(QStyle::SP_MediaSeekBackward), tr("左对齐"), this);
    alignLeftAction->setCheckable(false);
    alignLeftAction->setChecked(true);

    alignCenterAction = new QAction(style()->standardIcon(QStyle::SP_MediaStop), tr("居中"), this);
    alignCenterAction->setCheckable(false);

    alignRightAction = new QAction(style()->standardIcon(QStyle::SP_MediaSeekForward), tr("右对齐"), this);
    alignRightAction->setCheckable(false);

    // 创建对齐动作组
    QActionGroup *alignmentGroup = new QActionGroup(this);
    alignmentGroup->addAction(alignLeftAction);
    alignmentGroup->addAction(alignCenterAction);
    alignmentGroup->addAction(alignRightAction);
    alignmentGroup->setExclusive(true);

    // 连接对齐动作的信号槽
    connect(alignLeftAction, &QAction::triggered, this, &MainWindow::alignItemsLeft);
    connect(alignCenterAction, &QAction::triggered, this, &MainWindow::alignItemsCenter);
    connect(alignRightAction, &QAction::triggered, this, &MainWindow::alignItemsRight);

    copyItemAction = new QAction(tr("复制"), this);
    copyItemAction->setShortcut(QKeySequence::Copy);
    connect(copyItemAction, &QAction::triggered, this, &MainWindow::copySelectedItem);

    deleteItemAction = new QAction(tr("删除"), this);
    deleteItemAction->setShortcut(QKeySequence::Delete);
    connect(deleteItemAction, &QAction::triggered, this, &MainWindow::deleteSelectedItem);

    rotateItemAction = new QAction(tr("旋转90°"), this);
    rotateItemAction->setShortcut(QKeySequence(tr("Ctrl+R")));
    connect(rotateItemAction, &QAction::triggered, this, &MainWindow::rotateSelectedItem);

    bringToFrontAction = new QAction(tr("置于顶层"), this);
    bringToFrontAction->setShortcut(QKeySequence(tr("Ctrl+Shift+]")));
    connect(bringToFrontAction, &QAction::triggered, this, &MainWindow::bringSelectedItemToFront);

    sendToBackAction = new QAction(tr("置于底层"), this);
    sendToBackAction->setShortcut(QKeySequence(tr("Ctrl+Shift+[")));
    connect(sendToBackAction, &QAction::triggered, this, &MainWindow::sendSelectedItemToBack);

    bringForwardAction = new QAction(tr("上移一层"), this);
    bringForwardAction->setShortcut(QKeySequence(tr("Ctrl+]")));
    connect(bringForwardAction, &QAction::triggered, this, &MainWindow::bringSelectedItemForward);

    sendBackwardAction = new QAction(tr("下移一层"), this);
    sendBackwardAction->setShortcut(QKeySequence(tr("Ctrl+[")));
    connect(sendBackwardAction, &QAction::triggered, this, &MainWindow::sendSelectedItemBackward);

    // 规格化动作：裁剪所有选中项使其完全位于标签背景内
    normalizeAction = new QAction(tr("规格化"), this);
    normalizeAction->setToolTip(tr("裁剪/收缩使选中对象完全落入标签区域"));
    connect(normalizeAction, &QAction::triggered, this, &MainWindow::normalizeSelectedItems);
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    fileMenu->addAction(newFileAction);
    fileMenu->addAction(openFileAction);
    fileMenu->addSeparator();
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(printCenterAction);
    //fileMenu->addAction(layoutExportAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    editMenu = menuBar()->addMenu(tr("编辑(&E)"));
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();
    editMenu->addAction(cutAction);
    editMenu->addAction(copyAction);
    editMenu->addAction(pasteAction);

    viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    if (objectPropertiesDock) {
        QAction* toggleDockAction = objectPropertiesDock->toggleViewAction();
        toggleDockAction->setText(tr("属性面板")); // 设置更友好的显示文本
        toggleDockAction->setShortcut(QKeySequence(tr("Ctrl+P"))); // 可选：添加快捷键
        viewMenu->addAction(objectPropertiesDock->toggleViewAction());
    }


    insertMenu = menuBar()->addMenu(tr("插入(&I)"));
    insertMenu->addAction(textAction);
    insertMenu->addAction(barcodeAction);
    insertMenu->addAction(qrCodeAction);
    insertMenu->addAction(imageAction);
    insertMenu->addSeparator();
    insertMenu->addAction(assetBrowserAction);
    insertMenu->addSeparator();
    // “图形”子菜单：收纳直线/矩形/圆形/星形/箭头/多边形
    shapesMenu = insertMenu->addMenu(tr("图形"));
    shapesMenu->addAction(lineAction);
    shapesMenu->addAction(rectangleAction);
    shapesMenu->addAction(circleAction);
    shapesMenu->addAction(starAction);
    shapesMenu->addAction(arrowAction);
    shapesMenu->addAction(polygonAction);
    shapesMenu->addAction(tableAction);

    textMenu = menuBar()->addMenu(tr("图形项(&T)"));
    textMenu->addAction(alignLeftAction);
    textMenu->addAction(alignCenterAction);
    textMenu->addAction(alignRightAction);

    objectMenu = menuBar()->addMenu(tr("对象(&O)"));
    objectMenu->addAction(copyItemAction);
    objectMenu->addAction(deleteItemAction);
    objectMenu->addSeparator();
    objectMenu->addAction(rotateItemAction);
    objectMenu->addSeparator();
    objectMenu->addAction(bringToFrontAction);
    objectMenu->addAction(bringForwardAction);
    objectMenu->addAction(sendBackwardAction);
    objectMenu->addAction(sendToBackAction);
    objectMenu->addSeparator();
    objectMenu->addAction(normalizeAction);

    windowMenu = menuBar()->addMenu(tr("窗口(&W)"));
    helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
}

void MainWindow::createToolbars()
{
    mainToolBar = addToolBar(tr("主工具栏"));
    mainToolBar->setObjectName("mainToolBar");
    mainToolBar->setMovable(false);
    mainToolBar->setIconSize(QSize(24, 24));
    mainToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    mainToolBar->addAction(newFileAction);
    mainToolBar->addAction(openFileAction);
    mainToolBar->addAction(saveAction);
    mainToolBar->addAction(printCenterAction);
    mainToolBar->addAction(assetBrowserAction);
    //mainToolBar->addAction(layoutExportAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(undoAction);
    mainToolBar->addAction(redoAction);
    mainToolBar->addAction(cutAction);
    mainToolBar->addAction(copyAction);
    mainToolBar->addAction(pasteAction);
    mainToolBar->addAction(normalizeAction);

    drawingToolBar = addToolBar(tr("绘图工具栏"));
    drawingToolBar->setObjectName("drawingToolBar");
    drawingToolBar->setMovable(false);
    drawingToolBar->setIconSize(QSize(24, 24));
    drawingToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    drawingToolBar->addAction(selectAction);
    drawingToolBar->addAction(textAction);
    drawingToolBar->addAction(barcodeAction);
    drawingToolBar->addAction(qrCodeAction);
    drawingToolBar->addAction(imageAction);
    drawingToolBar->addSeparator();
    // 使用下拉按钮收纳图形动作
    shapesToolButton = new QToolButton(drawingToolBar);
    shapesToolButton->setText(tr("图形"));
    shapesToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    shapesToolButton->setPopupMode(QToolButton::MenuButtonPopup);
    // 共享同一菜单，或可创建独立菜单引用相同动作
    shapesToolButton->setMenu(shapesMenu);
    // 设置一个默认动作，点击主按钮时使用
    shapesToolButton->setDefaultAction(lineAction);
    if (shapesMenu) {
        connect(shapesMenu, &QMenu::triggered, this, [this](QAction* act){
            if (shapesToolButton && act) {
                shapesToolButton->setDefaultAction(act);
            }
        });
    }
    drawingToolBar->addWidget(shapesToolButton);

    textToolBar = addToolBar(tr("文本工具栏"));
    textToolBar->setObjectName("textToolBar");
    textToolBar->setMovable(false);
    textToolBar->setIconSize(QSize(24, 24));
    textToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    textToolBar->addAction(alignLeftAction);
    textToolBar->addAction(alignCenterAction);
    textToolBar->addAction(alignRightAction);

    fontComboBox = new QFontComboBox();
    fontComboBox->setFixedWidth(150);
    textToolBar->addWidget(fontComboBox);

    fontSizeSpinBox = new QSpinBox();
    fontSizeSpinBox->setRange(2, 200);
    fontSizeSpinBox->setValue(12);
    fontSizeSpinBox->setFixedWidth(50);
    textToolBar->addWidget(fontSizeSpinBox);
}

void MainWindow::createCentralWidget()
{
    // 创建简单布局，只包含视图
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(view);

    QWidget *centralWidget = new QWidget;
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);

    // 创建新的标尺系统 - 改用叠加式标尺
    createRulers();
}

void MainWindow::createDockWindows()
{
    objectPropertiesDock = new QDockWidget(tr("标签属性"), this);
    objectPropertiesDock->setObjectName("propertiesDock");
    objectPropertiesDock->setAllowedAreas(Qt::RightDockWidgetArea);
    objectPropertiesDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    objectPropertiesDock->setTitleBarWidget(new QWidget());

    objectPropertiesDock->setMinimumWidth(350);

    propertiesWidget = new QWidget();
    propertiesScrollArea = new QScrollArea();
    propertiesScrollArea->setWidget(propertiesWidget);
    propertiesScrollArea->setWidgetResizable(true);

    QVBoxLayout *propertiesLayout = new QVBoxLayout(propertiesWidget);
    propertiesLayout->setSpacing(10);
    propertiesLayout->setContentsMargins(10, 10, 10, 10);

    // Use LabelPropsWidget for label size
    auto *labelProps = new LabelPropsWidget(propertiesWidget);
    labelSizeGroup = labelProps; // keep pointer compatibility
    paperSizeComboBox = labelProps->paperSizeCombo();
    widthLineEdit = labelProps->widthEdit();
    heightLineEdit = labelProps->heightEdit();
    applyButton = labelProps->applyButton();
    resetButton = labelProps->resetButton();
    labelCornerRadiusSpin = labelProps->cornerRadiusSpin();

    propertiesLayout->addWidget(labelSizeGroup);    // 码类设置组（条形码和二维码）
    codeGroup = new QGroupBox(tr("码类设置"));
    QFormLayout *codeLayout = new QFormLayout(codeGroup);

    codeTypeComboBox = new QComboBox();
    codeTypeComboBox->addItems({
        "Code 128",
        "EAN-13",
        "UPC-A", 
        "Code 39",
        "QR Code",
        "PDF417",
        "Data Matrix",
        "Aztec"
    });

    codeDataEdit = new QLineEdit();
    codeWidthSpin = new QSpinBox();
    codeHeightSpin = new QSpinBox();

    codeWidthSpin->setRange(10, 1000);
    codeHeightSpin->setRange(10, 1000);
    codeWidthSpin->setValue(100);
    codeHeightSpin->setValue(30);

    codeLayout->addRow(tr("类型:"), codeTypeComboBox);
    codeLayout->addRow(tr("内容:"), codeDataEdit);
    codeLayout->addRow(tr("宽度:"), codeWidthSpin);
    codeLayout->addRow(tr("高度:"), codeHeightSpin);

    codeDatabaseWidget = new DatabasePrintWidget(codeGroup);
    codeLayout->addRow(codeDatabaseWidget);

    // 文本设置组
    textGroup = new QGroupBox(tr("文本设置"));
    QFormLayout *textLayout = new QFormLayout(textGroup);

    // 文本内容
    textContentEdit = new QLineEdit();
    textContentEdit->setPlaceholderText(tr("输入文本内容"));
    
    // 为属性面板创建独立的字体选择器
    propertiesFontComboBox = new QFontComboBox();
    
    // 为属性面板创建独立的字体大小选择器
    propertiesFontSizeSpinBox = new QSpinBox();
    propertiesFontSizeSpinBox->setRange(2, 200);
    propertiesFontSizeSpinBox->setValue(12);
    
    // 如果工具栏的字体组件还没有创建，创建它们
    if (!fontComboBox) {
        fontComboBox = new QFontComboBox();
    }
    if (!fontSizeSpinBox) {
        fontSizeSpinBox = new QSpinBox();
    fontSizeSpinBox->setRange(2, 200);
        fontSizeSpinBox->setValue(12);
    }

    fontStyleCombo = new QComboBox();
    fontStyleCombo->addItems({
        tr("常规"),
        tr("粗体"),
        tr("斜体"),
        tr("粗斜体")
    });

    //字距调整
    letterSpacingSpin = new QSpinBox();
    letterSpacingSpin->setRange(-5, 20);
    letterSpacingSpin->setValue(0);
    letterSpacingSpin->setSuffix(" px");

    // 文本颜色按钮
    textColorButton = new QPushButton();
    textColorButton->setFixedSize(30, 25);
    textColorButton->setStyleSheet("background-color: black; border: 1px solid gray;");
    textColorButton->setToolTip(tr("文本颜色"));

    // 背景颜色按钮
    textBackgroundColorButton = new QPushButton();
    textBackgroundColorButton->setFixedSize(30, 25);
    textBackgroundColorButton->setStyleSheet("background-color: white; border: 1px solid gray;");
    textBackgroundColorButton->setToolTip(tr("背景颜色"));

    // 边框颜色按钮
    textBorderColorButton = new QPushButton();
    textBorderColorButton->setFixedSize(30, 25);
    textBorderColorButton->setStyleSheet("background-color: black; border: 1px solid gray;");
    textBorderColorButton->setToolTip(tr("边框颜色"));

    // 文本对齐
    textAlignmentCombo = new QComboBox();
    textAlignmentCombo->addItems({
        tr("左对齐"),
        tr("居中对齐"),
        tr("右对齐")
    });

    // 边框宽度
    borderWidthSpin = new QSpinBox();
    borderWidthSpin->setRange(1, 10);
    borderWidthSpin->setValue(1);
    borderWidthSpin->setSuffix(" px");

    // 复选框选项
    wordWrapCheck = new QCheckBox(tr("自动换行"));
    autoResizeCheck = new QCheckBox(tr("自动调整大小"));
    autoResizeCheck->setChecked(true);
    borderEnabledCheck = new QCheckBox(tr("显示边框"));
    backgroundEnabledCheck = new QCheckBox(tr("显示背景"));

    // 创建颜色按钮的水平布局
    QHBoxLayout *textColorLayout = new QHBoxLayout();
    textColorLayout->addWidget(textColorButton);
    textColorLayout->addWidget(new QLabel(tr("文字")));
    textColorLayout->addStretch();
    QWidget *textColorWidget = new QWidget();
    textColorWidget->setLayout(textColorLayout);

    QHBoxLayout *bgColorLayout = new QHBoxLayout();
    bgColorLayout->addWidget(textBackgroundColorButton);
    bgColorLayout->addWidget(new QLabel(tr("背景")));
    bgColorLayout->addStretch();
    QWidget *bgColorWidget = new QWidget();
    bgColorWidget->setLayout(bgColorLayout);

    QHBoxLayout *borderColorLayout = new QHBoxLayout();
    borderColorLayout->addWidget(textBorderColorButton);
    borderColorLayout->addWidget(new QLabel(tr("边框")));
    borderColorLayout->addStretch();
    QWidget *borderColorWidget = new QWidget();
    borderColorWidget->setLayout(borderColorLayout);

    // 添加所有控件到布局
    textLayout->addRow(tr("内容:"), textContentEdit);
    textLayout->addRow(tr("字体:"), propertiesFontComboBox);
    textLayout->addRow(tr("大小:"), propertiesFontSizeSpinBox);
    textLayout->addRow(tr("样式:"), fontStyleCombo);
    textLayout->addRow(tr("字距:"), letterSpacingSpin);
    textLayout->addRow(tr("对齐:"), textAlignmentCombo);
    textLayout->addRow(tr("文本颜色:"), textColorWidget);
    textLayout->addRow(tr("背景颜色:"), bgColorWidget);
    textLayout->addRow(tr("边框颜色:"), borderColorWidget);
    textLayout->addRow(tr("边框宽度:"), borderWidthSpin);
    textLayout->addRow(wordWrapCheck);
    textLayout->addRow(autoResizeCheck);
    textLayout->addRow(borderEnabledCheck);
    textLayout->addRow(backgroundEnabledCheck);

    textDatabaseWidget = new DatabasePrintWidget(textGroup);
    textLayout->addRow(textDatabaseWidget);

    // 图像设置组
    imageGroup = new QGroupBox(tr("图像设置"));
    QFormLayout *imageLayout = new QFormLayout(imageGroup);

    // 加载图像按钮
    loadImageButton = new QPushButton(tr("加载图像..."));
    loadImageButton->setToolTip(tr("从文件加载图像"));

    // 图像尺寸
    imageWidthSpin = new QSpinBox();
    imageWidthSpin->setRange(10, 2000);
    imageWidthSpin->setValue(200);
    imageWidthSpin->setSuffix(" px");

    imageHeightSpin = new QSpinBox();
    imageHeightSpin->setRange(10, 2000);
    imageHeightSpin->setValue(200);
    imageHeightSpin->setSuffix(" px");

    // 保持宽高比
    keepAspectRatioCheck = new QCheckBox(tr("保持宽高比"));
    keepAspectRatioCheck->setChecked(true);

    // 透明度
    imageOpacitySpin = new QSpinBox();
    imageOpacitySpin->setRange(0, 100);
    imageOpacitySpin->setValue(100);
    imageOpacitySpin->setSuffix(" %");

    // 添加控件到布局
    imageLayout->addRow(loadImageButton);
    imageLayout->addRow(tr("宽度:"), imageWidthSpin);
    imageLayout->addRow(tr("高度:"), imageHeightSpin);
    imageLayout->addRow(keepAspectRatioCheck);
    imageLayout->addRow(tr("透明度:"), imageOpacitySpin);

    // 绘图工具设置组
    drawingGroup = new QGroupBox(tr("绘图设置"));
    QFormLayout *drawingLayout = new QFormLayout(drawingGroup);

    // 线条宽度
    lineWidthSpin = new QSpinBox();
    lineWidthSpin->setRange(1, 20);
    lineWidthSpin->setValue(2);
    lineWidthSpin->setSuffix(" px");

    // 线条颜色
    lineColorButton = new QPushButton();
    lineColorButton->setFixedSize(30, 25);
    lineColorButton->setStyleSheet("background-color: black; border: 1px solid gray;");
    lineColorButton->setToolTip(tr("线条颜色"));

    // 填充颜色
    fillColorButton = new QPushButton();
    fillColorButton->setFixedSize(30, 25);
    fillColorButton->setStyleSheet("background-color: lightblue; border: 1px solid gray;");
    fillColorButton->setToolTip(tr("填充颜色"));

    // 填充开关
    fillEnabledCheck = new QCheckBox(tr("启用填充"));
    fillEnabledCheck->setChecked(false);

    // 保持圆形
    keepCircleCheck = new QCheckBox(tr("保持圆形"));
    keepCircleCheck->setChecked(true);

    // 线条角度（与竖直方向夹角）
    lineAngleSpin = new QDoubleSpinBox();
    lineAngleSpin->setRange(0.0, 360.0);
    lineAngleSpin->setDecimals(1);
    lineAngleSpin->setSingleStep(1.0);
    lineAngleSpin->setValue(0.0);
    lineAngleSpin->setToolTip(tr("调整线条与竖直方向的夹角 (0-360°)"));

    // 线条虚线开关
    lineDashedCheck = new QCheckBox(tr("虚线"));
    lineDashedCheck->setToolTip(tr("切换线条虚线/实线样式"));

    // 矩形圆角半径
    cornerRadiusSpin = new QSpinBox();
    cornerRadiusSpin->setRange(0, 500); // 实际显示时再动态限制
    cornerRadiusSpin->setValue(0);
    cornerRadiusSpin->setSuffix(" px");
    cornerRadiusSpin->setToolTip(tr("设置矩形圆角半径"));

    // 添加控件到布局
    drawingLayout->addRow(tr("线条宽度:"), lineWidthSpin);
    drawingLayout->addRow(tr("线条颜色:"), lineColorButton);
    drawingLayout->addRow(tr("填充颜色:"), fillColorButton);
    drawingLayout->addRow(fillEnabledCheck);
    drawingLayout->addRow(keepCircleCheck);
    drawingLayout->addRow(tr("线条角度:"), lineAngleSpin);
    drawingLayout->addRow(tr("线条样式:"), lineDashedCheck);
    drawingLayout->addRow(tr("圆角半径:"), cornerRadiusSpin);

    // 将所有组添加到主布局（默认仅显示全局“标签尺寸”组，其它按选择显示）
    propertiesLayout->addWidget(labelSizeGroup);
    propertiesLayout->addWidget(codeGroup);
    propertiesLayout->addWidget(textGroup);
    propertiesLayout->addWidget(imageGroup);
    propertiesLayout->addWidget(drawingGroup);

    // 初始可见性：全局标签组常显，其余隐藏，避免面板臃肿
    labelSizeGroup->setVisible(true);
    codeGroup->setVisible(false);
    textGroup->setVisible(false);
    imageGroup->setVisible(false);
    drawingGroup->setVisible(false);
    propertiesLayout->addStretch();

    objectPropertiesDock->setWidget(propertiesScrollArea);
    addDockWidget(Qt::RightDockWidgetArea, objectPropertiesDock);
}

void MainWindow::createStatusBar()
{
    positionLabel = new QLabel(tr("位置: (0, 0)"));
    positionLabel->setMinimumWidth(100);

    sizeLabel = new QLabel(tr("大小: 100 × 75 mm"));
    sizeLabel->setMinimumWidth(150);

    zoomLabel = new QLabel(tr("缩放: 100%"));
    zoomLabel->setMinimumWidth(100);

    statusBar()->addWidget(positionLabel);
    statusBar()->addWidget(sizeLabel);
    statusBar()->addPermanentWidget(zoomLabel);
}

void MainWindow::setInitialStyles()
{
    // 设置应用程序样式表
    QString styleSheet = R"(
        QMainWindow {
            background-color: #f0f0f0;
        }

        QToolBar {
            border: none;
            background-color: #ffffff;
            spacing: 5px;
            padding: 2px;
        }

        QToolButton {
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 4px;
            background-color: transparent;
        }

        QToolButton:hover {
            background-color: #e0e0e0;
            border: 1px solid #c0c0c0;
        }

        QToolButton:checked {
            background-color: #d0d0d0;
            border: 1px solid #a0a0a0;
        }

        QDockWidget {
            border: 1px solid #c0c0c0;
        }

        QDockWidget::title {
            background: #e0e0e0;
            padding: 4px;
        }

        QGroupBox {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            margin-top: 16px;
            padding: 15px 10px 10px 10px;
            background-color: #f8f8f8;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 8px;
            padding: 0 5px;
            background-color: #f8f8f8;
            font-weight: bold;
            color: #404040;
        }

        QLabel {
            color: #404040;
            padding: 2px;
        }

        QPushButton {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 5px 15px;
            background-color: #ffffff;
            min-width: 80px;
        }

        QPushButton:hover {
            background-color: #e0e0e0;
        }

        QPushButton:pressed {
            background-color: #d0d0d0;
        }

        QLineEdit, QSpinBox, QComboBox {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 4px;
            background-color: #ffffff;
            min-height: 20px;
        }

        QComboBox {
            padding-right: 20px;
        }

        QComboBox::drop-down {
            border: none;
            width: 20px;
        }

        QComboBox::down-arrow {
            image: url(:/icons/down-arrow.png);
            width: 12px;
            height: 12px;
        }

        QScrollArea {
            border: none;
            background-color: transparent;
        }

        #propertiesWidget {
            background-color: #f8f8f8;
        }

        QStatusBar {
            background-color: #ffffff;
            border-top: 1px solid #c0c0c0;
        }

        QStatusBar::item {
            border: none;
        }
    )";

    setStyleSheet(styleSheet);

    // 设置属性面板的布局间距
    if (propertiesWidget) {
        QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(propertiesWidget->layout());
        if (layout) {
            layout->setSpacing(12);
            layout->setContentsMargins(12, 12, 12, 12);
        }
    }    // 设置各个组的布局间距
    QList<QGroupBox*> groups = {labelSizeGroup, codeGroup, textGroup};
    for (QGroupBox* group : groups) {
        if (group) {
            QFormLayout* layout = qobject_cast<QFormLayout*>(group->layout());
            if (layout) {
                layout->setSpacing(8);
                layout->setContentsMargins(8, 8, 8, 8);
                layout->setLabelAlignment(Qt::AlignLeft);
                layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
            }
        }
    }
}

void MainWindow::connectSignals()
{
    // 撤销管理器信号连接
    connect(undoManager, &UndoManager::cleanChanged, this, [this](bool clean) {
        isModified = !clean;
        // 更新窗口标题显示修改状态
    QString title = tr("SimpleLabel");
        if (!currentFile.isEmpty()) {
            title += tr(" - %1").arg(QFileInfo(currentFile).fileName());
        }
        if (!clean) {
            title += tr(" *");
        }
            if (labelCornerRadiusSpin) {
                connect(labelCornerRadiusSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::updateLabelCornerRadius);
            }
        setWindowTitle(title);
    });


    // 标签尺寸相关连接
    connect(paperSizeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPaperSizeChanged);
    connect(applyButton, &QPushButton::clicked,
            this, &MainWindow::updateLabelSize);
    connect(resetButton, &QPushButton::clicked,
            this, &MainWindow::resetLabelSize);

    // 橡皮筋选择
    connect(view, &QGraphicsView::rubberBandChanged,
            this, &MainWindow::onRubberBandChanged);

    // 添加滚动条信号连接
    connect(view->horizontalScrollBar(), &QScrollBar::valueChanged,
         this, &MainWindow::updateRulerPosition);
    connect(view->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::updateRulerPosition);    // 添加码类属性面板的信号连接
    connectCodeSignals();

    // 添加文本属性面板的信号连接
    connectTextSignals();

    // 添加图像属性面板的信号连接
    connectImageSignals();

    // 添加绘图工具属性面板的信号连接
    connectDrawingSignals();    // 添加场景选择变更的监听
    connect(scene, &QGraphicsScene::selectionChanged,
            this, &MainWindow::updateSelectedItemProperties);
    connect(scene, &QGraphicsScene::selectionChanged,
            this, &MainWindow::updateContextMenuActions);
    // 选择变化时清除标尺参考线
    connect(scene, &QGraphicsScene::selectionChanged, this, [this]() {
        if (horizontalRuler && verticalRuler) {
            horizontalRuler->clearGuides();
            verticalRuler->clearGuides();
        }
    });

    connect(scene, &LabelScene::itemRemoved,
        this, &MainWindow::onSceneItemRemoved);

    if (codeDatabaseWidget) {
    connect(codeDatabaseWidget, &DatabasePrintWidget::applyRequested,
        this, &MainWindow::applyCodeDataSource);
    connect(codeDatabaseWidget, &DatabasePrintWidget::dataSourceEnabledChanged,
        this, &MainWindow::handleCodeDataSourceEnabledChanged);
    }
    if (textDatabaseWidget) {
    connect(textDatabaseWidget, &DatabasePrintWidget::applyRequested,
        this, &MainWindow::applyTextDataSource);
    connect(textDatabaseWidget, &DatabasePrintWidget::dataSourceEnabledChanged,
        this, &MainWindow::handleTextDataSourceEnabledChanged);
    }
    
    // 连接文本双击编辑信号
    connect(scene, &LabelScene::textItemDoubleClicked,
            this, &MainWindow::startTextEditing);
}

void MainWindow::onPaperSizeChanged(int index)
{
    switch (index) {
        case 0: // 自定义尺寸
            // 不做任何改变，保持当前输入框的值
            break;
        case 1: // 10mm × 20mm (小标签)
            widthLineEdit->setText("10");
            heightLineEdit->setText("20");
            updateLabelSize();
            break;
        case 2: // 30mm × 50mm (中标签)
            widthLineEdit->setText("30");
            heightLineEdit->setText("50");
            updateLabelSize();
            break;
        case 3: // 50mm × 70mm (大标签)
            widthLineEdit->setText("50");
            heightLineEdit->setText("70");
            updateLabelSize();
            break;
        case 4: // 70mm × 100mm (特大标签)
            widthLineEdit->setText("70");
            heightLineEdit->setText("100");
            updateLabelSize();
            break;
    }
}

// 修改 updateLabelSize 函数
void MainWindow::updateLabelSize()
{
    // 如果场景不存在，创建新场景
    if (!scene) {
        scene = new LabelScene(this);
        view->setScene(scene);
        
        // 设置撤销管理器
        undoManager->clear(); // 清空之前的撤销历史
        scene->setUndoManager(undoManager);
        
        scene->setBackgroundBrush(QColor("#E0E0E0"));
    }

    bool ok;
    double width = widthLineEdit->text().toDouble(&ok);
    if (!ok) width = DEFAULT_LABEL_WIDTH;

    double height = heightLineEdit->text().toDouble(&ok);
    if (!ok) height = DEFAULT_LABEL_HEIGHT;

    if (widthLineEdit) {
        QSignalBlocker blocker(widthLineEdit);
        widthLineEdit->setText(QString::number(width, 'g', 12));
    }
    if (heightLineEdit) {
        QSignalBlocker blocker(heightLineEdit);
        heightLineEdit->setText(QString::number(height, 'g', 12));
    }

    // 计算新的像素尺寸
    double labelWidth = width * MM_TO_PIXELS;
    double labelHeight = height * MM_TO_PIXELS;

    // 查找或创建标签背景
    QGraphicsPathItem *labelItem = nullptr;
    const QList<QGraphicsItem*> existingItems = scene->items(Qt::AscendingOrder);
    for (QGraphicsItem *item : existingItems) {
        if (item && item->data(0).toString() == "labelBackground") {
            labelItem = qgraphicsitem_cast<QGraphicsPathItem*>(item);
            break;
        }
    }

    int cornerPx = 0;
    if (labelCornerRadiusSpin) {
        cornerPx = labelCornerRadiusSpin->value();
        // 限制半径不超过标签宽高中较小的一半
        cornerPx = std::min(cornerPx, int(std::min(labelWidth, labelHeight) / 2.0));
    }

    QPainterPath path;
    if (cornerPx > 0) {
        path.addRoundedRect(QRectF(0,0,labelWidth,labelHeight), cornerPx, cornerPx);
    } else {
        path.addRect(QRectF(0, 0, labelWidth, labelHeight));
    }

    if (!labelItem) {
        labelItem = new QGraphicsPathItem(path);
        labelItem->setBrush(Qt::white);
        labelItem->setPen(Qt::NoPen);
        labelItem->setFlag(QGraphicsItem::ItemIsMovable, false);
        labelItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
        labelItem->setZValue(-1000);
        labelItem->setData(0, "labelBackground");
        scene->addItem(labelItem);
    } else {
        labelItem->setPath(path);
        labelItem->setBrush(Qt::white);
        labelItem->setPen(Qt::NoPen);
        labelItem->setZValue(-1000);
    }

    // 更新场景大小，设置足够大的边距以适应大标签
    double margin = qMax(200.0, qMax(labelWidth, labelHeight) * 0.2);
    QRectF sceneRect(-margin, -margin, labelWidth + 2*margin, labelHeight + 2*margin);
    scene->setSceneRect(sceneRect);

    // 更新标尺
    if (horizontalRuler && verticalRuler) {
        // 设置标尺长度
        horizontalRuler->setLength(width);  // 文档实际宽度，mm
        verticalRuler->setLength(height);   // 文档实际高度，mm

        // 确保比例一致
        updateRulerScale(zoomFactor);

        // 更新位置
        updateRulerPosition();

        // 显示标尺
        horizontalRuler->setVisible(true);
        verticalRuler->setVisible(true);
        cornerWidget->show();

    }

    // 显示标尺
    //horizontalRuler->show();
    //verticalRuler->show();

    // 确保滚动条策略正确设置
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 重置视图缩放比例，确保1:1显示
    setZoomFactor(1.0);

    // 居中显示标签
    if (labelItem) {
        view->centerOn(labelItem);
    }

    // 更新状态栏
    sizeLabel->setText(tr("大小: %1 × %2 mm").arg(width).arg(height));

    isModified = true;
}

void MainWindow::resetLabelSize()
{
    const double targetWidth = m_originalLabelWidthMM > 0.0 ? m_originalLabelWidthMM : DEFAULT_LABEL_WIDTH;
    const double targetHeight = m_originalLabelHeightMM > 0.0 ? m_originalLabelHeightMM : DEFAULT_LABEL_HEIGHT;

    widthLineEdit->setText(QString::number(targetWidth));
    heightLineEdit->setText(QString::number(targetHeight));
    paperSizeComboBox->setCurrentText(tr("自定义尺寸"));
    updateLabelSize();
}

void MainWindow::updateCodeTypeComboBox(bool isQRCode)
{
    // 阻断信号以避免触发变更事件
    QSignalBlocker blocker(codeTypeComboBox);
    
    // 清空现有选项
    codeTypeComboBox->clear();
    
    if (isQRCode) {
        // 二维码模式：只显示支持的二维码类型
        codeTypeComboBox->addItems({
            "QR Code",
            "PDF417", 
            "Data Matrix",
            "Aztec"
        });
    } else {
        // 条形码模式：显示支持的条形码类型
        codeTypeComboBox->addItems({
            "Code 128",
            "EAN-13",
            "UPC-A", 
            "Code 39"
        });
    }
}

void MainWindow::updateSelectedItemProperties()
{
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
    // 仅显示全局标签组，隐藏其他类型组
    labelSizeGroup->setVisible(true);
    codeGroup->setVisible(false);
    textGroup->setVisible(false);
    imageGroup->setVisible(false);
    drawingGroup->setVisible(false);

        if (codeDatabaseWidget) {
            codeDatabaseWidget->setEnabled(false);
            codeDatabaseWidget->clearInputs();
            codeDatabaseWidget->setDataSourceEnabled(false);
        }
        if (textDatabaseWidget) {
            textDatabaseWidget->setEnabled(false);
            textDatabaseWidget->clearInputs();
            textDatabaseWidget->setDataSourceEnabled(false);
        }
        
        // 恢复码类下拉框为完整列表
        QSignalBlocker blocker(codeTypeComboBox);
        codeTypeComboBox->clear();
        codeTypeComboBox->addItems({
            "Code 128",
            "EAN-13",
            "UPC-A", 
            "Code 39",
            "QR Code",
            "PDF417",
            "Data Matrix",
            "Aztec"
        });
        
        return;
    }

    // 获取选中项
    QGraphicsItem* item = selectedItems.first();

    // 处理条形码项
    BarcodeItem* barcodeItem = dynamic_cast<BarcodeItem*>(item);
    if (barcodeItem) {
    // 显示码类组，隐藏其它类型组
    labelSizeGroup->setVisible(true);
    codeGroup->setVisible(true);
    textGroup->setVisible(false);
    imageGroup->setVisible(false);
    drawingGroup->setVisible(false);

        if (codeDatabaseWidget) {
            codeDatabaseWidget->setEnabled(true);
            syncCodeDataSourceWidget(barcodeItem);
        }
        if (textDatabaseWidget) {
            textDatabaseWidget->setEnabled(false);
            textDatabaseWidget->clearInputs();
            textDatabaseWidget->setDataSourceEnabled(false);
        }

        // 更新码类下拉框为条形码类型
        updateCodeTypeComboBox(false); // false表示条形码

        // 阻断信号更新以避免循环触发
        QSignalBlocker blockType(codeTypeComboBox);
        QSignalBlocker blockData(codeDataEdit);
        QSignalBlocker blockWidth(codeWidthSpin);
        QSignalBlocker blockHeight(codeHeightSpin);

        // 设置条形码类型
        int typeIndex = 0; // 默认 CODE_128
        ZXing::BarcodeFormat format = barcodeItem->format();
        if (format == ZXing::BarcodeFormat::Code128) typeIndex = 0;
        else if (format == ZXing::BarcodeFormat::EAN13) typeIndex = 1;
        else if (format == ZXing::BarcodeFormat::UPCA) typeIndex = 2;
        else if (format == ZXing::BarcodeFormat::Code39) typeIndex = 3;
        codeTypeComboBox->setCurrentIndex(typeIndex);

        // 设置条形码数据
        codeDataEdit->setText(barcodeItem->data());

        // 设置条形码尺寸
        codeWidthSpin->setValue(barcodeItem->size().width());
        codeHeightSpin->setValue(barcodeItem->size().height());        // 显示显示文本选项（条形码支持）
        if (showBarcodeTextCheck) {
            showBarcodeTextCheck->setVisible(true);
            QSignalBlocker blockShowText(showBarcodeTextCheck);
            showBarcodeTextCheck->setChecked(barcodeItem->showHumanReadableText());
        }
    } 
    // 处理二维码项
    else {
        QRCodeItem* qrCodeItem = dynamic_cast<QRCodeItem*>(item);
        if (qrCodeItem) {
            // 显示码类组，隐藏其它类型组
            labelSizeGroup->setVisible(true);
            codeGroup->setVisible(true);
            textGroup->setVisible(false);
            imageGroup->setVisible(false);
            drawingGroup->setVisible(false);

            if (codeDatabaseWidget) {
                codeDatabaseWidget->setEnabled(true);
                syncCodeDataSourceWidget(qrCodeItem);
            }
            if (textDatabaseWidget) {
                textDatabaseWidget->setEnabled(false);
                textDatabaseWidget->clearInputs();
                textDatabaseWidget->setDataSourceEnabled(false);
            }

            // 更新码类下拉框为二维码类型
            updateCodeTypeComboBox(true); // true表示二维码

            // 阻断信号更新以避免循环触发
            QSignalBlocker blockType(codeTypeComboBox);
            QSignalBlocker blockData(codeDataEdit);
            QSignalBlocker blockWidth(codeWidthSpin);
            QSignalBlocker blockHeight(codeHeightSpin);

            // 设置二维码类型
            int typeIndex = 0; // 默认QR Code
            QString codeType = qrCodeItem->codeType();
            if (codeType == "QR") typeIndex = 0;
            else if (codeType == "PDF417") typeIndex = 1;
            else if (codeType == "DataMatrix") typeIndex = 2;
            else if (codeType == "Aztec") typeIndex = 3;
            codeTypeComboBox->setCurrentIndex(typeIndex);

            // 设置二维码数据
            codeDataEdit->setText(qrCodeItem->text());

            // 设置二维码尺寸
            codeWidthSpin->setValue(qrCodeItem->boundingRect().width());
            codeHeightSpin->setValue(qrCodeItem->boundingRect().height());            // 隐藏显示文本选项（二维码不支持）
            if (showBarcodeTextCheck) {
                showBarcodeTextCheck->setVisible(false);
            }        } else {
            // 处理文本项
            TextItem* textItem = dynamic_cast<TextItem*>(item);
            if (textItem) {
                // 显示文本组
                labelSizeGroup->setVisible(true);
                codeGroup->setVisible(false);
                textGroup->setVisible(true);
                imageGroup->setVisible(false);
                drawingGroup->setVisible(false);

                if (textDatabaseWidget) {
                    textDatabaseWidget->setEnabled(true);
                    syncTextDataSourceWidget(textItem);
                }
                if (codeDatabaseWidget) {
                    codeDatabaseWidget->setEnabled(false);
                    codeDatabaseWidget->clearInputs();
                    codeDatabaseWidget->setDataSourceEnabled(false);
                }
                
                // 阻断信号更新以避免循环触发
                QSignalBlocker blockContent(textContentEdit);
                QSignalBlocker blockFont(fontComboBox);
                QSignalBlocker blockSize(fontSizeSpinBox);
                QSignalBlocker blockPropertiesFont(propertiesFontComboBox);
                QSignalBlocker blockPropertiesSize(propertiesFontSizeSpinBox);
                QSignalBlocker blockAlignment(textAlignmentCombo);
                QSignalBlocker blockWordWrap(wordWrapCheck);
                QSignalBlocker blockAutoResize(autoResizeCheck);
                QSignalBlocker blockBorderEnabled(borderEnabledCheck);
                QSignalBlocker blockBackgroundEnabled(backgroundEnabledCheck);
                QSignalBlocker blockBorderWidth(borderWidthSpin);
                QSignalBlocker blockLetterSpacing(letterSpacingSpin);
                
                // 设置文本内容
                textContentEdit->setText(textItem->text());
                
                // 设置字体（同时更新工具栏和属性面板）
                QFont itemFont = textItem->font();
                fontComboBox->setCurrentFont(itemFont);
                fontSizeSpinBox->setValue(itemFont.pointSize());
                propertiesFontComboBox->setCurrentFont(itemFont);
                propertiesFontSizeSpinBox->setValue(itemFont.pointSize());
                // 设置字距
                if (letterSpacingSpin) {
                    letterSpacingSpin->setValue(static_cast<int>(std::round(textItem->letterSpacing())));
                }
                
                // 设置对齐方式
                Qt::Alignment alignment = textItem->alignment();
                int alignmentIndex = 0;
                if (alignment & Qt::AlignLeft) alignmentIndex = 0;
                else if (alignment & Qt::AlignCenter) alignmentIndex = 1;
                else if (alignment & Qt::AlignRight) alignmentIndex = 2;
                textAlignmentCombo->setCurrentIndex(alignmentIndex);
                  // 设置文本选项
                wordWrapCheck->setChecked(textItem->wordWrap());
                autoResizeCheck->setChecked(textItem->autoResize());
                borderEnabledCheck->setChecked(textItem->isBorderEnabled());
                backgroundEnabledCheck->setChecked(textItem->isBackgroundEnabled());
                
                // 设置边框宽度
                borderWidthSpin->setValue(textItem->borderWidth());
                
                // 隐藏显示文本选项（文本项不需要）
                if (showBarcodeTextCheck) {
                    showBarcodeTextCheck->setVisible(false);
                }
            } else {
                // 处理图像项
                ImageItem* imageItem = dynamic_cast<ImageItem*>(item);
                if (imageItem) {
                    // 显示图像组
                    labelSizeGroup->setVisible(true);
                    codeGroup->setVisible(false);
                    textGroup->setVisible(false);
                    imageGroup->setVisible(true);
                    drawingGroup->setVisible(false);

                    if (codeDatabaseWidget) {
                        codeDatabaseWidget->setEnabled(false);
                        codeDatabaseWidget->clearInputs();
                        codeDatabaseWidget->setDataSourceEnabled(false);
                    }
                    if (textDatabaseWidget) {
                        textDatabaseWidget->setEnabled(false);
                        textDatabaseWidget->clearInputs();
                        textDatabaseWidget->setDataSourceEnabled(false);
                    }

                    // 阻断信号更新以避免循环触发
                    QSignalBlocker blockWidth(imageWidthSpin);
                    QSignalBlocker blockHeight(imageHeightSpin);
                    QSignalBlocker blockAspectRatio(keepAspectRatioCheck);
                    QSignalBlocker blockOpacity(imageOpacitySpin);

                    // 设置图像尺寸
                    imageWidthSpin->setValue(static_cast<int>(imageItem->size().width()));
                    imageHeightSpin->setValue(static_cast<int>(imageItem->size().height()));

                    // 设置保持宽高比
                    keepAspectRatioCheck->setChecked(imageItem->keepAspectRatio());

                    // 设置透明度
                    imageOpacitySpin->setValue(static_cast<int>(imageItem->opacity() * 100));

                    // 隐藏显示文本选项（图像项不需要）
                    if (showBarcodeTextCheck) {
                        showBarcodeTextCheck->setVisible(false);
                    }
                } else {
                    // 其他类型（线条/矩形/圆形/星形/箭头/多边形等）显示绘图设置组
                    labelSizeGroup->setVisible(true);
                    codeGroup->setVisible(false);
                    textGroup->setVisible(false);
                    imageGroup->setVisible(false);
                    drawingGroup->setVisible(true);

                    if (codeDatabaseWidget) {
                        codeDatabaseWidget->setEnabled(false);
                        codeDatabaseWidget->clearInputs();
                        codeDatabaseWidget->setDataSourceEnabled(false);
                    }
                    if (textDatabaseWidget) {
                        textDatabaseWidget->setEnabled(false);
                        textDatabaseWidget->clearInputs();
                        textDatabaseWidget->setDataSourceEnabled(false);
                    }
                    // 控件可见性按照类型细化
                    bool isLine = dynamic_cast<LineItem*>(item) != nullptr;
                    bool isRect = dynamic_cast<RectangleItem*>(item) != nullptr;
                    bool isCircle = dynamic_cast<CircleItem*>(item) != nullptr;
                    bool isStar = dynamic_cast<StarItem*>(item) != nullptr;
                    bool isArrow = dynamic_cast<ArrowItem*>(item) != nullptr;
                    bool isPolygon = dynamic_cast<PolygonItem*>(item) != nullptr;
                    const bool canFill = isRect || isCircle || isStar || isPolygon;
                    if (fillColorButton) fillColorButton->setVisible(canFill);
                    if (fillEnabledCheck) fillEnabledCheck->setVisible(canFill);
                    if (keepCircleCheck) keepCircleCheck->setVisible(isCircle);
                    if (lineAngleSpin) lineAngleSpin->setVisible(isLine || isArrow);
                    if (lineDashedCheck) lineDashedCheck->setVisible(isLine || isArrow);
                    if (cornerRadiusSpin) cornerRadiusSpin->setVisible(isRect);
                    if (isLine && lineAngleSpin) {
                        LineItem* lineItem = static_cast<LineItem*>(item);
                        QSignalBlocker blk(lineAngleSpin);
                        lineAngleSpin->setValue(lineItem->angleFromVertical());
                        if (lineDashedCheck) {
                            QSignalBlocker blk2(lineDashedCheck);
                            lineDashedCheck->setChecked(lineItem->isDashed());
                        }
                    }
                    if (isArrow && lineAngleSpin) {
                        ArrowItem* arrowItem = static_cast<ArrowItem*>(item);
                        QSignalBlocker blk(lineAngleSpin);
                        lineAngleSpin->setValue(arrowItem->angleFromVertical());
                        if (lineDashedCheck) {
                            QSignalBlocker blk2(lineDashedCheck);
                            lineDashedCheck->setChecked(arrowItem->isDashed());
                        }
                    }
                    if (isRect && cornerRadiusSpin) {
                        RectangleItem* rectItem = static_cast<RectangleItem*>(item);
                        QSignalBlocker blk2(cornerRadiusSpin);
                        // 动态限制最大值为当前尺寸一半
                        int maxRadius = int(std::round(std::min(rectItem->size().width(), rectItem->size().height()) / 2.0));
                        cornerRadiusSpin->setMaximum(maxRadius);
                        cornerRadiusSpin->setValue(int(std::round(rectItem->cornerRadius())));
                    }
                }
            }
        }
    }
}

void MainWindow::updateLineAngle(double degrees)
{
    if (!scene) return;
    QList<QGraphicsItem*> sel = scene->selectedItems();
    if (sel.isEmpty()) return;
    LineItem* line = dynamic_cast<LineItem*>(sel.first());
    if (!line) return;
    QRectF oldGeom(line->startPoint(), line->endPoint());
    line->setAngleFromVertical(degrees);
    QRectF newGeom(line->startPoint(), line->endPoint());
    if (undoManager) {
        undoManager->resizeItem(line, oldGeom, newGeom);
    }
    isModified = true;
    scene->update();
}

void MainWindow::updateLineDashed(bool dashed)
{
    if (!scene) return;
    QList<QGraphicsItem*> sel = scene->selectedItems();
    if (sel.isEmpty()) return;
    LineItem* line = dynamic_cast<LineItem*>(sel.first());
    if (!line) return;
    QPen oldPen = line->pen();
    QPen newPen = oldPen;
    newPen.setStyle(dashed ? Qt::DashLine : Qt::SolidLine);
    if (undoManager) {
        undoManager->changeLinePenStyle(line, oldPen, newPen);
    } else {
        line->setPen(newPen);
    }
    isModified = true;
    scene->update();
}

// 连接码类属性编辑器与选中项的信号槽
void MainWindow::connectCodeSignals()
{
    // 连接码类类型选项变更信号
    connect(codeTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateCodeType);

    // 连接码类数据编辑框的信号
    connect(codeDataEdit, &QLineEdit::editingFinished,
            this, &MainWindow::updateCodeData);

    // 连接码类尺寸调整器的信号
    connect(codeWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::updateCodeWidth);
    connect(codeHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::updateCodeHeight);

    // 添加并连接显示文本复选框（仅用于条形码）
    showBarcodeTextCheck = new QCheckBox(tr("显示文本"));
    QFormLayout* codeLayout = qobject_cast<QFormLayout*>(codeGroup->layout());
    if (codeLayout) {
        codeLayout->addRow(tr("选项:"), showBarcodeTextCheck);
    }

    connect(showBarcodeTextCheck, &QCheckBox::toggled,
            this, &MainWindow::updateBarcodeShowText);
}

// 更新码类类型
void MainWindow::updateCodeType(int index)
{
    // 获取选中的条形码项
    BarcodeItem* barcodeItem = getSelectedBarcodeItem();
    if (barcodeItem) {
        ZXing::BarcodeFormat format;
        // 条形码类型映射
        switch (index) {
            case 0: format = ZXing::BarcodeFormat::Code128; break;
            case 1: format = ZXing::BarcodeFormat::EAN13; break;
            case 2: format = ZXing::BarcodeFormat::UPCA; break;
            case 3: format = ZXing::BarcodeFormat::Code39; break;
            default: format = ZXing::BarcodeFormat::Code128; break;
        }
        
        barcodeItem->setFormat(format);
        
        // 条形码始终显示文本选项
        showBarcodeTextCheck->setVisible(true);
        
        scene->update();
        isModified = true;
        return;
    }
    
    // 获取选中的二维码项
    QRCodeItem* qrCodeItem = getSelectedQRCodeItem();
    if (qrCodeItem) {
        // 二维码类型映射（当前下拉框中的选项）
        QString codeType;
        switch (index) {
            case 0: codeType = "QR"; break;
            case 1: codeType = "PDF417"; break;
            case 2: codeType = "DataMatrix"; break;
            case 3: codeType = "Aztec"; break;
            default: codeType = "QR"; break;
        }
        
        // 设置二维码类型
        qrCodeItem->setCodeType(codeType);
        
        // 二维码不显示文本选项
        showBarcodeTextCheck->setVisible(false);
        scene->update();
        isModified = true;
    }
}

// 更新码类数据
void MainWindow::updateCodeData()
{
    QString newData = codeDataEdit->text();
    
    // 更新条形码数据
    BarcodeItem* barcodeItem = getSelectedBarcodeItem();
    if (barcodeItem) {
        QString oldData = barcodeItem->data();
        if (oldData != newData && undoManager) {
            undoManager->changeBarcodeData(barcodeItem, oldData, newData);
        } else {
            barcodeItem->setData(newData);
        }
        scene->update();
        return;
    }
    
    // 更新二维码数据
    QRCodeItem* qrCodeItem = getSelectedQRCodeItem();
    if (qrCodeItem) {
        QString oldData = qrCodeItem->text();
        if (oldData != newData && undoManager) {
            undoManager->changeQRCodeData(qrCodeItem, oldData, newData);
        } else {
            qrCodeItem->setText(newData);
        }
        scene->update();
    }
}

void MainWindow::applyCodeDataSource()
{
    if (!codeDatabaseWidget || !scene) {
        return;
    }

    const QList<QGraphicsItem*> selected = scene->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, tr("数据源"), tr("请先选择条码或二维码元素。"));
        return;
    }

    QGraphicsItem* item = selected.first();
    if (!dynamic_cast<BarcodeItem*>(item) && !dynamic_cast<QRCodeItem*>(item)) {
        QMessageBox::information(this, tr("数据源"), tr("当前选择的不是条码或二维码元素。"));
        return;
    }

    QString errorMessage;
    auto source = codeDatabaseWidget->createDataSource(&errorMessage);
    if (!source) {
        if (!errorMessage.isEmpty()) {
            QMessageBox::warning(this, tr("数据源无效"), errorMessage);
        }
        return;
    }

    DataSourceBinding binding;
    binding.source = source;
    binding.enabled = codeDatabaseWidget->isDataSourceEnabled() && source->isValid();

    m_itemDataSources.insert(item, binding);
    syncCodeDataSourceWidget(item);

    if (statusBar()) {
        statusBar()->showMessage(tr("条码数据源已更新"), 2000);
    }
    isModified = true;
}

void MainWindow::applyTextDataSource()
{
    if (!textDatabaseWidget || !scene) {
        return;
    }

    const QList<QGraphicsItem*> selected = scene->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, tr("数据源"), tr("请先选择文本元素。"));
        return;
    }

    QGraphicsItem* item = selected.first();
    if (!dynamic_cast<TextItem*>(item)) {
        QMessageBox::information(this, tr("数据源"), tr("当前选择的不是文本元素。"));
        return;
    }

    QString errorMessage;
    auto source = textDatabaseWidget->createDataSource(&errorMessage);
    if (!source) {
        if (!errorMessage.isEmpty()) {
            QMessageBox::warning(this, tr("数据源无效"), errorMessage);
        }
        return;
    }

    DataSourceBinding binding;
    binding.source = source;
    binding.enabled = textDatabaseWidget->isDataSourceEnabled() && source->isValid();

    m_itemDataSources.insert(item, binding);
    syncTextDataSourceWidget(item);

    if (statusBar()) {
        statusBar()->showMessage(tr("文本数据源已更新"), 2000);
    }
    isModified = true;
}

void MainWindow::handleCodeDataSourceEnabledChanged(bool enabled)
{
    if (!codeDatabaseWidget || !scene) {
        return;
    }

    const QList<QGraphicsItem*> selected = scene->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    QGraphicsItem* item = selected.first();
    if (!dynamic_cast<BarcodeItem*>(item) && !dynamic_cast<QRCodeItem*>(item)) {
        return;
    }

    auto it = m_itemDataSources.find(item);
    if (it == m_itemDataSources.end() || !it->source) {
        if (enabled) {
            QMessageBox::information(this, tr("数据源"), tr("请先配置数据源并点击“应用到当前元素”。"));
            codeDatabaseWidget->setDataSourceEnabled(false);
        }
        return;
    }

    if (enabled && !it->source->isValid()) {
        QMessageBox::warning(this, tr("数据源无效"), tr("当前数据源无效，请重新配置。"));
        codeDatabaseWidget->setDataSourceEnabled(false);
        return;
    }

    it->enabled = enabled;
    isModified = true;
}

void MainWindow::handleTextDataSourceEnabledChanged(bool enabled)
{
    if (!textDatabaseWidget || !scene) {
        return;
    }

    const QList<QGraphicsItem*> selected = scene->selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    QGraphicsItem* item = selected.first();
    if (!dynamic_cast<TextItem*>(item)) {
        return;
    }

    auto it = m_itemDataSources.find(item);
    if (it == m_itemDataSources.end() || !it->source) {
        if (enabled) {
            QMessageBox::information(this, tr("数据源"), tr("请先配置数据源并点击“应用到当前元素”。"));
            textDatabaseWidget->setDataSourceEnabled(false);
        }
        return;
    }

    if (enabled && !it->source->isValid()) {
        QMessageBox::warning(this, tr("数据源无效"), tr("当前数据源无效，请重新配置。"));
        textDatabaseWidget->setDataSourceEnabled(false);
        return;
    }

    it->enabled = enabled;
    isModified = true;
}

void MainWindow::onSceneItemRemoved(QGraphicsItem* item)
{
    clearDataSourceBinding(item);
}

void MainWindow::syncCodeDataSourceWidget(QGraphicsItem* item)
{
    if (!codeDatabaseWidget) {
        return;
    }

    if (!item) {
        codeDatabaseWidget->clearInputs();
        codeDatabaseWidget->setDataSourceEnabled(false);
        return;
    }

    const DataSourceBinding binding = m_itemDataSources.value(item);
    codeDatabaseWidget->setDataSource(binding.source);
    codeDatabaseWidget->setDataSourceEnabled(binding.enabled);
}

void MainWindow::syncTextDataSourceWidget(QGraphicsItem* item)
{
    if (!textDatabaseWidget) {
        return;
    }

    if (!item) {
        textDatabaseWidget->clearInputs();
        textDatabaseWidget->setDataSourceEnabled(false);
        return;
    }

    const DataSourceBinding binding = m_itemDataSources.value(item);
    textDatabaseWidget->setDataSource(binding.source);
    textDatabaseWidget->setDataSourceEnabled(binding.enabled);
}

void MainWindow::clearDataSourceBinding(QGraphicsItem* item)
{
    if (!item) {
        return;
    }
    m_itemDataSources.remove(item);
}

void MainWindow::initializePrinting()
{
    if (m_printEngine) {
        return;
    }

    auto renderer = std::make_unique<DefaultPrintRenderer>();
    m_printEngine = std::make_unique<PrintEngine>(std::move(renderer));
}

QList<labelelement*> MainWindow::collectElements(bool onlySelected) const
{
    QList<labelelement*> result;
    if (!scene) {
        return result;
    }

    QList<QGraphicsItem*> items;
    QGraphicsItem* selectionFrame = scene->selectionFrame();
    const QList<QGraphicsItem*> ordered = scene->items(Qt::AscendingOrder);
    if (onlySelected) {
        for (QGraphicsItem* candidate : ordered) {
            if (candidate && candidate != selectionFrame && candidate->isSelected()) {
                items.append(candidate);
            }
        }
    } else {
        items = ordered;
    }
    if (selectionFrame) {
        items.removeAll(selectionFrame);
    }
    m_elementCache.clear();
    m_elementCache.reserve(static_cast<size_t>(items.size()));

    for (QGraphicsItem* item : items) {
        if (!item || item == selectionFrame || item->data(0).toString() == "labelBackground") {
            continue;
        }

        auto element = wrapItem(item);
        if (!element) {
            continue;
        }

        bool dataSourceApplied = false;
        if (auto it = m_itemDataSources.constFind(item); it != m_itemDataSources.constEnd()) {
            const DataSourceBinding& binding = it.value();
            if (binding.source) {
                element->setDataSource(binding.source);
                element->setDataSourceEnabled(binding.enabled && binding.source->isValid());
                dataSourceApplied = true;
            } else {
                element->setDataSourceEnabled(false);
            }
        }

        if (!dataSourceApplied) {
            element->setDataSourceEnabled(false);
        }

        labelelement* raw = element.get();
        m_elementCache.emplace_back(std::move(element));
        result.append(raw);
    }

    return result;
}

PrintContext MainWindow::buildPrintContext(QPrinter *printer) const
{
    PrintContext context;
    context.printer = printer;
    context.sourceScene = scene;

    if (printer) {
        context.pageLayout = printer->pageLayout();
        context.contentMargins = context.pageLayout.margins(QPageLayout::Millimeter);
    }

    bool ok = false;
    double widthMM = widthLineEdit ? widthLineEdit->text().toDouble(&ok) : 0.0;
    if (!ok) {
        widthMM = DEFAULT_LABEL_WIDTH;
    }
    ok = false;
    double heightMM = heightLineEdit ? heightLineEdit->text().toDouble(&ok) : 0.0;
    if (!ok) {
        heightMM = DEFAULT_LABEL_HEIGHT;
    }

    context.labelSizeMM = QSizeF(widthMM, heightMM);
    context.labelSizePixels = QSizeF(widthMM * MM_TO_PIXELS, heightMM * MM_TO_PIXELS);

    double cornerPx = 0.0;
    if (labelCornerRadiusSpin) {
        cornerPx = static_cast<double>(labelCornerRadiusSpin->value());
    }
    const double maxCornerPx = std::min(context.labelSizePixels.width(), context.labelSizePixels.height()) / 2.0;
    if (maxCornerPx > 0.0) {
        cornerPx = std::clamp(cornerPx, 0.0, maxCornerPx);
    } else {
        cornerPx = 0.0;
    }
    context.labelCornerRadiusPixels = cornerPx;

    return context;
}

std::unique_ptr<labelelement> MainWindow::wrapItem(QGraphicsItem *item) const
{
    if (!item) {
        return nullptr;
    }

    if (auto barcode = dynamic_cast<BarcodeItem*>(item)) {
        return std::make_unique<BarcodeElement>(barcode);
    }
    if (auto qr = dynamic_cast<QRCodeItem*>(item)) {
        return std::make_unique<QRCodeElement>(qr);
    }
    if (auto text = dynamic_cast<TextItem*>(item)) {
        return std::make_unique<TextElement>(text);
    }
    if (auto image = dynamic_cast<ImageItem*>(item)) {
        return std::make_unique<ImageElement>(image);
    }
    if (auto line = dynamic_cast<LineItem*>(item)) {
        return std::make_unique<LineElement>(line);
    }
    if (auto rect = dynamic_cast<RectangleItem*>(item)) {
        return std::make_unique<RectangleElement>(rect);
    }
    if (auto circle = dynamic_cast<CircleItem*>(item)) {
        return std::make_unique<CircleElement>(circle);
    }
    if (auto star = dynamic_cast<StarItem*>(item)) {
        return std::make_unique<StarElement>(star);
    }
    if (auto arrow = dynamic_cast<ArrowItem*>(item)) {
        return std::make_unique<ArrowElement>(arrow);
    }
    if (auto poly = dynamic_cast<PolygonItem*>(item)) {
        return std::make_unique<PolygonElement>(poly);
    }

    return nullptr;
}

std::optional<bool> MainWindow::promptSelectionChoice(const QString &title,
                                                      const QString &message,
                                                      const QString &selectedLabel,
                                                      const QString &allLabel)
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(title);
    msgBox.setText(message);
    msgBox.setInformativeText(tr("请选择打印范围"));

    QPushButton *selectedButton = msgBox.addButton(selectedLabel, QMessageBox::ActionRole);
    QPushButton *allButton = msgBox.addButton(allLabel, QMessageBox::ActionRole);
    QPushButton *cancelButton = msgBox.addButton(tr("取消"), QMessageBox::RejectRole);

    msgBox.exec();

    if (msgBox.clickedButton() == cancelButton) {
        return std::nullopt;
    }

    return msgBox.clickedButton() == selectedButton;
}
// 更新码类宽度
void MainWindow::updateCodeWidth(int width)
{
    // 更新条形码宽度
    BarcodeItem* barcodeItem = getSelectedBarcodeItem();
    if (barcodeItem) {
        barcodeItem->setSize(QSizeF(width, barcodeItem->size().height()));
        scene->update();
        isModified = true;
        return;
    }
    
    // 更新二维码宽度
    QRCodeItem* qrCodeItem = getSelectedQRCodeItem();
    if (qrCodeItem) {
        qrCodeItem->setSize(QSizeF(width, qrCodeItem->size().height()));
        scene->update();
        isModified = true;
    }
}

// 更新码类高度
void MainWindow::updateCodeHeight(int height)
{
    // 更新条形码高度
    BarcodeItem* barcodeItem = getSelectedBarcodeItem();
    if (barcodeItem) {
        barcodeItem->setSize(QSizeF(barcodeItem->size().width(), height));
        scene->update();
        isModified = true;
        return;
    }
    
    // 更新二维码高度
    QRCodeItem* qrCodeItem = getSelectedQRCodeItem();
    if (qrCodeItem) {
        qrCodeItem->setSize(QSizeF(qrCodeItem->size().width(), height));
        scene->update();
        isModified = true;
    }
}

// 更新是否显示文本
void MainWindow::updateBarcodeShowText(bool showText)
{
    BarcodeItem* barcodeItem = getSelectedBarcodeItem();
    if (!barcodeItem) return;

    barcodeItem->setShowHumanReadableText(showText);
    scene->update();
    isModified = true;
}

// 获取选中的条形码项
BarcodeItem* MainWindow::getSelectedBarcodeItem()
{
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) return nullptr;

    return dynamic_cast<BarcodeItem*>(selectedItems.first());
}

// 获取选中的二维码项
QRCodeItem* MainWindow::getSelectedQRCodeItem()
{
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) return nullptr;

    return dynamic_cast<QRCodeItem*>(selectedItems.first());
}

void MainWindow::createRulers()
{
    // 创建标尺视图层
    horizontalRuler = new Ruler(Qt::Horizontal, view);
    verticalRuler = new Ruler(Qt::Vertical, view);

    cornerWidget = new QWidget(view);
    cornerWidget->setFixedSize(30, 30);
    cornerWidget->setStyleSheet("background-color: #F0F0F0; border: 1px solid gray;");
    cornerWidget->move(0, 0);
    cornerWidget->hide();

    // 设置标尺初始属性
    horizontalRuler->setVisible(false);
    verticalRuler->setVisible(false);

    // 配置标尺位置
    horizontalRuler->move(30, 0); // 从左边偏移30像素
    verticalRuler->move(0, 30);   // 从顶部偏移30像素

    // 连接视图变换信号
    view->viewport()->installEventFilter(this);
    connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::updateRulerPosition);
    connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this, &MainWindow::updateRulerPosition);

    // 视图缩放时更新标尺
    connect(this, &MainWindow::zoomFactorChanged, this, &MainWindow::updateRulerScale);
}

void MainWindow::updateRulerPosition()
{
    if (horizontalRuler && verticalRuler) {
        // 更新标尺位置和大小
        horizontalRuler->updatePositionAndSize();
        verticalRuler->updatePositionAndSize();

        // 强制重绘
        horizontalRuler->update();
        verticalRuler->update();
    }
}

void MainWindow::updateRulerScale(double zoomFactor)
{
    Q_UNUSED(zoomFactor);

    if (!horizontalRuler || !verticalRuler) {
        return;
    }

    const double rulerScale = MM_TO_PIXELS;
    horizontalRuler->setScale(rulerScale);
    verticalRuler->setScale(rulerScale);

    horizontalRuler->update();
    verticalRuler->update();
}

void MainWindow::setZoomFactor(double factor)
{
    const double clamped = qBound(MIN_ZOOM / 100.0, factor, MAX_ZOOM / 100.0);
    const bool changed = !qFuzzyCompare(1.0 + zoomFactor, 1.0 + clamped);
    zoomFactor = clamped;

    QTransform transform;
    transform.scale(zoomFactor, zoomFactor);
    view->setTransform(transform);

    if (changed) {
        emit zoomFactorChanged(zoomFactor);
    } else {
        updateRulerScale(zoomFactor);
    }

    updateRulerPosition();
    zoomLabel->setText(tr("缩放: %1%").arg(int(std::round(zoomFactor * 100))));
}

void MainWindow::onRubberBandChanged(const QRect &viewportRect,
                                     const QPointF &fromScenePoint,
                                     const QPointF &toScenePoint)
{
    if (!scene || !view) {
        return;
    }

    const bool rubberBandVisible = viewportRect.width() > 0 && viewportRect.height() > 0;
    QRectF selectionRect(fromScenePoint, toScenePoint);
    selectionRect = selectionRect.normalized();

    if (rubberBandVisible) {
        if (!m_isRubberBandSelecting) {
            m_isRubberBandSelecting = true;
        }

        if (selectionRect.width() < 1.0 && selectionRect.height() < 1.0) {
            return;
        }

        handleRubberBandSelection(selectionRect, false);
        return;
    }

    if (!m_isRubberBandSelecting) {
        return;
    }

    m_isRubberBandSelecting = false;

    if (selectionRect.width() < 1.0 && selectionRect.height() < 1.0) {
        return;
    }

    handleRubberBandSelection(selectionRect, true);
}

void MainWindow::handleRubberBandSelection(const QRectF &selectionRect, bool finalize)
{
    if (!scene) {
        return;
    }

    if (selectionRect.isNull()) {
        return;
    }

    const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();

    if ((modifiers & Qt::ControlModifier) && !finalize) {
        return;
    }

    std::unique_ptr<QSignalBlocker> blocker;
    if (!finalize) {
        blocker = std::make_unique<QSignalBlocker>(scene);
    }

    const QList<QGraphicsItem*> itemsInRect = scene->items(selectionRect, Qt::IntersectsItemShape);

    if (!(modifiers & (Qt::ControlModifier | Qt::ShiftModifier))) {
        scene->clearSelection();
    }

    for (QGraphicsItem* item : itemsInRect) {
        if (!item) {
            continue;
        }
        if (!(item->flags() & QGraphicsItem::ItemIsSelectable)) {
            continue;
        }
        if (item->data(0).toString() == QStringLiteral("labelBackground")) {
            continue;
        }
        if (qgraphicsitem_cast<QGraphicsProxyWidget*>(item)) {
            continue;
        }

        if (modifiers & Qt::ControlModifier) {
            if (finalize) {
                item->setSelected(!item->isSelected());
            }
        } else {
            item->setSelected(true);
        }
    }

    if (finalize) {
        scene->update();
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == view->viewport() &&
       (event->type() == QEvent::Resize || event->type() == QEvent::Paint)) {
        updateRulerPosition();
       }
    
    // 处理粘贴预览模式下的鼠标事件
    if (watched == view->viewport() && m_pasteModeActive) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QPointF scenePos = view->mapToScene(mouseEvent->pos());
            updatePreviewPosition(scenePos);
            return true;
        }
        else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QPointF scenePos = view->mapToScene(mouseEvent->pos());
                confirmPaste(scenePos);
                return true;
            }
            else if (mouseEvent->button() == Qt::RightButton) {
                // 右键取消粘贴
                endPastePreview();
                return true;
            }
        }
    }
    
    // 处理键盘事件（ESC取消粘贴）
    if (m_pasteModeActive && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            endPastePreview();
            return true;
        }
    }
    
    // 处理视图中的鼠标事件
    if (watched == view->viewport()) {
        // 处理鼠标按下事件
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            // 如果按下的是左键且同时按下了Ctrl键
            if (mouseEvent->button() == Qt::LeftButton &&
                mouseEvent->modifiers() & Qt::ControlModifier) {
                // 记录当前鼠标位置
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                lastMousePos = mouseEvent->position();
#else
                lastMousePos = mouseEvent->pos();
#endif
                return true; // 事件已处理
            }
        }
        // 处理鼠标移动事件
        else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            // 如果鼠标左键被按下且同时按下了Ctrl键
            if (mouseEvent->buttons() & Qt::LeftButton &&
                mouseEvent->modifiers() & Qt::ControlModifier) {
                // 计算鼠标Y方向的移动距离
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                qreal delta = mouseEvent->position().y() - lastMousePos.y();
#else
                qreal delta = mouseEvent->pos().y() - lastMousePos.y();
#endif
                // 根据移动距离调整缩放比例
                double newZoomFactor = zoomFactor;
                if (delta > 0) {
                    // 向下移动，缩小
                    newZoomFactor = qMax(zoomFactor - delta/100, MIN_ZOOM/100.0);
                } else {
                    // 向上移动，放大
                    newZoomFactor = qMin(zoomFactor - delta/100, MAX_ZOOM/100.0);
                }

                if (newZoomFactor != zoomFactor) {
                    // 应用新的缩放比例
                    setZoomFactor(newZoomFactor);
                }

                // 更新上次鼠标位置
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                lastMousePos = mouseEvent->position();
#else
                lastMousePos = mouseEvent->pos();
#endif
                return true; // 事件已处理
            }

            // 更新标尺参考线：当按住左键拖动已选中的单个项目时，显示其上下/左右边对应的虚线
            if (horizontalRuler && verticalRuler) {
                if (mouseEvent->buttons() & Qt::LeftButton) {
                    QList<QGraphicsItem*> sel = scene->selectedItems();
                    if (sel.size() == 1) {
                        QGraphicsItem* item = sel.first();
                        if (item && item->data(0).toString() != "labelBackground" && !qgraphicsitem_cast<QGraphicsProxyWidget*>(item)) {
                            // 优先使用 AlignableItem 提供的“内容矩形”的场景坐标，确保与视觉内容边缘一致
                            if (auto alignable = dynamic_cast<AlignableItem*>(item)) {
                                QRectF rect = alignable->alignmentSceneRect();
                                horizontalRuler->setGuidesFromSceneRect(rect);
                                verticalRuler->setGuidesFromSceneRect(rect);
                            } else {
                                // 回退：无对齐接口时使用 sceneBoundingRect
                                QRectF rect = item->sceneBoundingRect();
                                horizontalRuler->setGuidesFromSceneRect(rect);
                                verticalRuler->setGuidesFromSceneRect(rect);
                            }
                        }
                    }
                } else {
                    // 未按下左键时清除参考线
                    horizontalRuler->clearGuides();
                    verticalRuler->clearGuides();
                }
            }
        }
        // 鼠标释放时清除标尺参考线
        else if (event->type() == QEvent::MouseButtonRelease) {
            if (horizontalRuler && verticalRuler) {
                horizontalRuler->clearGuides();
                verticalRuler->clearGuides();
            }
        }
        // 处理鼠标滚轮事件 - 与Ctrl组合使用时缩放
        else if (event->type() == QEvent::Wheel) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            if (wheelEvent->modifiers() & Qt::ControlModifier) {
                QPoint numDegrees = wheelEvent->angleDelta() / 8;
                double factor = zoomFactor;

                // 计算缩放因子
                if (!numDegrees.isNull()) {
                    double step = numDegrees.y() / 150.0; // 调整缩放速度
                    factor = qBound(MIN_ZOOM/100.0, zoomFactor * (1.0 + step), MAX_ZOOM/100.0);
                }

                if (factor != zoomFactor) {
                    // 保存鼠标位置，以便在该位置进行缩放
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                    QPointF scenePos = view->mapToScene(wheelEvent->position().toPoint());
#else
                    QPointF scenePos = view->mapToScene(wheelEvent->pos());
#endif

                    // 应用缩放
                    setZoomFactorAtPosition(factor, scenePos);
                }

                return true; // 事件已处理
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // 处理粘贴预览模式下的ESC键
    if (m_pasteModeActive && event->key() == Qt::Key_Escape) {
        endPastePreview();
        event->accept();
        return;
    }
    
    // 调用父类的处理
    QMainWindow::keyPressEvent(event);
}

void MainWindow::setZoomFactorAtPosition(double factor, const QPointF &scenePos)
{
    // 保存原来的场景坐标
    QPointF viewPos = view->mapFromScene(scenePos);

    const double previousZoom = zoomFactor;
    setZoomFactor(factor);

    if (!qFuzzyCompare(1.0 + previousZoom, 1.0 + zoomFactor)) {
        QPointF newViewPos = view->mapFromScene(scenePos);
        QPointF delta = newViewPos - viewPos;
        view->horizontalScrollBar()->setValue(view->horizontalScrollBar()->value() + delta.x());
        view->verticalScrollBar()->setValue(view->verticalScrollBar()->value() + delta.y());
    }
}

// 右键菜单槽函数实现
void MainWindow::copySelectedItem()
{
    qDebug() << "=== copySelectedItem called ===";

    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        qDebug() << "No selected items";
        return;
    }

    QGraphicsItem* item = selectedItems.first();
    qDebug() << "Selected item type:" << typeid(*item).name();

    // 复制条形码项
    BarcodeItem* barcodeItem = dynamic_cast<BarcodeItem*>(item);
    if (barcodeItem) {
        qDebug() << "Copying BarcodeItem";
        // 清除其他类型的复制状态
        QRCodeItem::s_hasCopiedItem = false;
        TextItem::s_hasCopiedItem = false;
        ImageItem::s_hasCopiedItem = false;
        LineItem::s_hasCopiedItem = false;
        RectangleItem::s_hasCopiedItem = false;
        CircleItem::s_hasCopiedItem = false;

        barcodeItem->copyItem(barcodeItem);
        // 更新按钮状态
        updateContextMenuActions();
        return;
    }

    // 复制二维码项
    QRCodeItem* qrCodeItem = dynamic_cast<QRCodeItem*>(item);
    if (qrCodeItem) {
        qDebug() << "Copying QRCodeItem";
        // 清除其他类型的复制状态
        BarcodeItem::s_hasCopiedItem = false;
        TextItem::s_hasCopiedItem = false;
        ImageItem::s_hasCopiedItem = false;
        LineItem::s_hasCopiedItem = false;
        RectangleItem::s_hasCopiedItem = false;
        CircleItem::s_hasCopiedItem = false;

        qrCodeItem->copyItem(qrCodeItem);
        // 更新按钮状态
        updateContextMenuActions();
        return;
    }

    // 复制文本项
    TextItem* textItem = dynamic_cast<TextItem*>(item);
    if (textItem) {
        qDebug() << "Copying TextItem";
        // 清除其他类型的复制状态
        QRCodeItem::s_hasCopiedItem = false;
        BarcodeItem::s_hasCopiedItem = false;
        ImageItem::s_hasCopiedItem = false;
        LineItem::s_hasCopiedItem = false;
        RectangleItem::s_hasCopiedItem = false;
        CircleItem::s_hasCopiedItem = false;

        textItem->copyItem(textItem);
        // 更新按钮状态
        updateContextMenuActions();
        return;
    }

    // 复制图像项
    ImageItem* imageItem = dynamic_cast<ImageItem*>(item);
    if (imageItem) {
        qDebug() << "Copying ImageItem";
        // 清除其他类型的复制状态
        QRCodeItem::s_hasCopiedItem = false;
        BarcodeItem::s_hasCopiedItem = false;
        TextItem::s_hasCopiedItem = false;
        LineItem::s_hasCopiedItem = false;
        RectangleItem::s_hasCopiedItem = false;
        CircleItem::s_hasCopiedItem = false;

        imageItem->copyItem(imageItem);
        // 更新按钮状态
        updateContextMenuActions();
        return;
    }

    // 复制线条项
    LineItem* lineItem = dynamic_cast<LineItem*>(item);
    if (lineItem) {
        qDebug() << "Copying LineItem";
        // 清除其他类型的复制状态
        QRCodeItem::s_hasCopiedItem = false;
        BarcodeItem::s_hasCopiedItem = false;
        TextItem::s_hasCopiedItem = false;
        ImageItem::s_hasCopiedItem = false;
        RectangleItem::s_hasCopiedItem = false;
        CircleItem::s_hasCopiedItem = false;
        StarItem::s_hasCopiedItem = false;
        ArrowItem::s_hasCopiedItem = false;
        PolygonItem::s_hasCopiedItem = false;

        lineItem->copyItem(lineItem);
        // 更新按钮状态
        updateContextMenuActions();
        return;
    }

    // 复制矩形项
    RectangleItem* rectangleItem = dynamic_cast<RectangleItem*>(item);
    if (rectangleItem) {
        qDebug() << "Copying RectangleItem";
        // 清除其他类型的复制状态
        QRCodeItem::s_hasCopiedItem = false;
        BarcodeItem::s_hasCopiedItem = false;
        TextItem::s_hasCopiedItem = false;
        ImageItem::s_hasCopiedItem = false;
        LineItem::s_hasCopiedItem = false;
        CircleItem::s_hasCopiedItem = false;
        StarItem::s_hasCopiedItem = false;
        ArrowItem::s_hasCopiedItem = false;
        PolygonItem::s_hasCopiedItem = false;

        rectangleItem->copyItem(rectangleItem);
        // 更新按钮状态
        updateContextMenuActions();
        return;
    }

    // 复制圆形项
    CircleItem* circleItem = dynamic_cast<CircleItem*>(item);
    if (circleItem) {
        qDebug() << "Copying CircleItem";
        // 清除其他类型的复制状态
        QRCodeItem::s_hasCopiedItem = false;
        BarcodeItem::s_hasCopiedItem = false;
        TextItem::s_hasCopiedItem = false;
        ImageItem::s_hasCopiedItem = false;
        LineItem::s_hasCopiedItem = false;
        RectangleItem::s_hasCopiedItem = false;
        StarItem::s_hasCopiedItem = false;
        ArrowItem::s_hasCopiedItem = false;
        PolygonItem::s_hasCopiedItem = false;

        circleItem->copyItem(circleItem);
        // 更新按钮状态
        updateContextMenuActions();
        return;
    }

    // 复制星形项
    if (auto starItem = dynamic_cast<StarItem*>(item)) {
        qDebug() << "Copying StarItem";
        QRCodeItem::s_hasCopiedItem = false;
        BarcodeItem::s_hasCopiedItem = false;
        TextItem::s_hasCopiedItem = false;
        ImageItem::s_hasCopiedItem = false;
        LineItem::s_hasCopiedItem = false;
        RectangleItem::s_hasCopiedItem = false;
        CircleItem::s_hasCopiedItem = false;
        ArrowItem::s_hasCopiedItem = false;
        PolygonItem::s_hasCopiedItem = false;
        starItem->copyItem(starItem);
        updateContextMenuActions();
        return;
    }

    // 复制箭头项
    if (auto arrowItem = dynamic_cast<ArrowItem*>(item)) {
        qDebug() << "Copying ArrowItem";
        QRCodeItem::s_hasCopiedItem = false;
        BarcodeItem::s_hasCopiedItem = false;
        TextItem::s_hasCopiedItem = false;
        ImageItem::s_hasCopiedItem = false;
        LineItem::s_hasCopiedItem = false;
        RectangleItem::s_hasCopiedItem = false;
        CircleItem::s_hasCopiedItem = false;
        StarItem::s_hasCopiedItem = false;
        PolygonItem::s_hasCopiedItem = false;
        arrowItem->copyItem(arrowItem);
        updateContextMenuActions();
        return;
    }

    // 复制多边形项
    if (auto polyItem = dynamic_cast<PolygonItem*>(item)) {
        qDebug() << "Copying PolygonItem";
        QRCodeItem::s_hasCopiedItem = false;
        BarcodeItem::s_hasCopiedItem = false;
        TextItem::s_hasCopiedItem = false;
        ImageItem::s_hasCopiedItem = false;
        LineItem::s_hasCopiedItem = false;
        RectangleItem::s_hasCopiedItem = false;
        CircleItem::s_hasCopiedItem = false;
        StarItem::s_hasCopiedItem = false;
        ArrowItem::s_hasCopiedItem = false;
        polyItem->copyItem(polyItem);
        updateContextMenuActions();
        return;
    }

    qDebug() << "Unknown item type, cannot copy";
}

void MainWindow::deleteSelectedItem()
{
    if (!scene) {
        return;
    }
    
    // 使用场景的删除方法，这样会自动使用撤销管理器
    scene->removeSelectedItems();
    isModified = true;
}

void MainWindow::rotateSelectedItem()
{
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    QGraphicsItem* item = selectedItems.first();
    
    // 设置变换原点为项目中心，使旋转时位置保持不变
    QRectF boundingRect = item->boundingRect();
    QPointF center = boundingRect.center();
    item->setTransformOriginPoint(center);
    
    // 获取当前旋转角度并增加90度
    qreal currentRotation = item->rotation();
    item->setRotation(currentRotation + 90);
    
    isModified = true;
}

void MainWindow::bringSelectedItemToFront()
{
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    QGraphicsItem* item = selectedItems.first();
    scene->bringToFront(item);
    
    isModified = true;
}

void MainWindow::sendSelectedItemToBack()
{
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    QGraphicsItem* item = selectedItems.first();
    scene->sendToBack(item);
    
    isModified = true;
}

void MainWindow::bringSelectedItemForward()
{
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    QGraphicsItem* item = selectedItems.first();
    
    // 获取所有项目并找到当前项的Z值
    QList<QGraphicsItem*> allItems = scene->items();
    qreal currentZ = item->zValue();
    qreal nextZ = currentZ + 1;
    
    // 找到下一个更高的Z值
    for (QGraphicsItem* otherItem : allItems) {
        if (otherItem != item && otherItem->zValue() > currentZ) {
            nextZ = qMin(nextZ, otherItem->zValue() + 0.1);
        }
    }
    
    item->setZValue(nextZ);
    isModified = true;
}

void MainWindow::sendSelectedItemBackward()
{
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    QGraphicsItem* item = selectedItems.first();
    
    // 获取所有项目并找到当前项的Z值
    QList<QGraphicsItem*> allItems = scene->items();
    qreal currentZ = item->zValue();
    qreal prevZ = currentZ - 1;
    
    // 找到下一个更低的Z值
    for (QGraphicsItem* otherItem : allItems) {
        if (otherItem != item && otherItem->zValue() < currentZ) {
            prevZ = qMax(prevZ, otherItem->zValue() - 0.1);
        }
    }
    
    item->setZValue(prevZ);
    isModified = true;
}

void MainWindow::updateContextMenuActions()
{
    bool hasSelection = !scene->selectedItems().isEmpty();
    bool hasSingleSelection = scene->selectedItems().size() == 1;

    qDebug() << "=== updateContextMenuActions ===";
    qDebug() << "hasSelection:" << hasSelection << "hasSingleSelection:" << hasSingleSelection;
    qDebug() << "QRCodeItem::s_hasCopiedItem:" << QRCodeItem::s_hasCopiedItem;
    qDebug() << "BarcodeItem::s_hasCopiedItem:" << BarcodeItem::s_hasCopiedItem;
    qDebug() << "TextItem::s_hasCopiedItem:" << TextItem::s_hasCopiedItem;
    qDebug() << "ImageItem::s_hasCopiedItem:" << ImageItem::s_hasCopiedItem;
    qDebug() << "LineItem::s_hasCopiedItem:" << LineItem::s_hasCopiedItem;
    qDebug() << "RectangleItem::s_hasCopiedItem:" << RectangleItem::s_hasCopiedItem;
    qDebug() << "CircleItem::s_hasCopiedItem:" << CircleItem::s_hasCopiedItem;
    qDebug() << "StarItem::s_hasCopiedItem:" << StarItem::s_hasCopiedItem;
    qDebug() << "ArrowItem::s_hasCopiedItem:" << ArrowItem::s_hasCopiedItem;
    qDebug() << "PolygonItem::s_hasCopiedItem:" << PolygonItem::s_hasCopiedItem;
    qDebug() << "StarItem::s_hasCopiedItem:" << StarItem::s_hasCopiedItem;
    qDebug() << "ArrowItem::s_hasCopiedItem:" << ArrowItem::s_hasCopiedItem;
    qDebug() << "PolygonItem::s_hasCopiedItem:" << PolygonItem::s_hasCopiedItem;

    // 启用/禁用右键菜单动作
    copyItemAction->setEnabled(hasSingleSelection);
    deleteItemAction->setEnabled(hasSelection);
    rotateItemAction->setEnabled(hasSingleSelection);
    bringToFrontAction->setEnabled(hasSingleSelection);
    sendToBackAction->setEnabled(hasSingleSelection);
    bringForwardAction->setEnabled(hasSingleSelection);
    sendBackwardAction->setEnabled(hasSingleSelection);
      // 更新编辑菜单中的复制和剪切动作
    copyAction->setEnabled(hasSingleSelection);
    cutAction->setEnabled(hasSelection);

    // 更新粘贴动作 - 检查是否有可粘贴的内容
    bool canPaste = QRCodeItem::s_hasCopiedItem || BarcodeItem::s_hasCopiedItem || TextItem::s_hasCopiedItem || ImageItem::s_hasCopiedItem ||
                   LineItem::s_hasCopiedItem || RectangleItem::s_hasCopiedItem || CircleItem::s_hasCopiedItem || StarItem::s_hasCopiedItem ||
                   ArrowItem::s_hasCopiedItem || PolygonItem::s_hasCopiedItem;
    pasteAction->setEnabled(canPaste);

    qDebug() << "pasteAction enabled:" << canPaste;

    // 更新对齐按钮 - 需要有选择的项目才能进行对齐
    alignLeftAction->setEnabled(hasSelection);
    alignCenterAction->setEnabled(hasSelection);
    alignRightAction->setEnabled(hasSelection);
}

// ========== 文本相关方法实现 ==========

void MainWindow::connectTextSignals()
{
    // 连接文本内容编辑框的信号
    connect(textContentEdit, &QLineEdit::editingFinished,
            this, &MainWindow::updateTextContent);

    // 只连接属性面板的字体相关信号，工具栏的信号通过同步来处理
    if (propertiesFontComboBox) {
        connect(propertiesFontComboBox, &QFontComboBox::currentFontChanged,
                this, &MainWindow::updateTextFont);
    }
    
    if (propertiesFontSizeSpinBox) {
        connect(propertiesFontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &MainWindow::updateTextSize);
    }
    
    // 连接工具栏字体组件到属性面板组件的同步（单向）
    if (fontComboBox && propertiesFontComboBox) {
        connect(fontComboBox, &QFontComboBox::currentFontChanged,
                this, [this](const QFont &font) {
            QSignalBlocker blocker(propertiesFontComboBox);
            propertiesFontComboBox->setCurrentFont(font);
            // 触发属性面板的更新
            updateTextFont();
        });
    }
    
    if (fontSizeSpinBox && propertiesFontSizeSpinBox) {
        connect(fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int size) {
            QSignalBlocker blocker(propertiesFontSizeSpinBox);
            propertiesFontSizeSpinBox->setValue(size);
            // 触发属性面板的更新
            updateTextSize(size);
        });
    }

    // 连接颜色按钮信号
    connect(textColorButton, &QPushButton::clicked,
            this, &MainWindow::updateTextColor);
    connect(textBackgroundColorButton, &QPushButton::clicked,
            this, &MainWindow::updateTextBackgroundColor);

    // 连接对齐方式信号
    connect(textAlignmentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateTextAlignment);

    // 连接文本选项信号
    connect(wordWrapCheck, &QCheckBox::toggled,
            this, &MainWindow::updateTextWordWrap);
    connect(letterSpacingSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::updateTextLetterSpacing);
    connect(autoResizeCheck, &QCheckBox::toggled,
            this, &MainWindow::updateTextAutoResize);
    connect(backgroundEnabledCheck, &QCheckBox::toggled,
            this, &MainWindow::updateTextBackgroundEnabled);

    // 连接边框相关信号
    connect(borderEnabledCheck, &QCheckBox::toggled,
            this, &MainWindow::updateTextBorder);
    connect(borderWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::updateTextBorder);
    connect(textBorderColorButton, &QPushButton::clicked,
            this, &MainWindow::updateTextBorder);
}

void MainWindow::updateTextContent()
{
    TextItem* textItem = getSelectedTextItem();
    if (textItem) {
        textItem->setText(textContentEdit->text());
        // 更新属性面板中的文本内容
        if (textContentEdit) {
            QSignalBlocker blocker(textContentEdit);
            textContentEdit->setText(textContentEdit->text());
        }
        isModified = true;
    }
}

void MainWindow::updateTextFont()
{
    TextItem* textItem = getSelectedTextItem();
    if (textItem) {
        // 直接使用属性面板的字体组件
        QFont font = propertiesFontComboBox->currentFont();
        font.setPointSize(propertiesFontSizeSpinBox->value());
        
        textItem->setFont(font);
        
        // 同步工具栏的字体显示（单向，不触发信号）
        if (fontComboBox) {
            QSignalBlocker blocker1(fontComboBox);
            fontComboBox->setCurrentFont(font);
        }
        if (fontSizeSpinBox) {
            QSignalBlocker blocker2(fontSizeSpinBox);
            fontSizeSpinBox->setValue(font.pointSize());
        }
        
        isModified = true;
    }
}

void MainWindow::updateTextSize(int size)
{
    TextItem* textItem = getSelectedTextItem();
    if (textItem) {
        QFont font = textItem->font();
        font.setPointSize(size);
        textItem->setFont(font);
        
        // 同步工具栏的字体大小显示（单向，不触发信号）
        if (fontSizeSpinBox) {
            QSignalBlocker blocker(fontSizeSpinBox);
            fontSizeSpinBox->setValue(size);
        }
        
        isModified = true;
    }
}

void MainWindow::updateTextColor()
{
    TextItem* textItem = getSelectedTextItem();
    if (textItem) {
        QColorDialog dialog(textItem->textColor(), this);
        dialog.setWindowTitle(tr("选择文本颜色"));
        dialog.setOptions(QColorDialog::DontUseNativeDialog); // 强制使用Qt自己的对话框
        
        if (dialog.exec() == QDialog::Accepted) {
            QColor color = dialog.currentColor();
            if (color.isValid()) {
                textItem->setTextColor(color);
                // 更新按钮颜色显示
                QString buttonStyle = QString("QPushButton { background-color: %1; border: 1px solid #999; }")
                                    .arg(color.name());
                textColorButton->setStyleSheet(buttonStyle);
                isModified = true;
            }
        }
    }
}

void MainWindow::updateTextBackgroundColor()
{
    TextItem* textItem = getSelectedTextItem();
    if (textItem) {
        QColorDialog dialog(textItem->backgroundColor(), this);
        dialog.setWindowTitle(tr("选择背景颜色"));
        dialog.setOptions(QColorDialog::DontUseNativeDialog); // 强制使用Qt自己的对话框
        
        if (dialog.exec() == QDialog::Accepted) {
            QColor color = dialog.currentColor();
            if (color.isValid()) {
                textItem->setBackgroundColor(color);
                // 更新按钮颜色显示
                QString buttonStyle = QString("QPushButton { background-color: %1; border: 1px solid #999; }")
                                    .arg(color.name());
                textBackgroundColorButton->setStyleSheet(buttonStyle);
                isModified = true;
            }
        }
    }
}

void MainWindow::updateTextAlignment()
{
    TextItem* textItem = getSelectedTextItem();
    if (textItem) {
        Qt::Alignment alignment = Qt::AlignLeft;
        switch (textAlignmentCombo->currentIndex()) {
            case 0: alignment = Qt::AlignLeft; break;
            case 1: alignment = Qt::AlignCenter; break;
            case 2: alignment = Qt::AlignRight; break;
        }
        textItem->setAlignment(alignment);
        isModified = true;
    }
}

void MainWindow::updateTextBorder()
{
    TextItem* textItem = getSelectedTextItem();
    if (textItem) {
        // 更新边框启用状态
        textItem->setBorderEnabled(borderEnabledCheck->isChecked());
        
        // 更新边框宽度
        textItem->setBorderWidth(borderWidthSpin->value());
        
        // 如果是颜色按钮触发的，则打开颜色对话框
        QPushButton* button = qobject_cast<QPushButton*>(sender());
        if (button == textBorderColorButton) {
            QColorDialog dialog(textItem->borderColor(), this);
            dialog.setWindowTitle(tr("选择边框颜色"));
            dialog.setOptions(QColorDialog::DontUseNativeDialog); // 强制使用Qt自己的对话框
            
            if (dialog.exec() == QDialog::Accepted) {
                QColor color = dialog.currentColor();
                if (color.isValid()) {
                    textItem->setBorderColor(color);
                    // 更新按钮颜色显示
                    QString buttonStyle = QString("QPushButton { background-color: %1; border: 1px solid #999; }")
                                        .arg(color.name());
                    textBorderColorButton->setStyleSheet(buttonStyle);
                }
            }
        }
        
        isModified = true;
    }
}

void MainWindow::updateTextWordWrap(bool wrap)
{
    TextItem* textItem = getSelectedTextItem();
    if (textItem) {
        textItem->setWordWrap(wrap);
        isModified = true;
    }
}

void MainWindow::updateTextAutoResize(bool autoResize)
{
    TextItem* textItem = getSelectedTextItem();
    if (textItem) {
        textItem->setAutoResize(autoResize);
        isModified = true;
    }
}

void MainWindow::updateTextBackgroundEnabled(bool enabled)
{
    TextItem* textItem = getSelectedTextItem();
    if (textItem) {
        textItem->setBackgroundEnabled(enabled);
        isModified = true;
    }
}

TextItem* MainWindow::getSelectedTextItem()
{
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        return nullptr;
    }
    
    return dynamic_cast<TextItem*>(selectedItems.first());
}

void MainWindow::startTextEditing(TextItem* textItem)
{
    if (textItem && textEditor) {
        // 记录编辑前的原始文本
        m_originalTextMap[textItem] = textItem->text();
        textEditor->startEditing(textItem, scene);
    }
}

void MainWindow::onTextEditingFinished(TextItem* textItem, const QString& newText)
{
    if (textItem) {
        // 获取原始文本
        QString originalText = m_originalTextMap.value(textItem, textItem->text());
        
        // 如果文本真的发生了变化，使用撤销管理器记录
        if (originalText != newText && undoManager) {
            undoManager->changeText(textItem, originalText, newText);
        } else {
            // 如果没有撤销管理器或文本没有变化，直接设置文本
            textItem->setText(newText);
        }
        
        // 更新属性面板中的文本内容
        if (textContentEdit) {
            QSignalBlocker blocker(textContentEdit);
            textContentEdit->setText(newText);
        }
        
        // 清理原始文本记录
        m_originalTextMap.remove(textItem);
    }
}

void MainWindow::onTextEditingCancelled(TextItem* textItem)
{
    // 编辑取消时清理原始文本记录
    if (textItem) {
        m_originalTextMap.remove(textItem);
    }
}

// ========== 图像相关方法实现 ==========

void MainWindow::connectImageSignals()
{
    // 连接加载图像按钮的信号
    connect(loadImageButton, &QPushButton::clicked,
            this, &MainWindow::loadImageFile);

    // 连接图像尺寸调整器的信号
    connect(imageWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::updateImageWidth);
    connect(imageHeightSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::updateImageHeight);

    // 连接保持宽高比复选框的信号
    connect(keepAspectRatioCheck, &QCheckBox::toggled,
            this, &MainWindow::updateImageKeepAspectRatio);

    // 连接透明度调整器的信号
    connect(imageOpacitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::updateImageOpacity);
}

void MainWindow::loadImageFile()
{
    ImageItem* imageItem = getSelectedImageItem();
    if (!imageItem) return;

    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("选择图像文件"),
        QString(),
        tr("图像文件 (*.png *.jpg *.jpeg *.gif *.bmp *.svg)")
    );

    if (!fileName.isEmpty()) {
        if (imageItem->loadImage(fileName)) {
            // 更新属性面板中的尺寸显示
            QSignalBlocker widthBlocker(imageWidthSpin);
            QSignalBlocker heightBlocker(imageHeightSpin);

            imageWidthSpin->setValue(static_cast<int>(imageItem->size().width()));
            imageHeightSpin->setValue(static_cast<int>(imageItem->size().height()));

            isModified = true;
        } else {
            QMessageBox::warning(this, tr("错误"), tr("无法加载图像文件"));
        }
    }
}

void MainWindow::updateImageWidth(int width)
{
    ImageItem* imageItem = getSelectedImageItem();
    if (imageItem) {
        QSizeF newSize(width, imageItem->size().height());
        imageItem->setSize(newSize);

        // 如果保持宽高比，更新高度显示
        if (imageItem->keepAspectRatio()) {
            QSignalBlocker blocker(imageHeightSpin);
            imageHeightSpin->setValue(static_cast<int>(imageItem->size().height()));
        }

        isModified = true;
    }
}

void MainWindow::updateImageHeight(int height)
{
    ImageItem* imageItem = getSelectedImageItem();
    if (imageItem) {
        QSizeF newSize(imageItem->size().width(), height);
        imageItem->setSize(newSize);

        // 如果保持宽高比，更新宽度显示
        if (imageItem->keepAspectRatio()) {
            QSignalBlocker blocker(imageWidthSpin);
            imageWidthSpin->setValue(static_cast<int>(imageItem->size().width()));
        }

        isModified = true;
    }
}

void MainWindow::updateImageKeepAspectRatio(bool keep)
{
    ImageItem* imageItem = getSelectedImageItem();
    if (imageItem) {
        imageItem->setKeepAspectRatio(keep);
        isModified = true;
    }
}

void MainWindow::updateImageOpacity(int opacity)
{
    ImageItem* imageItem = getSelectedImageItem();
    if (imageItem) {
        imageItem->setOpacity(opacity / 100.0); // 转换为0.0-1.0范围
        isModified = true;
    }
}

ImageItem* MainWindow::getSelectedImageItem()
{
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        return nullptr;
    }

    return dynamic_cast<ImageItem*>(selectedItems.first());
}

// ========== 绘图工具相关方法实现 ==========

void MainWindow::connectDrawingSignals()
{
    // 连接线条宽度调整器的信号
    connect(lineWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::updateLineWidth);

    // 连接颜色按钮的信号
    connect(cornerRadiusSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::updateCornerRadius);
    connect(lineColorButton, &QPushButton::clicked,
            this, &MainWindow::updateLineColor);
    connect(fillColorButton, &QPushButton::clicked,
            this, &MainWindow::updateFillColor);

    // 连接复选框的信号
    connect(fillEnabledCheck, &QCheckBox::toggled,
            this, &MainWindow::updateFillEnabled);
    connect(keepCircleCheck, &QCheckBox::toggled,
            this, &MainWindow::updateKeepCircle);

    if (lineAngleSpin) {
        connect(lineAngleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MainWindow::updateLineAngle);
    }
    if (lineDashedCheck) {
        connect(lineDashedCheck, &QCheckBox::toggled, this, &MainWindow::updateLineDashed);
    }
}

void MainWindow::updateLineWidth()
{
    // 更新选中的绘图项的线条宽度
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        if (auto lineItem = dynamic_cast<LineItem*>(item)) {
            QPen pen = lineItem->pen();
            pen.setWidth(lineWidthSpin->value());
            lineItem->setPen(pen);
        } else if (auto arrowItem = dynamic_cast<ArrowItem*>(item)) {
            QPen pen = arrowItem->pen();
            pen.setWidth(lineWidthSpin->value());
            arrowItem->setPen(pen);
        } else if (auto rectItem = dynamic_cast<RectangleItem*>(item)) {
            QPen pen = rectItem->pen();
            pen.setWidth(lineWidthSpin->value());
            rectItem->setPen(pen);
        } else if (auto circleItem = dynamic_cast<CircleItem*>(item)) {
            QPen pen = circleItem->pen();
            pen.setWidth(lineWidthSpin->value());
            circleItem->setPen(pen);
        } else if (auto starItem = dynamic_cast<StarItem*>(item)) {
            QPen pen = starItem->pen();
            pen.setWidth(lineWidthSpin->value());
            starItem->setPen(pen);
        } else if (auto polyItem = dynamic_cast<PolygonItem*>(item)) {
            QPen pen = polyItem->pen();
            pen.setWidth(lineWidthSpin->value());
            polyItem->setPen(pen);
        }
    }
    isModified = true;
}

void MainWindow::updateLineColor()
{
    QColorDialog dialog(Qt::black, this);
    dialog.setWindowTitle(tr("选择线条颜色"));
    dialog.setOptions(QColorDialog::DontUseNativeDialog);

    if (dialog.exec() == QDialog::Accepted) {
        QColor color = dialog.currentColor();
        if (color.isValid()) {
            // 更新按钮颜色
            lineColorButton->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(color.name()));

            // 更新选中的绘图项的线条颜色
            QList<QGraphicsItem*> selectedItems = scene->selectedItems();
            for (QGraphicsItem* item : selectedItems) {
                if (auto lineItem = dynamic_cast<LineItem*>(item)) {
                    QPen pen = lineItem->pen();
                    pen.setColor(color);
                    lineItem->setPen(pen);
                } else if (auto arrowItem = dynamic_cast<ArrowItem*>(item)) {
                    QPen pen = arrowItem->pen();
                    pen.setColor(color);
                    arrowItem->setPen(pen);
                } else if (auto rectItem = dynamic_cast<RectangleItem*>(item)) {
                    QPen pen = rectItem->pen();
                    pen.setColor(color);
                    rectItem->setPen(pen);
                } else if (auto circleItem = dynamic_cast<CircleItem*>(item)) {
                    QPen pen = circleItem->pen();
                    pen.setColor(color);
                    circleItem->setPen(pen);
                } else if (auto starItem = dynamic_cast<StarItem*>(item)) {
                    QPen pen = starItem->pen();
                    pen.setColor(color);
                    starItem->setPen(pen);
                } else if (auto polyItem = dynamic_cast<PolygonItem*>(item)) {
                    QPen pen = polyItem->pen();
                    pen.setColor(color);
                    polyItem->setPen(pen);
                }
            }
            isModified = true;
        }
    }
}

void MainWindow::updateFillColor()
{
    QColorDialog dialog(QColor(173, 216, 230), this); // 浅蓝色
    dialog.setWindowTitle(tr("选择填充颜色"));
    dialog.setOptions(QColorDialog::DontUseNativeDialog);

    if (dialog.exec() == QDialog::Accepted) {
        QColor color = dialog.currentColor();
        if (color.isValid()) {
            // 更新按钮颜色
            fillColorButton->setStyleSheet(QString("background-color: %1; border: 1px solid gray;").arg(color.name()));

            // 更新选中的绘图项的填充颜色
            QList<QGraphicsItem*> selectedItems = scene->selectedItems();
            for (QGraphicsItem* item : selectedItems) {
                if (auto rectItem = dynamic_cast<RectangleItem*>(item)) {
                    QBrush brush = rectItem->brush();
                    brush.setColor(color);
                    rectItem->setBrush(brush);
                } else if (auto circleItem = dynamic_cast<CircleItem*>(item)) {
                    QBrush brush = circleItem->brush();
                    brush.setColor(color);
                    circleItem->setBrush(brush);
                } else if (auto starItem = dynamic_cast<StarItem*>(item)) {
                    QBrush brush = starItem->brush();
                    brush.setColor(color);
                    starItem->setBrush(brush);
                } else if (auto polyItem = dynamic_cast<PolygonItem*>(item)) {
                    QBrush brush = polyItem->brush();
                    brush.setColor(color);
                    polyItem->setBrush(brush);
                }
            }
            isModified = true;
        }
    }
}

void MainWindow::updateFillEnabled()
{
    bool enabled = fillEnabledCheck->isChecked();

    // 更新选中的绘图项的填充状态
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        if (auto rectItem = dynamic_cast<RectangleItem*>(item)) {
            rectItem->setFillEnabled(enabled);
        } else if (auto circleItem = dynamic_cast<CircleItem*>(item)) {
            circleItem->setFillEnabled(enabled);
        } else if (auto starItem = dynamic_cast<StarItem*>(item)) {
            starItem->setFillEnabled(enabled);
        } else if (auto polyItem = dynamic_cast<PolygonItem*>(item)) {
            polyItem->setFillEnabled(enabled);
        }
    }
    isModified = true;
}

void MainWindow::updateKeepCircle()
{
    bool keepCircle = keepCircleCheck->isChecked();

    // 更新选中的圆形项的保持圆形状态
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    for (QGraphicsItem* item : selectedItems) {
        if (auto circleItem = dynamic_cast<CircleItem*>(item)) {
            circleItem->setIsCircle(keepCircle);
        }
    }
    isModified = true;
}

void MainWindow::pasteSelectedItem()
{
    qDebug() << "=== pasteSelectedItem called ===";
    qDebug() << "QRCodeItem::s_hasCopiedItem:" << QRCodeItem::s_hasCopiedItem;
    qDebug() << "BarcodeItem::s_hasCopiedItem:" << BarcodeItem::s_hasCopiedItem;
    qDebug() << "TextItem::s_hasCopiedItem:" << TextItem::s_hasCopiedItem;
    qDebug() << "ImageItem::s_hasCopiedItem:" << ImageItem::s_hasCopiedItem;
    qDebug() << "LineItem::s_hasCopiedItem:" << LineItem::s_hasCopiedItem;
    qDebug() << "RectangleItem::s_hasCopiedItem:" << RectangleItem::s_hasCopiedItem;
    qDebug() << "CircleItem::s_hasCopiedItem:" << CircleItem::s_hasCopiedItem;

    // 检查是否有复制的内容
    if (!QRCodeItem::s_hasCopiedItem && !BarcodeItem::s_hasCopiedItem && !TextItem::s_hasCopiedItem && !ImageItem::s_hasCopiedItem &&
        !LineItem::s_hasCopiedItem && !RectangleItem::s_hasCopiedItem && !CircleItem::s_hasCopiedItem && !StarItem::s_hasCopiedItem &&
        !ArrowItem::s_hasCopiedItem && !PolygonItem::s_hasCopiedItem) {
        qDebug() << "没有可粘贴的内容";
        return;
    }

    qDebug() << "Starting paste preview...";
    // 开始粘贴预览模式
    startPastePreview();
}

// 粘贴预览相关方法实现
void MainWindow::startPastePreview()
{
    qDebug() << "=== startPastePreview called ===";

    // 检查是否有可粘贴的内容
    if (!QRCodeItem::s_hasCopiedItem && !BarcodeItem::s_hasCopiedItem && !TextItem::s_hasCopiedItem && !ImageItem::s_hasCopiedItem &&
        !LineItem::s_hasCopiedItem && !RectangleItem::s_hasCopiedItem && !CircleItem::s_hasCopiedItem && !StarItem::s_hasCopiedItem &&
        !ArrowItem::s_hasCopiedItem && !PolygonItem::s_hasCopiedItem) {
        qDebug() << "startPastePreview: No copied items available";
        return;
    }

    qDebug() << "Setting paste mode active...";
    // 设置粘贴模式标志
    m_pasteModeActive = true;
    
    // 根据复制的类型创建对应的预览项目
    if (QRCodeItem::s_hasCopiedItem) {
        // 创建预览二维码
        m_previewItem = new QRCodeItem(QRCodeItem::s_copiedText);
        QRCodeItem* previewQR = static_cast<QRCodeItem*>(m_previewItem);
        previewQR->setSize(QRCodeItem::s_copiedSize);
        previewQR->setForegroundColor(QRCodeItem::s_copiedForegroundColor);
        previewQR->setBackgroundColor(QRCodeItem::s_copiedBackgroundColor);
    }
    else if (BarcodeItem::s_hasCopiedItem) {
        // 创建预览条形码
        m_previewItem = new BarcodeItem(BarcodeItem::s_copiedData, BarcodeItem::s_copiedFormat);
        BarcodeItem* previewBarcode = static_cast<BarcodeItem*>(m_previewItem);
        previewBarcode->setSize(BarcodeItem::s_copiedSize);
        previewBarcode->setForegroundColor(BarcodeItem::s_copiedForegroundColor);
        previewBarcode->setBackgroundColor(BarcodeItem::s_copiedBackgroundColor);
        previewBarcode->setShowHumanReadableText(BarcodeItem::s_copiedShowHumanReadableText);
        previewBarcode->setHumanReadableTextFont(BarcodeItem::s_copiedHumanReadableTextFont);
    }
    else if (TextItem::s_hasCopiedItem) {
        // 创建预览文本项
        m_previewItem = new TextItem(TextItem::s_copiedText);
        TextItem* previewText = static_cast<TextItem*>(m_previewItem);
        previewText->setSize(TextItem::s_copiedSize);
        previewText->setFont(TextItem::s_copiedFont);
        previewText->setTextColor(TextItem::s_copiedTextColor);
        previewText->setBackgroundColor(TextItem::s_copiedBackgroundColor);
        previewText->setBorderPen(TextItem::s_copiedBorderPen);
        previewText->setBorderEnabled(TextItem::s_copiedBorderEnabled);
        previewText->setAlignment(TextItem::s_copiedAlignment);
        previewText->setWordWrap(TextItem::s_copiedWordWrap);
        previewText->setAutoResize(TextItem::s_copiedAutoResize);
        previewText->setBackgroundEnabled(TextItem::s_copiedBackgroundEnabled);
        previewText->setLetterSpacing(TextItem::s_copiedLetterSpacing);
    }
    else if (ImageItem::s_hasCopiedItem) {
        // 创建预览图像项
        m_previewItem = new ImageItem(ImageItem::s_copiedImageData);
        ImageItem* previewImage = static_cast<ImageItem*>(m_previewItem);
        previewImage->setSize(ImageItem::s_copiedSize);
        previewImage->setKeepAspectRatio(ImageItem::s_copiedKeepAspectRatio);
        previewImage->setOpacity(ImageItem::s_copiedOpacity);
    }
    else if (LineItem::s_hasCopiedItem) {
        // 创建预览线条项
        m_previewItem = new LineItem(LineItem::s_copiedStartPoint, LineItem::s_copiedEndPoint);
        LineItem* previewLine = static_cast<LineItem*>(m_previewItem);
        previewLine->setPen(LineItem::s_copiedPen);
    }
    else if (RectangleItem::s_hasCopiedItem) {
        // 创建预览矩形项
        m_previewItem = new RectangleItem(RectangleItem::s_copiedSize);
        RectangleItem* previewRect = static_cast<RectangleItem*>(m_previewItem);
        previewRect->setPen(RectangleItem::s_copiedPen);
        previewRect->setBrush(RectangleItem::s_copiedBrush);
        previewRect->setFillEnabled(RectangleItem::s_copiedFillEnabled);
        previewRect->setBorderEnabled(RectangleItem::s_copiedBorderEnabled);
    }
    else if (CircleItem::s_hasCopiedItem) {
        // 创建预览圆形项
        m_previewItem = new CircleItem(CircleItem::s_copiedSize);
        CircleItem* previewCircle = static_cast<CircleItem*>(m_previewItem);
        previewCircle->setPen(CircleItem::s_copiedPen);
        previewCircle->setBrush(CircleItem::s_copiedBrush);
        previewCircle->setFillEnabled(CircleItem::s_copiedFillEnabled);
        previewCircle->setBorderEnabled(CircleItem::s_copiedBorderEnabled);
        previewCircle->setIsCircle(CircleItem::s_copiedIsCircle);
    }
    else if (StarItem::s_hasCopiedItem) {
        // 创建预览星形项
        m_previewItem = new StarItem(StarItem::s_copiedSize);
        StarItem* previewStar = static_cast<StarItem*>(m_previewItem);
        previewStar->setPen(StarItem::s_copiedPen);
        previewStar->setBrush(StarItem::s_copiedBrush);
        previewStar->setFillEnabled(StarItem::s_copiedFillEnabled);
        previewStar->setBorderEnabled(StarItem::s_copiedBorderEnabled);
        previewStar->setPointCount(StarItem::s_copiedPoints);
        previewStar->setInnerRatio(StarItem::s_copiedInnerRatio);
    }
    else if (ArrowItem::s_hasCopiedItem) {
        // 创建预览箭头项
        m_previewItem = new ArrowItem(ArrowItem::s_copiedStartPoint, ArrowItem::s_copiedEndPoint);
        ArrowItem* previewArrow = static_cast<ArrowItem*>(m_previewItem);
        previewArrow->setPen(ArrowItem::s_copiedPen);
        previewArrow->setHeadLength(ArrowItem::s_copiedHeadLength);
        previewArrow->setHeadAngle(ArrowItem::s_copiedHeadAngle);
    }
    else if (PolygonItem::s_hasCopiedItem) {
        // 创建预览多边形项
        m_previewItem = new PolygonItem(PolygonItem::s_copiedSize);
        PolygonItem* previewPoly = static_cast<PolygonItem*>(m_previewItem);
        previewPoly->setPen(PolygonItem::s_copiedPen);
        previewPoly->setBrush(PolygonItem::s_copiedBrush);
        previewPoly->setFillEnabled(PolygonItem::s_copiedFillEnabled);
        previewPoly->setBorderEnabled(PolygonItem::s_copiedBorderEnabled);
        previewPoly->setPoints(PolygonItem::s_copiedPoints);
    }

    if (!m_previewItem) {
        m_pasteModeActive = false;
        return;
    }
    
    // 设置预览样式（半透明，不可选择）
    m_previewItem->setOpacity(0.6);
    m_previewItem->setFlag(QGraphicsItem::ItemIsSelectable, false);
    m_previewItem->setFlag(QGraphicsItem::ItemIsMovable, false);
    
    // 添加到场景
    scene->addItem(m_previewItem);
    
    // 设置鼠标跟踪
    view->setMouseTracking(true);
    
    // 改变鼠标光标
    view->setCursor(Qt::CrossCursor);
    
    // 设置焦点以便接收键盘事件
    view->setFocus();
    
    qDebug() << "粘贴预览模式开始";
}

void MainWindow::endPastePreview()
{
    if (!m_pasteModeActive) {
        return;
    }
    
    m_pasteModeActive = false;
    
    // 移除预览item
    if (m_previewItem) {
        scene->removeItem(m_previewItem);
        delete m_previewItem;
        m_previewItem = nullptr;
    }
    
    // 恢复鼠标设置
    view->setMouseTracking(false);
    view->setCursor(Qt::ArrowCursor);
    
    qDebug() << "粘贴预览模式结束";
}

void MainWindow::updatePreviewPosition(const QPointF& scenePos)
{
    if (m_previewItem && m_pasteModeActive) {
        m_previewItem->setPos(scenePos);
    }
}

void MainWindow::confirmPaste(const QPointF& scenePos)
{
    if (!m_pasteModeActive) {
        return;
    }
    
    QGraphicsItem* newItem = nullptr;
    QString description;
    
    // 根据复制的类型创建对应的新项目
    if (QRCodeItem::s_hasCopiedItem) {
        QRCodeItem* newQRCode = new QRCodeItem(QRCodeItem::s_copiedText);
        newQRCode->setSize(QRCodeItem::s_copiedSize);
        newQRCode->setForegroundColor(QRCodeItem::s_copiedForegroundColor);
        newQRCode->setBackgroundColor(QRCodeItem::s_copiedBackgroundColor);
        newQRCode->setPos(scenePos);
        newItem = newQRCode;
        description = tr("粘贴二维码");
    }
    else if (BarcodeItem::s_hasCopiedItem) {
        BarcodeItem* newBarcode = new BarcodeItem(BarcodeItem::s_copiedData, BarcodeItem::s_copiedFormat);
        newBarcode->setSize(BarcodeItem::s_copiedSize);
        newBarcode->setForegroundColor(BarcodeItem::s_copiedForegroundColor);
        newBarcode->setBackgroundColor(BarcodeItem::s_copiedBackgroundColor);
        newBarcode->setShowHumanReadableText(BarcodeItem::s_copiedShowHumanReadableText);
        newBarcode->setHumanReadableTextFont(BarcodeItem::s_copiedHumanReadableTextFont);
        newBarcode->setPos(scenePos);
        newItem = newBarcode;
        description = tr("粘贴条形码");
    }
    else if (TextItem::s_hasCopiedItem) {
        TextItem* newText = new TextItem(TextItem::s_copiedText);
        newText->setSize(TextItem::s_copiedSize);
        newText->setFont(TextItem::s_copiedFont);
        newText->setTextColor(TextItem::s_copiedTextColor);
        newText->setBackgroundColor(TextItem::s_copiedBackgroundColor);
        newText->setBorderPen(TextItem::s_copiedBorderPen);
        newText->setBorderEnabled(TextItem::s_copiedBorderEnabled);
        newText->setAlignment(TextItem::s_copiedAlignment);
        newText->setWordWrap(TextItem::s_copiedWordWrap);
        newText->setAutoResize(TextItem::s_copiedAutoResize);
        newText->setBackgroundEnabled(TextItem::s_copiedBackgroundEnabled);
        newText->setLetterSpacing(TextItem::s_copiedLetterSpacing);
        newText->setPos(scenePos);
        newItem = newText;
        description = tr("粘贴文本");
    }
    else if (ImageItem::s_hasCopiedItem) {
        ImageItem* newImage = new ImageItem(ImageItem::s_copiedImageData);
        newImage->setSize(ImageItem::s_copiedSize);
        newImage->setKeepAspectRatio(ImageItem::s_copiedKeepAspectRatio);
        newImage->setOpacity(ImageItem::s_copiedOpacity);
        newImage->setPos(scenePos);
        newItem = newImage;
        description = tr("粘贴图像");
    }
    else if (LineItem::s_hasCopiedItem) {
        LineItem* newLine = new LineItem(LineItem::s_copiedStartPoint, LineItem::s_copiedEndPoint);
        newLine->setPen(LineItem::s_copiedPen);
        newLine->setPos(scenePos);
        newItem = newLine;
        description = tr("粘贴线条");
    }
    else if (RectangleItem::s_hasCopiedItem) {
        RectangleItem* newRect = new RectangleItem(RectangleItem::s_copiedSize);
        newRect->setPen(RectangleItem::s_copiedPen);
        newRect->setBrush(RectangleItem::s_copiedBrush);
        newRect->setFillEnabled(RectangleItem::s_copiedFillEnabled);
        newRect->setBorderEnabled(RectangleItem::s_copiedBorderEnabled);
        newRect->setPos(scenePos);
        newItem = newRect;
        description = tr("粘贴矩形");
    }
    else if (CircleItem::s_hasCopiedItem) {
        CircleItem* newCircle = new CircleItem(CircleItem::s_copiedSize);
        newCircle->setPen(CircleItem::s_copiedPen);
        newCircle->setBrush(CircleItem::s_copiedBrush);
        newCircle->setFillEnabled(CircleItem::s_copiedFillEnabled);
        newCircle->setBorderEnabled(CircleItem::s_copiedBorderEnabled);
        newCircle->setIsCircle(CircleItem::s_copiedIsCircle);
        newCircle->setPos(scenePos);
        newItem = newCircle;
        description = tr("粘贴圆形");
    }
    else if (StarItem::s_hasCopiedItem) {
        StarItem* newStar = new StarItem(StarItem::s_copiedSize);
        newStar->setPen(StarItem::s_copiedPen);
        newStar->setBrush(StarItem::s_copiedBrush);
        newStar->setFillEnabled(StarItem::s_copiedFillEnabled);
        newStar->setBorderEnabled(StarItem::s_copiedBorderEnabled);
        newStar->setPointCount(StarItem::s_copiedPoints);
        newStar->setInnerRatio(StarItem::s_copiedInnerRatio);
        newStar->setPos(scenePos);
        newItem = newStar;
        description = tr("粘贴星形");
    }
    else if (ArrowItem::s_hasCopiedItem) {
        ArrowItem* newArrow = new ArrowItem(ArrowItem::s_copiedStartPoint, ArrowItem::s_copiedEndPoint);
        newArrow->setPen(ArrowItem::s_copiedPen);
        newArrow->setHeadLength(ArrowItem::s_copiedHeadLength);
        newArrow->setHeadAngle(ArrowItem::s_copiedHeadAngle);
        newArrow->setPos(scenePos);
        newItem = newArrow;
        description = tr("粘贴箭头");
    }
    else if (PolygonItem::s_hasCopiedItem) {
        PolygonItem* newPoly = new PolygonItem(PolygonItem::s_copiedSize);
        newPoly->setPen(PolygonItem::s_copiedPen);
        newPoly->setBrush(PolygonItem::s_copiedBrush);
        newPoly->setFillEnabled(PolygonItem::s_copiedFillEnabled);
        newPoly->setBorderEnabled(PolygonItem::s_copiedBorderEnabled);
        newPoly->setPoints(PolygonItem::s_copiedPoints);
        newPoly->setPos(scenePos);
        newItem = newPoly;
        description = tr("粘贴多边形");
    }
    else if (LineItem::s_hasCopiedItem) {
        LineItem* newLine = new LineItem(LineItem::s_copiedStartPoint, LineItem::s_copiedEndPoint);
        newLine->setPen(LineItem::s_copiedPen);
        newLine->setPos(scenePos);
        newItem = newLine;
        description = tr("粘贴直线");
    }
    else if (RectangleItem::s_hasCopiedItem) {
        RectangleItem* newRect = new RectangleItem(RectangleItem::s_copiedSize);
        newRect->setPen(RectangleItem::s_copiedPen);
        newRect->setBrush(RectangleItem::s_copiedBrush);
        newRect->setFillEnabled(RectangleItem::s_copiedFillEnabled);
        newRect->setBorderEnabled(RectangleItem::s_copiedBorderEnabled);
        newRect->setPos(scenePos);
        newItem = newRect;
        description = tr("粘贴矩形");
    }
    else if (CircleItem::s_hasCopiedItem) {
        CircleItem* newCircle = new CircleItem(CircleItem::s_copiedSize);
        newCircle->setPen(CircleItem::s_copiedPen);
        newCircle->setBrush(CircleItem::s_copiedBrush);
        newCircle->setFillEnabled(CircleItem::s_copiedFillEnabled);
        newCircle->setBorderEnabled(CircleItem::s_copiedBorderEnabled);
        newCircle->setIsCircle(CircleItem::s_copiedIsCircle);
        newCircle->setPos(scenePos);
        newItem = newCircle;
        description = tr("粘贴圆形");
    }

    if (!newItem) {
        endPastePreview();
        return;
    }
    
    // 使用撤销管理器添加到场景
    if (undoManager) {
        undoManager->addItem(newItem, description);
    } else {
        scene->addItem(newItem);
        emit scene->itemAdded(newItem);
    }
    
    // 选中新创建的项目
    scene->clearSelection();
    newItem->setSelected(true);
    
    // 结束预览模式
    endPastePreview();
    
    // 标记为已修改
    isModified = true;
    
    qDebug() << description << "已粘贴到位置:" << scenePos;
}

void MainWindow::cutSelectedItem()
{
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }
    qDebug()<<"cutSelectedItem";
    QGraphicsItem* item = selectedItems.first();

    // 使用统一的剪切命令，而不是分开的复制和删除操作
    if (undoManager) {
        undoManager->cutItem(item, tr("剪切图形项"));
    } else {
        // 如果没有撤销管理器，回退到原来的方法
        qDebug()<<"调用了";
        copySelectedItem();
        deleteSelectedItem();
    }
}

// 对齐操作实现
void MainWindow::alignItemsLeft()
{
    if (!scene) {
        return;
    }
    
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }
    
    // 找到标签背景项来获取标签边界
    QGraphicsItem* labelBackground = nullptr;
    for (QGraphicsItem* item : scene->items()) {
        if (item->data(0).toString() == "labelBackground") {
            labelBackground = item;
            break;
        }
    }
    
    if (!labelBackground) {
        return;
    }
    
    // 获取标签的左边界
    qreal labelLeft = labelBackground->boundingRect().left();
    
    // 开始宏命令用于撤销
    if (undoManager) {
        undoManager->beginMacro(tr("左对齐"));
    }
    
    // 移动每个选中的项到左边界
    for (QGraphicsItem* item : selectedItems) {
        if (item->data(0).toString() != "labelBackground") {  // 不移动标签背景自身
            QPointF oldPos = item->pos();
            QPointF newPos(labelLeft, oldPos.y());
            
            if (undoManager) {
                undoManager->moveItem(item, oldPos, newPos, false);
            } else {
                item->setPos(newPos);
            }
        }
    }
    
    if (undoManager) {
        undoManager->endMacro();
    }
    
    isModified = true;
}

void MainWindow::alignItemsCenter()
{
    if (!scene) {
        return;
    }
    
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }
    
    // 找到标签背景项来获取标签边界
    QGraphicsItem* labelBackground = nullptr;
    for (QGraphicsItem* item : scene->items()) {
        if (item->data(0).toString() == "labelBackground") {
            labelBackground = item;
            break;
        }
    }
    
    if (!labelBackground) {
        return;
    }
    
    // 获取标签的中心X坐标
    QRectF labelRect = labelBackground->boundingRect();
    qreal labelCenterX = labelRect.left() + labelRect.width() / 2.0;
    
    // 开始宏命令用于撤销
    if (undoManager) {
        undoManager->beginMacro(tr("居中对齐"));
    }
    
    // 移动每个选中的项到中心
    for (QGraphicsItem* item : selectedItems) {
        if (item->data(0).toString() != "labelBackground") {  // 不移动标签背景自身
            QPointF oldPos = item->pos();
            qreal itemWidth = 0;
            
            // 获取不同图形项的实际绘制宽度
            QString itemType = typeid(*item).name();
            qDebug() << "Item actual type:" << itemType;
            
            if (itemType.contains("QRCodeItem")) {
                QRCodeItem* qrItem = static_cast<QRCodeItem*>(item);
                itemWidth = qrItem->size().width();
                qDebug()<<"二维码调用";
            } else if (itemType.contains("BarcodeItem")) {
                BarcodeItem* bcItem = static_cast<BarcodeItem*>(item);
                itemWidth = bcItem->size().width();
                qDebug()<<"条形码调用";
            } else if (itemType.contains("TextItem")) {
                TextItem* textItem = static_cast<TextItem*>(item);
                itemWidth = textItem->size().width();
                qDebug()<<"文本项调用";
            } else {
                // 对于其他类型的图形项，使用 boundingRect
                itemWidth = item->boundingRect().width();
                qDebug()<<"其他类型调用, typeid name:" << typeid(*item).name();
            }
            
            // 居中对齐：项目中心与标签中心对齐
            // 对于所有图形项，实际绘制区域都是从 (0,0) 开始的
            // 所以只需要：itemLeftX = labelCenterX - itemWidth/2
            qreal itemLeftX = labelCenterX - itemWidth / 2.0;
            qDebug()<<"居中对齐计算 - 标签中心:"<<labelCenterX<<"项目宽度:"<<itemWidth<<"左边位置:"<<itemLeftX;
            QPointF newPos(itemLeftX, oldPos.y());
            
            if (undoManager) {
                undoManager->moveItem(item, oldPos, newPos, false);
            } else {
                item->setPos(newPos);
            }
        }
    }
    
    if (undoManager) {
        undoManager->endMacro();
    }
    
    isModified = true;
}

void MainWindow::alignItemsRight()
{
    if (!scene) {
        return;
    }
    
    QList<QGraphicsItem*> selectedItems = scene->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }
    
    // 找到标签背景项来获取标签边界
    QGraphicsItem* labelBackground = nullptr;
    for (QGraphicsItem* item : scene->items()) {
        if (item->data(0).toString() == "labelBackground") {
            labelBackground = item;
            break;
        }
    }
    
    if (!labelBackground) {
        return;
    }
    
    // 获取标签的右边界
    QRectF labelRect = labelBackground->boundingRect();
    qreal labelRight = labelRect.right();
    
    // 开始宏命令用于撤销
    if (undoManager) {
        undoManager->beginMacro(tr("右对齐"));
    }
    
    // 移动每个选中的项到右边界
    for (QGraphicsItem* item : selectedItems) {
        if (item->data(0).toString() != "labelBackground") {  // 不移动标签背景自身
            QPointF oldPos = item->pos();
            qreal itemWidth = 0;
            
            // 获取不同图形项的实际绘制宽度
            QString itemType = typeid(*item).name();
            
            if (itemType.contains("QRCodeItem")) {
                QRCodeItem* qrItem = static_cast<QRCodeItem*>(item);
                itemWidth = qrItem->size().width();
            } else if (itemType.contains("BarcodeItem")) {
                BarcodeItem* bcItem = static_cast<BarcodeItem*>(item);
                itemWidth = bcItem->size().width();
            } else if (itemType.contains("TextItem")) {
                TextItem* textItem = static_cast<TextItem*>(item);
                itemWidth = textItem->size().width();
            } else {
                // 对于其他类型的图形项，使用 boundingRect
                itemWidth = item->boundingRect().width();
            }
            
            // 右对齐：项目右边与标签右边对齐
            // 对于所有图形项，实际绘制区域都是从 (0,0) 开始的
            // 所以只需要：itemLeftX = labelRight - itemWidth
            qreal itemLeftX = labelRight - itemWidth;
            
            qDebug() << "项目类型:" << itemType << "项目宽度:" << itemWidth 
                     << "计算的左边位置:" << itemLeftX;
            
            QPointF newPos(itemLeftX, oldPos.y());
            
            if (undoManager) {
                undoManager->moveItem(item, oldPos, newPos, false);
            } else {
                item->setPos(newPos);
            }
        }
    }
    
    if (undoManager) {
        undoManager->endMacro();
    }
    
    isModified = true;
}

void MainWindow::updateTextLetterSpacing(int value)
{
    TextItem* textItem = getSelectedTextItem();
    if (textItem) {
        textItem->setLetterSpacing(value);
        isModified = true;
    }
}

void MainWindow::openAssetBrowser()
{
    AssetBrowserDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted || !dlg.hasSelection()) {
        return;
    }

    const AssetItem item = dlg.selected();

    // 将素材作为图像元素加入场景
    if (!scene) return;

    // 将 QPixmap 转为 PNG 字节
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    const QPixmap pm = item.full;
    pm.save(&buffer, "PNG");
    buffer.close();

    auto element = std::make_unique<ImageElement>(bytes, QSizeF(pm.width(), pm.height()), true, 1.0);
    element->addToScene(scene);
    if (QGraphicsItem* gi = element->getItem()) {
        // 放置到视图中心
        QPointF center = view ? view->mapToScene(view->viewport()->rect().center()) : QPointF(0,0);
        gi->setPos(center - QPointF(pm.width()/2.0, pm.height()/2.0));
    }

    // 纳入撤销系统（简单方式：标记修改即可；后续可加入命令）
    isModified = true;
}

// 规格化：确保选中项完全位于标签背景内；对图像执行裁剪，对形状/文本等执行位置与尺寸收缩
void MainWindow::normalizeSelectedItems()
{
    if (!scene) return;
    QList<QGraphicsItem*> sel = scene->selectedItems();
    if (sel.isEmpty()) return;

    // 找到标签背景尺寸（场景中 data(0)=="labelBackground" 的 QGraphicsPathItem）
    QRectF labelRectPx; // 场景坐标
    for (QGraphicsItem* item : scene->items()) {
        if (item && item->data(0).toString() == "labelBackground") {
            labelRectPx = item->sceneBoundingRect();
            break;
        }
    }
    if (labelRectPx.isNull()) return;

    // 撤销宏：批量规格化
    if (undoManager) {
        undoManager->beginMacro(tr("规格化选中对象"));
    }

    auto pushMove = [this](QGraphicsItem* it, const QPointF& oldPos){
        if (!undoManager) return;
        undoManager->moveItem(it, oldPos, it->pos(), false);
    };
    auto pushResize = [this](QGraphicsItem* it, const QRectF& oldRect, const QRectF& newRect){
        if (!undoManager) return;
        undoManager->resizeItem(it, oldRect, newRect);
    };

    auto clipLineToRect = [](const QRectF& r, QPointF p0, QPointF p1, QPointF& out0, QPointF& out1)->bool {
        // Cohen–Sutherland
        auto outCode = [&](const QPointF& p){
            int code = 0;
            if (p.x() < r.left()) code |= 1;       // left
            else if (p.x() > r.right()) code |= 2; // right
            if (p.y() < r.top()) code |= 4;        // top
            else if (p.y() > r.bottom()) code |= 8;// bottom
            return code;
        };
        int c0 = outCode(p0);
        int c1 = outCode(p1);
        bool accept = false;
        while (true) {
            if (!(c0 | c1)) { accept = true; break; }          // both inside
            if (c0 & c1) { break; }                            // both outside same half-space
            int cOut = c0 ? c0 : c1;
            double x=0, y=0;
            double x0=p0.x(), y0=p0.y(), x1=p1.x(), y1=p1.y();
            if (cOut & 4) { // top
                x = x0 + (x1 - x0) * (r.top() - y0) / (y1 - y0);
                y = r.top();
            } else if (cOut & 8) { // bottom
                x = x0 + (x1 - x0) * (r.bottom() - y0) / (y1 - y0);
                y = r.bottom();
            } else if (cOut & 1) { // left
                y = y0 + (y1 - y0) * (r.left() - x0) / (x1 - x0);
                x = r.left();
            } else { // right
                y = y0 + (y1 - y0) * (r.right() - x0) / (x1 - x0);
                x = r.right();
            }
            if (cOut == c0) { p0 = QPointF(x,y); c0 = outCode(p0);} else { p1 = QPointF(x,y); c1 = outCode(p1);}        
        }
        if (accept) { out0 = p0; out1 = p1; return true; }
        return false;
    };

    for (QGraphicsItem* item : sel) {
        if (!item) continue;
        if (item->data(0).toString() == "labelBackground") continue;
        // 仅处理顶层选中项（不处理选择框）
        if (item->data(0).toString() == "selectionFrame") continue;

        // 使用内容矩形（不含手柄/描边padding）确保定位准确，避免偏移
        QRectF contentSceneRect;
        if (auto ti = dynamic_cast<TextItem*>(item)) {
            QPointF tl = item->mapToScene(QPointF(0,0));
            QPointF br = item->mapToScene(QPointF(ti->size().width(), ti->size().height()));
            contentSceneRect = QRectF(tl, br).normalized();
        } else if (auto ii = dynamic_cast<ImageItem*>(item)) {
            QPointF tl = item->mapToScene(QPointF(0,0));
            QPointF br = item->mapToScene(QPointF(ii->size().width(), ii->size().height()));
            contentSceneRect = QRectF(tl, br).normalized();
        } else if (auto ri = dynamic_cast<RectangleItem*>(item)) {
            QPointF tl = item->mapToScene(QPointF(0,0));
            QPointF br = item->mapToScene(QPointF(ri->size().width(), ri->size().height()));
            contentSceneRect = QRectF(tl, br).normalized();
        } else if (auto ci = dynamic_cast<CircleItem*>(item)) {
            QPointF tl = item->mapToScene(QPointF(0,0));
            QPointF br = item->mapToScene(QPointF(ci->size().width(), ci->size().height()));
            contentSceneRect = QRectF(tl, br).normalized();
        } else if (auto li = dynamic_cast<LineItem*>(item)) {
            QPointF sp = li->mapToScene(li->startPoint());
            QPointF ep = li->mapToScene(li->endPoint());
            contentSceneRect = QRectF(sp, ep).normalized();
        } else {
            contentSceneRect = item->sceneBoundingRect();
        }
        QRectF sb = contentSceneRect;
        if (labelRectPx.contains(sb)) {
            // 已完全在内，跳过
            continue;
        }

        // 计算与标签的交集
        QRectF inter = labelRectPx.intersected(sb);
        if (inter.isNull()) {
            // 完全在外：将其移动到标签左上角并若支持尺寸则收缩到 10x10 像素最小尺寸
            QPointF oldPos = item->pos();
            QPointF targetTopLeft = labelRectPx.topLeft();
            // 转换到局部坐标的移动量
            QPointF deltaScene = targetTopLeft - contentSceneRect.topLeft();
            item->moveBy(deltaScene.x(), deltaScene.y());
            pushMove(item, oldPos);

            // 尺寸收缩
            if (auto textItem = dynamic_cast<TextItem*>(item)) {
                QSizeF oldSize = textItem->size();
                QRectF oldRect(QPointF(0,0), oldSize);
                QSizeF newSize( qMax<qreal>(10.0, oldSize.width()), qMax<qreal>(10.0, oldSize.height()) );
                textItem->setSize(newSize);
                pushResize(item, oldRect, QRectF(QPointF(0,0), newSize));
            } else if (auto img = dynamic_cast<ImageItem*>(item)) {
                QSizeF oldSize = img->size();
                QRectF oldRect(QPointF(0,0), oldSize);
                QSizeF newSize( qMax<qreal>(10.0, oldSize.width()), qMax<qreal>(10.0, oldSize.height()) );
                img->setSize(newSize);
                pushResize(item, oldRect, QRectF(QPointF(0,0), newSize));
            } else if (auto rect = dynamic_cast<RectangleItem*>(item)) {
                QSizeF oldSize = rect->size();
                QRectF oldRect(QPointF(0,0), oldSize);
                QSizeF newSize( qMax<qreal>(10.0, oldSize.width()), qMax<qreal>(10.0, oldSize.height()) );
                rect->setSize(newSize);
                pushResize(item, oldRect, QRectF(QPointF(0,0), newSize));
            } else if (auto circle = dynamic_cast<CircleItem*>(item)) {
                QSizeF oldSize = circle->size();
                QRectF oldRect(QPointF(0,0), oldSize);
                QSizeF newSize( qMax<qreal>(10.0, oldSize.width()), qMax<qreal>(10.0, oldSize.height()) );
                circle->setSize(newSize);
                pushResize(item, oldRect, QRectF(QPointF(0,0), newSize));
            } else if (auto line = dynamic_cast<LineItem*>(item)) {
                // 使用 Cohen–Sutherland 将线段裁剪到标签内
                QRectF oldGeom(line->startPoint(), line->endPoint());
                QPointF spScene = line->mapToScene(line->startPoint());
                QPointF epScene = line->mapToScene(line->endPoint());
                QPointF c0, c1;
                if (clipLineToRect(labelRectPx, spScene, epScene, c0, c1)) {
                    QPointF newSpLocal = line->mapFromScene(c0);
                    QPointF newEpLocal = line->mapFromScene(c1);
                    line->setLine(newSpLocal, newEpLocal);
                    pushResize(item, oldGeom, QRectF(newSpLocal, newEpLocal));
                }
            }
            continue;
        }

        // 部分在外：需要裁剪或收缩
        // 对于可缩放的类型：调整尺寸与位置，使其局部内容矩形与交集对应
        if (auto img = dynamic_cast<ImageItem*>(item)) {
            // 图像：按照当前缩放比例计算源像素裁剪区域
            QPixmap pm = img->pixmap();
            if (!pm.isNull()) {
                // 局部->场景: item->mapToScene(localPoint)
                QPointF localTopLeft = item->mapFromScene(inter.topLeft());
                QPointF localBottomRight = item->mapFromScene(inter.bottomRight());
                QRectF localCropRect(localTopLeft, localBottomRight);
                QSizeF oldSize = img->size();
                if (oldSize.width() > 0 && oldSize.height() > 0) {
                    // 将局部坐标裁剪框映射到源像素坐标
                    qreal sx = localCropRect.x() * (pm.width() / oldSize.width());
                    qreal sy = localCropRect.y() * (pm.height() / oldSize.height());
                    qreal sw = localCropRect.width() * (pm.width() / oldSize.width());
                    qreal sh = localCropRect.height() * (pm.height() / oldSize.height());
                    QRect cropPx = QRect(qFloor(sx), qFloor(sy), qCeil(sw), qCeil(sh)).intersected(pm.rect());
                    if (!cropPx.isEmpty()) {
                        QPixmap cropped = pm.copy(cropPx);
                        QRectF oldRect(QPointF(0,0), oldSize);
                        // 暂时关闭等比以精确设置尺寸
                        bool oldKeep = img->keepAspectRatio();
                        img->setKeepAspectRatio(false);
                        img->setPixmap(cropped);
                        img->setSize(cropped.size());
                        img->setKeepAspectRatio(oldKeep);
                        // 将 item 移动，使裁剪后图像与交集左上角对齐
                        QPointF oldPos = item->pos();
                        QPointF deltaScene = inter.topLeft() - contentSceneRect.topLeft();
                        item->moveBy(deltaScene.x(), deltaScene.y());
                        pushMove(item, oldPos);
                        pushResize(item, oldRect, QRectF(QPointF(0,0), img->size()));
                    }
                }
            }
        } else if (auto textItem = dynamic_cast<TextItem*>(item)) {
            QSizeF oldSize = textItem->size();
            QRectF oldRect(QPointF(0,0), oldSize);
            // 收缩尺寸，不做内容智能裁剪（简单方式）
            QPointF localTL = item->mapFromScene(inter.topLeft());
            QPointF localBR = item->mapFromScene(inter.bottomRight());
            QSizeF newSize(localBR.x()-localTL.x(), localBR.y()-localTL.y());
            if (newSize.width() < 10) newSize.setWidth(10);
            if (newSize.height() < 10) newSize.setHeight(10);
            QPointF oldPos = item->pos();
            QPointF deltaScene = inter.topLeft() - contentSceneRect.topLeft();
            item->moveBy(deltaScene.x(), deltaScene.y());
            textItem->setSize(newSize);
            pushMove(item, oldPos);
            pushResize(item, oldRect, QRectF(QPointF(0,0), newSize));
        } else if (auto rect = dynamic_cast<RectangleItem*>(item)) {
            QSizeF oldSize = rect->size();
            QRectF oldRect(QPointF(0,0), oldSize);
            QPointF localTL = item->mapFromScene(inter.topLeft());
            QPointF localBR = item->mapFromScene(inter.bottomRight());
            QSizeF newSize(localBR.x()-localTL.x(), localBR.y()-localTL.y());
            if (newSize.width() < 10) newSize.setWidth(10);
            if (newSize.height() < 10) newSize.setHeight(10);
            QPointF oldPos = item->pos();
            QPointF deltaScene = inter.topLeft() - contentSceneRect.topLeft();
            item->moveBy(deltaScene.x(), deltaScene.y());
            rect->setSize(newSize);
            pushMove(item, oldPos);
            pushResize(item, oldRect, QRectF(QPointF(0,0), newSize));
        } else if (auto circle = dynamic_cast<CircleItem*>(item)) {
            QSizeF oldSize = circle->size();
            QRectF oldRect(QPointF(0,0), oldSize);
            QPointF localTL = item->mapFromScene(inter.topLeft());
            QPointF localBR = item->mapFromScene(inter.bottomRight());
            QSizeF newSize(localBR.x()-localTL.x(), localBR.y()-localTL.y());
            if (newSize.width() < 10) newSize.setWidth(10);
            if (newSize.height() < 10) newSize.setHeight(10);
            QPointF oldPos = item->pos();
            QPointF deltaScene = inter.topLeft() - contentSceneRect.topLeft();
            item->moveBy(deltaScene.x(), deltaScene.y());
            circle->setSize(newSize);
            pushMove(item, oldPos);
            pushResize(item, oldRect, QRectF(QPointF(0,0), newSize));
        } else if (auto line = dynamic_cast<LineItem*>(item)) {
            QRectF oldGeom(line->startPoint(), line->endPoint());
            QPointF spScene = line->mapToScene(line->startPoint());
            QPointF epScene = line->mapToScene(line->endPoint());
            QPointF c0, c1;
            if (clipLineToRect(labelRectPx, spScene, epScene, c0, c1)) {
                QPointF newSpLocal = line->mapFromScene(c0);
                QPointF newEpLocal = line->mapFromScene(c1);
                line->setLine(newSpLocal, newEpLocal);
                pushResize(item, oldGeom, QRectF(newSpLocal, newEpLocal));
            }
        }
    }

    if (undoManager) {
        undoManager->endMacro();
    }
    statusBar()->showMessage(tr("规格化完成"), 3000);
    scene->update();
}

void MainWindow::updateCornerRadius(int radius)
{
    if (!scene) return;
    QList<QGraphicsItem*> sel = scene->selectedItems();
    if (sel.isEmpty()) return;
    RectangleItem* rect = dynamic_cast<RectangleItem*>(sel.first());
    if (!rect) return;
    double oldR = rect->cornerRadius();
    double newR = radius;
    if (undoManager) {
        undoManager->changeCornerRadius(rect, oldR, newR);
    } else {
        rect->setCornerRadius(newR);
    }
    isModified = true;
    scene->update();
}

void MainWindow::updateLabelCornerRadius(int px)
{
    Q_UNUSED(px);
    // 直接调用 updateLabelSize 重新生成背景路径
    updateLabelSize();
}