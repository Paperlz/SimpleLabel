#ifndef LAYOUTEXPORTDIALOG_H
#define LAYOUTEXPORTDIALOG_H

#include <QDialog>
#include <QSizeF>

class QComboBox;
class QRadioButton;
class QSpinBox;
class QDoubleSpinBox;
class QSlider;
class QLabel;
class QPushButton;
class QCheckBox;
class LayoutPreviewWidget;

class LayoutExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LayoutExportDialog(QWidget *parent = nullptr);
    int rows() const;
    int columns() const;
    bool horizontalOrder() const; // true 横向排序, false 纵向排序
    QSizeF pageSizeMM() const; // 基于所选标准尺寸
    bool landscape() const;
    void setLabelSizeMM(const QSizeF &mm);
    // 新增：页边距与间距（单位：mm）
    QMarginsF marginsMM() const;
    double hSpacingMM() const;
    double vSpacingMM() const;
    // 起始偏移（第一页从指定行列开始） 1-based；超出范围时内部自动钳制
    int startRow() const; // 1-based
    int startColumn() const; // 1-based

private slots:
    void updatePreview();
    void syncRowSlider(int value);
    void syncColSlider(int value);
    void syncRowSpin(int value);
    void syncColSpin(int value);
    void recalcGrid();

private:
    void buildUi();
    void populatePageSizes();
    QSizeF selectedPageSize() const;
    QSizeF orientedPageSizeMM() const;

    QComboBox *m_pageSizeCombo{};
    QCheckBox *m_landscapeCheck{};
    QRadioButton *m_horizontalRadio{};
    QRadioButton *m_verticalRadio{};
    QSlider *m_rowSlider{};
    QSlider *m_colSlider{};
    QSpinBox *m_rowSpin{};
    QSpinBox *m_colSpin{};
    // 页边距与间距（mm）
    QDoubleSpinBox *m_marginLeft{};
    QDoubleSpinBox *m_marginRight{};
    QDoubleSpinBox *m_marginTop{};
    QDoubleSpinBox *m_marginBottom{};
    QDoubleSpinBox *m_hSpacing{};
    QDoubleSpinBox *m_vSpacing{};
    QSpinBox *m_startRowSpin{};
    QSpinBox *m_startColSpin{};
    LayoutPreviewWidget *m_preview{};
    QPushButton *m_exportButton{};
    QPushButton *m_cancelButton{};
    QSizeF m_labelMM{0.0, 0.0};
};

// 简单网格预览控件
class LayoutPreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LayoutPreviewWidget(QWidget *parent = nullptr);
    void setGrid(int rows, int cols);
    void setPageSize(const QSizeF &mm, bool landscape);
    void setLabelSizeMM(const QSizeF &mm);
    void setMarginsMM(const QMarginsF &mm);
    void setSpacingMM(double hSpacing, double vSpacing);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    int m_rows{0};
    int m_cols{0};
    QSizeF m_pageMM{210.0,297.0};
    bool m_landscape{false};
    QSizeF m_labelMM{0.0,0.0};
    QMarginsF m_marginsMM{0.0,0.0,0.0,0.0};
    double m_hSpacingMM{0.0};
    double m_vSpacingMM{0.0};
};

#endif // LAYOUTEXPORTDIALOG_H
