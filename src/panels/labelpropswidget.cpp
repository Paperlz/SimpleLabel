#include "labelpropswidget.h"
#include <QFormLayout>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

LabelPropsWidget::LabelPropsWidget(QWidget* parent)
    : QGroupBox(tr("标签尺寸"), parent)
{
    auto* layout = new QFormLayout(this);
    layout->setSpacing(8);

    m_paperSizeCombo = new QComboBox(this);
    m_paperSizeCombo->addItems({
        tr("自定义尺寸"),
        tr("10mm × 20mm (小标签)"),
        tr("30mm × 50mm (中标签)"),
        tr("50mm × 70mm (大标签)"),
        tr("70mm × 100mm (特大标签)")
    });

    auto* sizeRow = new QWidget(this);
    auto* sizeLayout = new QHBoxLayout(sizeRow);
    sizeLayout->setSpacing(5);
    sizeLayout->setContentsMargins(0,0,0,0);
    m_widthEdit = new QLineEdit(sizeRow);
    m_heightEdit = new QLineEdit(sizeRow);
    m_widthEdit->setFixedWidth(60);
    m_heightEdit->setFixedWidth(60);
    auto* multiplyLabel = new QLabel("×", sizeRow);
    multiplyLabel->setAlignment(Qt::AlignCenter);
    auto* unitLabel = new QLabel("mm", sizeRow);
    sizeLayout->addWidget(m_widthEdit);
    sizeLayout->addWidget(multiplyLabel);
    sizeLayout->addWidget(m_heightEdit);
    sizeLayout->addWidget(unitLabel);
    sizeLayout->addStretch();

    auto* btnRow = new QWidget(this);
    auto* btnLayout = new QHBoxLayout(btnRow);
    btnLayout->setContentsMargins(0,0,0,0);
    m_applyBtn = new QPushButton(tr("应用"), btnRow);
    m_resetBtn = new QPushButton(tr("重置"), btnRow);
    btnLayout->addWidget(m_applyBtn);
    btnLayout->addWidget(m_resetBtn);
    btnLayout->addStretch();

    layout->addRow(tr("预设尺寸:"), m_paperSizeCombo);
    layout->addRow(tr("自定义尺寸:"), sizeRow);
    // 圆角半径
    m_cornerRadiusSpin = new QSpinBox(this);
    m_cornerRadiusSpin->setRange(0, 500); // 以像素为单位，稍后根据标签尺寸限制
    m_cornerRadiusSpin->setValue(0);
    m_cornerRadiusSpin->setSuffix(" px");

    layout->addRow(tr("圆角半径:"), m_cornerRadiusSpin);
    layout->addRow(btnRow);
}
