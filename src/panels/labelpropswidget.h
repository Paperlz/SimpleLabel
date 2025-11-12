#ifndef LABELPROPSWIDGET_H
#define LABELPROPSWIDGET_H

#include <QGroupBox>

class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

class LabelPropsWidget : public QGroupBox {
    Q_OBJECT
public:
    explicit LabelPropsWidget(QWidget* parent = nullptr);

    // Getters for controls
    QComboBox* paperSizeCombo() const { return m_paperSizeCombo; }
    QLineEdit* widthEdit() const { return m_widthEdit; }
    QLineEdit* heightEdit() const { return m_heightEdit; }
    QPushButton* applyButton() const { return m_applyBtn; }
    QPushButton* resetButton() const { return m_resetBtn; }
    QSpinBox* cornerRadiusSpin() const { return m_cornerRadiusSpin; }

private:
    QComboBox* m_paperSizeCombo = nullptr;
    QLineEdit* m_widthEdit = nullptr;
    QLineEdit* m_heightEdit = nullptr;
    QPushButton* m_applyBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
    QSpinBox* m_cornerRadiusSpin = nullptr;
};

#endif // LABELPROPSWIDGET_H
