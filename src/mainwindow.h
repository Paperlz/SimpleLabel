#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QDockWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QFontComboBox>
#include <QScrollArea>
#include <QToolBar>
#include <QMenu>
#include <QToolButton>
#include <QStyle>
#include <QStyleFactory>
#include <QCloseEvent>
#include <QPrinter>           // 添加这行
#include <QPrintDialog>       // 添加这行
#include<QActionGroup>
#include<QScrollBar>
#include<QTimer>
#include <QPointF>
#include <QRectF>
#include <QHash>
#include <memory>
#include "graphics/labelscene.h"
#include "graphics/ruler.h"
#include "graphics/qrcodeitem.h"
#include "graphics/textitem.h"
#include "graphics/imageitem.h"
#include "graphics/inlinetexteditor.h"
#include "dialogs/templatecenterdialog.h"
#include "core/qrcodeelement.h"
#include "core/textelement.h"
#include "commands/undomanager.h"
#include <QCheckBox>
#include "panels/databaseprintwidget.h"
#include <vector>
#include <optional>
#include <utility>

class DataSource;
class PrintEngine;
struct PrintContext;
class BatchPrintManager;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event);

    void setZoomFactorAtPosition(double factor, const QPointF &scenePos);

private slots:
    // 文件操作槽函数
    void newFile();
    void openFile();

    void clearScene();

    bool saveFile();
    bool saveFileAs();
    void openPrintCenter();  // 统一打印中心
    //void openLayoutExportDialog(); // 排版导出

    // 工具操作槽函数
    void updateLabelSize();
    void resetLabelSize();
    void onPaperSizeChanged(int index);
    void updateLabelCornerRadius(int px);

    void updateRulerPosition();
    void updateRulerScale(double zoomFactor);

    // 右键菜单槽函数
    void copySelectedItem();
    void pasteSelectedItem();
    void deleteSelectedItem();
    void rotateSelectedItem();
    void bringSelectedItemToFront();
    void sendSelectedItemToBack();
    void bringSelectedItemForward();
    void sendSelectedItemBackward();
    // 将超出标签区域的选中项裁剪/收缩到标签内
    void normalizeSelectedItems();

    void cutSelectedItem();

    // 对齐操作槽函数
    void alignItemsLeft();
    void alignItemsCenter();
    void alignItemsRight();

private:
    // 文件操作相关
    bool maybeSave();
    bool saveFile(const QString &fileName);
    bool loadFile(const QString &fileName);
    //void writeSettings();
    //void readSettings();

    void createActions();
    void createMenus();
    void createToolbars();
    void createCentralWidget();
    void createDockWindows();
    void createStatusBar();
    void setInitialStyles();
    void connectSignals();    void updateSelectedItemProperties();    // 码类相关（条形码和二维码）
    void openAssetBrowser(); // 打开素材浏览对话框
    void updateCodeTypeComboBox(bool isQRCode); // 更新码类下拉框选项
    void connectCodeSignals();
    void updateCodeType(int index);
    void updateCodeData();
    void updateCodeWidth(int width);
    void updateCodeHeight(int height);
    void updateBarcodeShowText(bool showText);
    BarcodeItem* getSelectedBarcodeItem();
    QRCodeItem* getSelectedQRCodeItem();

    // 文本相关
    void connectTextSignals();
    void updateTextContent();
    void updateTextFont();
    void updateTextSize(int size);
    void updateTextColor();
    void updateTextBackgroundColor();
    void updateTextAlignment();
    void updateTextBorder();
    void updateTextWordWrap(bool wrap);
    void updateTextAutoResize(bool autoResize);
    void updateTextBackgroundEnabled(bool enabled);
    void updateTextLetterSpacing(int spacing);
    TextItem* getSelectedTextItem();

    void applyCodeDataSource();
    void applyTextDataSource();
    void handleCodeDataSourceEnabledChanged(bool enabled);
    void handleTextDataSourceEnabledChanged(bool enabled);
    void onSceneItemRemoved(QGraphicsItem* item);

    void syncCodeDataSourceWidget(QGraphicsItem* item);
    void syncTextDataSourceWidget(QGraphicsItem* item);
    void clearDataSourceBinding(QGraphicsItem* item);

    void initializePrinting();
    QList<labelelement*> collectElements(bool onlySelected = false) const;
    PrintContext buildPrintContext(QPrinter *printer) const;
    std::unique_ptr<labelelement> wrapItem(QGraphicsItem *item) const;
    std::optional<bool> promptSelectionChoice(const QString &title,
                                             const QString &message,
                                             const QString &selectedLabel,
                                             const QString &allLabel);

    // 图像相关
    void connectImageSignals();
    void loadImageFile();
    void updateImageWidth(int width);
    void updateImageHeight(int height);
    void updateImageKeepAspectRatio(bool keep);
    void updateImageOpacity(int opacity);
    ImageItem* getSelectedImageItem();

    // 绘图工具相关方法
    void connectDrawingSignals();
    void updateLineWidth();
    void updateLineColor();
    void updateFillColor();
    void updateFillEnabled();
    void updateKeepCircle();
    void updateLineAngle(double degrees);
    void updateLineDashed(bool dashed);
    void updateCornerRadius(int radius);

    // 内联编辑相关
    void startTextEditing(TextItem* textItem);
    void onTextEditingFinished(TextItem* textItem, const QString& newText);
    void onTextEditingCancelled(TextItem* textItem);

    void updateContextMenuActions();

    void createRulers();

    void setZoomFactor(double factor);
    void onRubberBandChanged(const QRect &viewportRect,
                             const QPointF &fromScenePoint,
                             const QPointF &toScenePoint);
    
    // 粘贴预览相关方法
         QAction *tableAction = nullptr;
    void startPastePreview();
    void endPastePreview();
    void updatePreviewPosition(const QPointF& scenePos);
         QGroupBox *tableGroup = nullptr;  // 表格设置组（预留）
    void confirmPaste(const QPointF& scenePos);
    void handleRubberBandSelection(const QRectF &selectionRect, bool finalize);

    QCheckBox *showBarcodeTextCheck;

         void insertTable();
    //标尺
    Ruler *horizontalRuler;
    Ruler *verticalRuler;

    // 图形场景相关
    LabelScene *scene = nullptr;
    QGraphicsView *view = nullptr;
    QWidget* cornerWidget = nullptr;
    bool m_isRubberBandSelecting = false;

    // 工具栏
    QToolBar *mainToolBar = nullptr;
    QToolBar *drawingToolBar = nullptr;
    QToolBar *textToolBar = nullptr;
    QToolButton *shapesToolButton = nullptr; // 工具栏上的“图形”下拉按钮

    // 菜单
    QMenu *fileMenu = nullptr;
    QMenu *editMenu = nullptr;
    QMenu *viewMenu = nullptr;
    QMenu *insertMenu = nullptr;
    QMenu *shapesMenu = nullptr; // “图形”子菜单
    QMenu *objectMenu = nullptr;
    QMenu *textMenu = nullptr;
    QMenu *windowMenu = nullptr;
    QMenu *helpMenu = nullptr;

    // 文件操作动作
    QAction *newFileAction = nullptr;
    QAction *openFileAction = nullptr;
    QAction *saveAction = nullptr;
    QAction *saveAsAction = nullptr;
    QAction *printCenterAction = nullptr;  // 统一打印中心
    QAction *assetBrowserAction = nullptr; // 素材浏览入口
    //QAction *layoutExportAction = nullptr; // 排版导出
    QAction *exitAction = nullptr;

    // 编辑操作动作
    QAction *undoAction = nullptr;
    QAction *redoAction = nullptr;
    QAction *cutAction = nullptr;
    QAction *copyAction = nullptr;
    QAction *pasteAction = nullptr;

    // 工具操作动作
    QAction *selectAction = nullptr;
    QAction *textAction = nullptr;
    QAction *barcodeAction = nullptr;
    QAction *qrCodeAction = nullptr;
    QAction *imageAction = nullptr;
    QAction *lineAction = nullptr;
    QAction *rectangleAction = nullptr;
    QAction *circleAction = nullptr;
    QAction *starAction = nullptr;
    QAction *arrowAction = nullptr;
    QAction *polygonAction = nullptr;

    // 文本格式动作
    QAction *alignLeftAction = nullptr;
    QAction *alignCenterAction = nullptr;
    QAction *alignRightAction = nullptr;

    // 右键菜单动作
    QAction *copyItemAction = nullptr;
    QAction *deleteItemAction = nullptr;
    QAction *rotateItemAction = nullptr;
    QAction *bringToFrontAction = nullptr;
    QAction *sendToBackAction = nullptr;
    QAction *bringForwardAction = nullptr;
    QAction *sendBackwardAction = nullptr;
    QAction *normalizeAction = nullptr; // 规格化（裁剪到标签内）

    // 停靠窗口
    QDockWidget *objectPropertiesDock = nullptr;
    QWidget *propertiesWidget = nullptr;
    QScrollArea *propertiesScrollArea = nullptr;    // 属性面板组
    QGroupBox *labelSizeGroup = nullptr;
    QGroupBox *codeGroup = nullptr;  // 统一的码类设置组
    QGroupBox *textGroup = nullptr;
    QGroupBox *imageGroup = nullptr;  // 图像设置组
    QGroupBox *drawingGroup = nullptr;  // 绘图设置组
    QGroupBox *colorGroup = nullptr;

    DatabasePrintWidget *codeDatabaseWidget = nullptr;
    DatabasePrintWidget *textDatabaseWidget = nullptr;

    // 标签尺寸控件
    QComboBox *paperSizeComboBox = nullptr;
    QLineEdit *widthLineEdit = nullptr;
    QLineEdit *heightLineEdit = nullptr;
    QSpinBox *labelCornerRadiusSpin = nullptr;
    QPushButton *applyButton = nullptr;
    QPushButton *resetButton = nullptr;    // 码类设置控件（条形码和二维码）
    QComboBox *codeTypeComboBox = nullptr;
    QLineEdit *codeDataEdit = nullptr;
    QSpinBox *codeWidthSpin = nullptr;
    QSpinBox *codeHeightSpin = nullptr;    // 文本设置控件
    QLineEdit *textContentEdit = nullptr;
    QFontComboBox *fontComboBox = nullptr;  // 工具栏中的字体组件
    QSpinBox *fontSizeSpinBox = nullptr;    // 工具栏中的字体大小组件
    QFontComboBox *propertiesFontComboBox = nullptr;  // 属性面板中的字体组件
    QSpinBox *propertiesFontSizeSpinBox = nullptr;    // 属性面板中的字体大小组件
    QComboBox *fontStyleCombo = nullptr;
    QSpinBox *letterSpacingSpin = nullptr;
    QPushButton *textColorButton = nullptr;
    QPushButton *textBackgroundColorButton = nullptr;
    QPushButton *textBorderColorButton = nullptr;
    QComboBox *textAlignmentCombo = nullptr;
    QCheckBox *wordWrapCheck = nullptr;
    QCheckBox *autoResizeCheck = nullptr;
    QCheckBox *borderEnabledCheck = nullptr;
    QCheckBox *backgroundEnabledCheck = nullptr;
    QSpinBox *borderWidthSpin = nullptr;

    // 图像设置控件
    QPushButton *loadImageButton = nullptr;
    QSpinBox *imageWidthSpin = nullptr;
    QSpinBox *imageHeightSpin = nullptr;
    QCheckBox *keepAspectRatioCheck = nullptr;
    QSpinBox *imageOpacitySpin = nullptr;

    // 绘图工具设置控件
    QSpinBox *lineWidthSpin = nullptr;
    QPushButton *lineColorButton = nullptr;
    QPushButton *fillColorButton = nullptr;
    QCheckBox *fillEnabledCheck = nullptr;
    QCheckBox *keepCircleCheck = nullptr;
    QDoubleSpinBox *lineAngleSpin = nullptr; // 线条角度（与竖直夹角）
    QCheckBox *lineDashedCheck = nullptr;    // 线条虚线开关
    QSpinBox *cornerRadiusSpin = nullptr;    // 矩形圆角半径

    QSize initialWindowSize;

    // 颜色设置控件
    QPushButton *foregroundColorButton = nullptr;
    QPushButton *backgroundColorButton = nullptr;

    // 状态栏标签
    QLabel *positionLabel = nullptr;
    QLabel *sizeLabel = nullptr;
    QLabel *zoomLabel = nullptr;

    // 文件相关
    QString currentFile;
    QString m_templateSourcePath;
    double m_originalLabelWidthMM = DEFAULT_LABEL_WIDTH;
    double m_originalLabelHeightMM = DEFAULT_LABEL_HEIGHT;
    bool isModified = false;

    // 视图设置
    double zoomFactor = 1.0;
    QPointF lastMousePos;

    // 常量
    const int MIN_ZOOM = 10;  // 10%
    const int MAX_ZOOM = 400; // 400%
    const int DEFAULT_LABEL_WIDTH = 100;  // mm
    const int DEFAULT_LABEL_HEIGHT = 75;  // mm
    static constexpr double MM_TO_PIXELS = 7.559056; // 放大系数 (96/25.4 * 2)    // 动作组
    QActionGroup *toolsGroup = nullptr;

    // 内联文本编辑器
    InlineTextEditor *textEditor = nullptr;

    // 撤销管理器
    UndoManager *undoManager = nullptr;
    
    // 文本编辑撤销支持
    QString m_originalTextBeforeEdit;
    QMap<TextItem*, QString> m_originalTextMap;
    
    // 码类数据编辑撤销支持
    QMap<QRCodeItem*, QString> m_originalQRCodeData;
    QMap<BarcodeItem*, QString> m_originalBarcodeData;
    
    // 粘贴预览相关
    bool m_pasteModeActive = false;
    QGraphicsItem* m_previewItem = nullptr;

    struct DataSourceBinding {
        std::shared_ptr<DataSource> source;
        bool enabled = false;
    };

    QHash<QGraphicsItem*, DataSourceBinding> m_itemDataSources;
        std::unique_ptr<PrintEngine> m_printEngine;
    std::unique_ptr<BatchPrintManager> m_batchPrintManager;

        mutable std::vector<std::unique_ptr<labelelement>> m_elementCache;
signals:
    void zoomFactorChanged(double factor);
};

#endif // MAINWINDOW_H