#include "layoutexportdialog.h"
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QPainter>
#include <QPaintEvent>

LayoutPreviewWidget::LayoutPreviewWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(180,240);
}

void LayoutPreviewWidget::setGrid(int rows, int cols)
{
    m_rows = rows; m_cols = cols; update();
}

void LayoutPreviewWidget::setPageSize(const QSizeF &mm, bool landscape)
{
    m_pageMM = mm; m_landscape = landscape; update();
}

void LayoutPreviewWidget::setLabelSizeMM(const QSizeF &mm)
{
    m_labelMM = mm; update();
}

void LayoutPreviewWidget::setMarginsMM(const QMarginsF &mm)
{
    m_marginsMM = mm; update();
}

void LayoutPreviewWidget::setSpacingMM(double hSpacing, double vSpacing)
{
    m_hSpacingMM = hSpacing; m_vSpacingMM = vSpacing; update();
}

void LayoutPreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing,false);
    p.fillRect(rect(), palette().base());
    QRectF pageRect = QRectF(10,10,width()-20,height()-20);
    // 外框
    p.setPen(QPen(Qt::gray,1));
    p.drawRect(pageRect);

    // 处理横向/纵向方向尺寸
    const QSizeF orientedMM = m_landscape ? QSizeF(m_pageMM.height(), m_pageMM.width()) : m_pageMM;
    if (orientedMM.width() <= 0.0 || orientedMM.height() <= 0.0) return;

    // 将mm映射到预览坐标
    const qreal sx = pageRect.width() / orientedMM.width();
    const qreal sy = pageRect.height() / orientedMM.height();

    // 绘制页边距区域
    QRectF innerRect = pageRect.adjusted(m_marginsMM.left()*sx,
                                         m_marginsMM.top()*sy,
                                         -m_marginsMM.right()*sx,
                                         -m_marginsMM.bottom()*sy);
    if (innerRect.isValid()) {
        // 阴影显示边距
        QColor shade(0,0,0,20);
        p.fillRect(QRectF(pageRect.left(), pageRect.top(), pageRect.width(), innerRect.top()-pageRect.top()), shade);
        p.fillRect(QRectF(pageRect.left(), innerRect.bottom(), pageRect.width(), pageRect.bottom()-innerRect.bottom()), shade);
        p.fillRect(QRectF(pageRect.left(), innerRect.top(), innerRect.left()-pageRect.left(), innerRect.height()), shade);
        p.fillRect(QRectF(innerRect.right(), innerRect.top(), pageRect.right()-innerRect.right(), innerRect.height()), shade);
        p.setPen(QPen(QColor(180,180,180),1,Qt::DashLine));
        p.drawRect(innerRect);
    } else {
        innerRect = pageRect;
    }

    // 如果没有标签尺寸或行列，则不画网格
    if (m_rows<=0 || m_cols<=0 || m_labelMM.width()<=0.0 || m_labelMM.height()<=0.0) return;

    // 以真实标签尺寸和间距绘制“块”
    const qreal tileW = m_labelMM.width() * sx;
    const qreal tileH = m_labelMM.height() * sy;
    const qreal gapX = qMax<qreal>(0.0, m_hSpacingMM * sx);
    const qreal gapY = qMax<qreal>(0.0, m_vSpacingMM * sy);

    // 计算总占用，尽量在innerRect中居上左对齐展示
    const qreal totalW = m_cols*tileW + (m_cols-1)*gapX;
    const qreal totalH = m_rows*tileH + (m_rows-1)*gapY;
    QRectF tilesRect(innerRect.topLeft(), QSizeF(qMin(innerRect.width(), totalW), qMin(innerRect.height(), totalH)));

    p.setPen(QPen(QColor(180,180,180),0));
    for (int r=0; r<m_rows; ++r) {
        for (int c=0; c<m_cols; ++c) {
            const qreal x = tilesRect.left() + c*(tileW+gapX);
            const qreal y = tilesRect.top() + r*(tileH+gapY);
            QRectF rect(x,y,tileW,tileH);
            if (!innerRect.contains(rect)) continue;
            p.drawRect(rect);
        }
    }
}

LayoutExportDialog::LayoutExportDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("排版导出"));
    buildUi();
    populatePageSizes();
    updatePreview();
}

void LayoutExportDialog::buildUi()
{
    m_pageSizeCombo = new QComboBox(this);
    m_landscapeCheck = new QCheckBox(tr("横向"),this);
    m_horizontalRadio = new QRadioButton(tr("横向排序"),this);
    m_verticalRadio = new QRadioButton(tr("竖向排序"),this);
    m_horizontalRadio->setChecked(true);

    m_rowSlider = new QSlider(Qt::Horizontal,this);
    m_rowSlider->setRange(1,20); m_rowSlider->setValue(5);
    m_colSlider = new QSlider(Qt::Horizontal,this);
    m_colSlider->setRange(1,20); m_colSlider->setValue(4);

    m_rowSpin = new QSpinBox(this); m_rowSpin->setRange(1,20); m_rowSpin->setValue(5);
    m_colSpin = new QSpinBox(this); m_colSpin->setRange(1,20); m_colSpin->setValue(4);

    // 页边距与间距（mm）
    auto makeMMSpin = [this](double min, double max, double val){
        QDoubleSpinBox *s = new QDoubleSpinBox(this);
        s->setRange(min, max);
        s->setDecimals(1);
        s->setSingleStep(1.0);
        s->setValue(val);
        s->setSuffix(" mm");
        return s;
    };
    m_marginLeft = makeMMSpin(0.0, 50.0, 0.0);
    m_marginRight = makeMMSpin(0.0, 50.0, 0.0);
    m_marginTop = makeMMSpin(0.0, 50.0, 0.0);
    m_marginBottom = makeMMSpin(0.0, 50.0, 0.0);
    m_hSpacing = makeMMSpin(0.0, 50.0, 0.0);
    m_vSpacing = makeMMSpin(0.0, 50.0, 0.0);

    m_preview = new LayoutPreviewWidget(this);
    m_exportButton = new QPushButton(tr("立即导出"),this);
    m_cancelButton = new QPushButton(tr("取消"),this);
    connect(m_cancelButton,&QPushButton::clicked,this,&QDialog::reject);
    connect(m_exportButton,&QPushButton::clicked,this,&QDialog::accept);

    // sync
    connect(m_rowSlider,&QSlider::valueChanged,this,&LayoutExportDialog::syncRowSlider);
    connect(m_colSlider,&QSlider::valueChanged,this,&LayoutExportDialog::syncColSlider);
    connect(m_rowSpin,qOverload<int>(&QSpinBox::valueChanged),this,&LayoutExportDialog::syncRowSpin);
    connect(m_colSpin,qOverload<int>(&QSpinBox::valueChanged),this,&LayoutExportDialog::syncColSpin);
    connect(m_pageSizeCombo,qOverload<int>(&QComboBox::currentIndexChanged),this,&LayoutExportDialog::recalcGrid);
    connect(m_landscapeCheck,&QCheckBox::toggled,this,&LayoutExportDialog::recalcGrid);
    connect(m_horizontalRadio,&QRadioButton::toggled,this,&LayoutExportDialog::updatePreview);
    // 边距/间距联动
    auto connectMM = [this](QDoubleSpinBox* s){ connect(s, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &LayoutExportDialog::recalcGrid); };
    connectMM(m_marginLeft); connectMM(m_marginRight); connectMM(m_marginTop); connectMM(m_marginBottom);
    connectMM(m_hSpacing); connectMM(m_vSpacing);

    QGroupBox *orderGroup = new QGroupBox(tr("排序方式"),this);
    QVBoxLayout *orderLay = new QVBoxLayout(orderGroup);
    orderLay->addWidget(m_horizontalRadio);
    orderLay->addWidget(m_verticalRadio);

    QGridLayout *gridCfg = new QGridLayout();
    gridCfg->addWidget(new QLabel(tr("行数:"),this),0,0);
    gridCfg->addWidget(m_rowSlider,0,1);
    gridCfg->addWidget(m_rowSpin,0,2);
    gridCfg->addWidget(new QLabel(tr("列数:"),this),1,0);
    gridCfg->addWidget(m_colSlider,1,1);
    gridCfg->addWidget(m_colSpin,1,2);

    QGroupBox *gridGroup = new QGroupBox(tr("网格配置"),this);
    gridGroup->setLayout(gridCfg);

    // 页边距与间距配置
    QGroupBox *marginGroup = new QGroupBox(tr("页边距与间距"), this);
    QGridLayout *marginLay = new QGridLayout(marginGroup);
    int row = 0;
    marginLay->addWidget(new QLabel(tr("左边距:"),this), row, 0); marginLay->addWidget(m_marginLeft, row, 1); ++row;
    marginLay->addWidget(new QLabel(tr("右边距:"),this), row, 0); marginLay->addWidget(m_marginRight, row, 1); ++row;
    marginLay->addWidget(new QLabel(tr("上边距:"),this), row, 0); marginLay->addWidget(m_marginTop, row, 1); ++row;
    marginLay->addWidget(new QLabel(tr("下边距:"),this), row, 0); marginLay->addWidget(m_marginBottom, row, 1); ++row;
    marginLay->addWidget(new QLabel(tr("水平间距:"),this), row, 0); marginLay->addWidget(m_hSpacing, row, 1); ++row;
    marginLay->addWidget(new QLabel(tr("垂直间距:"),this), row, 0); marginLay->addWidget(m_vSpacing, row, 1);

    // 起始偏移组（仅作用于第一页）
    QGroupBox *startGroup = new QGroupBox(tr("起始偏移(第一页)"), this);
    QGridLayout *startLay = new QGridLayout(startGroup);
    m_startRowSpin = new QSpinBox(this);
    m_startColSpin = new QSpinBox(this);
    m_startRowSpin->setRange(1, 999); // 真实范围在 recalcGrid 中重设
    m_startColSpin->setRange(1, 999);
    m_startRowSpin->setValue(1);
    m_startColSpin->setValue(1);
    startLay->addWidget(new QLabel(tr("起始行:"),this),0,0); startLay->addWidget(m_startRowSpin,0,1);
    startLay->addWidget(new QLabel(tr("起始列:"),this),1,0); startLay->addWidget(m_startColSpin,1,1);
    connect(m_startRowSpin,qOverload<int>(&QSpinBox::valueChanged),this,&LayoutExportDialog::updatePreview);
    connect(m_startColSpin,qOverload<int>(&QSpinBox::valueChanged),this,&LayoutExportDialog::updatePreview);

    QHBoxLayout *topLine = new QHBoxLayout();
    topLine->addWidget(new QLabel(tr("页面大小:"),this));
    topLine->addWidget(m_pageSizeCombo);
    topLine->addWidget(m_landscapeCheck);
    topLine->addStretch();

    QHBoxLayout *buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->addWidget(m_exportButton);
    buttons->addWidget(m_cancelButton);

    QVBoxLayout *right = new QVBoxLayout();
    right->addLayout(topLine);
    right->addWidget(orderGroup);
    right->addWidget(gridGroup);
    right->addWidget(marginGroup);
    right->addWidget(startGroup);
    right->addStretch();
    right->addLayout(buttons);

    QHBoxLayout *mainLay = new QHBoxLayout(this);
    mainLay->addWidget(m_preview,1);
    mainLay->addLayout(right,0);
    setLayout(mainLay);
    resize(780,480);
}

void LayoutExportDialog::setLabelSizeMM(const QSizeF &mm)
{
    m_labelMM = mm;
    recalcGrid();
}

QSizeF LayoutExportDialog::orientedPageSizeMM() const
{
    QSizeF base = selectedPageSize();
    if (landscape()) {
        return QSizeF(base.height(), base.width());
    }
    return base;
}

void LayoutExportDialog::populatePageSizes()
{
    // 简化，只添加常用尺寸
    m_pageSizeCombo->addItem("A4 (210×297mm)", QSizeF(210.0,297.0));
    m_pageSizeCombo->addItem("A5 (148×210mm)", QSizeF(148.0,210.0));
    m_pageSizeCombo->addItem("Letter (216×279mm)", QSizeF(216.0,279.0));
}

QSizeF LayoutExportDialog::selectedPageSize() const
{
    QVariant v = m_pageSizeCombo->currentData();
    if (v.isValid()) return v.toSizeF();
    return QSizeF(210.0,297.0);
}

int LayoutExportDialog::rows() const { return m_rowSpin->value(); }
int LayoutExportDialog::columns() const { return m_colSpin->value(); }
bool LayoutExportDialog::horizontalOrder() const { return m_horizontalRadio->isChecked(); }
bool LayoutExportDialog::landscape() const { return m_landscapeCheck->isChecked(); }
QSizeF LayoutExportDialog::pageSizeMM() const { return selectedPageSize(); }

QMarginsF LayoutExportDialog::marginsMM() const {
    return QMarginsF(m_marginLeft ? m_marginLeft->value() : 0.0,
                     m_marginTop ? m_marginTop->value() : 0.0,
                     m_marginRight ? m_marginRight->value() : 0.0,
                     m_marginBottom ? m_marginBottom->value() : 0.0);
}
double LayoutExportDialog::hSpacingMM() const { return m_hSpacing ? m_hSpacing->value() : 0.0; }
double LayoutExportDialog::vSpacingMM() const { return m_vSpacing ? m_vSpacing->value() : 0.0; }
int LayoutExportDialog::startRow() const { return m_startRowSpin ? m_startRowSpin->value() : 1; }
int LayoutExportDialog::startColumn() const { return m_startColSpin ? m_startColSpin->value() : 1; }

void LayoutExportDialog::updatePreview()
{
    QSizeF mm = selectedPageSize();
    m_preview->setPageSize(mm, landscape());
    m_preview->setLabelSizeMM(m_labelMM);
    m_preview->setMarginsMM(marginsMM());
    m_preview->setSpacingMM(hSpacingMM(), vSpacingMM());
    m_preview->setGrid(rows(), columns());
}

void LayoutExportDialog::syncRowSlider(int value){ m_rowSpin->setValue(value); updatePreview(); }
void LayoutExportDialog::syncColSlider(int value){ m_colSpin->setValue(value); updatePreview(); }
void LayoutExportDialog::syncRowSpin(int value){ m_rowSlider->setValue(value); updatePreview(); }
void LayoutExportDialog::syncColSpin(int value){ m_colSlider->setValue(value); updatePreview(); }

void LayoutExportDialog::recalcGrid()
{
    // 如果没有标签尺寸，则保持用户输入
    if (m_labelMM.width() <= 0.0 || m_labelMM.height() <= 0.0) {
        updatePreview();
        return;
    }

    const QSizeF pageMM = orientedPageSizeMM();
    if (pageMM.width() <= 0.0 || pageMM.height() <= 0.0) {
        updatePreview();
        return;
    }

    // 考虑页边距与间距的最大行列数
    const QMarginsF m = marginsMM();
    const double innerW = qMax(0.0, pageMM.width() - (m.left() + m.right()));
    const double innerH = qMax(0.0, pageMM.height() - (m.top() + m.bottom()));
    const double gapX = qMax(0.0, hSpacingMM());
    const double gapY = qMax(0.0, vSpacingMM());
    // N*W + (N-1)*gap <= inner -> N <= floor((inner + gap)/(W + gap))
    int maxCols = (m_labelMM.width() > 0.0)
        ? static_cast<int>(std::floor((innerW + gapX) / (m_labelMM.width() + gapX))) : 0;
    int maxRows = (m_labelMM.height() > 0.0)
        ? static_cast<int>(std::floor((innerH + gapY) / (m_labelMM.height() + gapY))) : 0;
    maxCols = std::max(1, maxCols);
    maxRows = std::max(1, maxRows);

    // 若当前用户设定超过最大值，自动截断；若为初始状态使用最大值填充
    if (m_colSpin->value() > maxCols) {
        m_colSpin->setValue(maxCols);
    }
    if (m_rowSpin->value() > maxRows) {
        m_rowSpin->setValue(maxRows);
    }
    if (m_colSlider->value() != m_colSpin->value()) {
        m_colSlider->setValue(m_colSpin->value());
    }
    if (m_rowSlider->value() != m_rowSpin->value()) {
        m_rowSlider->setValue(m_rowSpin->value());
    }

    // 更新滑块范围，使用户不能超过合理上限
    m_colSlider->setMaximum(maxCols);
    m_rowSlider->setMaximum(maxRows);
    m_colSpin->setMaximum(maxCols);
    m_rowSpin->setMaximum(maxRows);

    // 起始偏移最大范围同步并钳制
    if (m_startRowSpin) {
        m_startRowSpin->setMaximum(maxRows);
        if (m_startRowSpin->value() > maxRows) m_startRowSpin->setValue(maxRows);
    }
    if (m_startColSpin) {
        m_startColSpin->setMaximum(maxCols);
        if (m_startColSpin->value() > maxCols) m_startColSpin->setValue(maxCols);
    }

    updatePreview();
}
