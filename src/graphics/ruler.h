#ifndef RULER_H
#define RULER_H

#include <QWidget>

class QGraphicsView;

class Ruler : public QWidget
{
    Q_OBJECT
public:
    explicit Ruler(Qt::Orientation orientation, QGraphicsView *view);

    // 设置标尺长度（毫米）
    void setLength(int length);

    // 设置标尺缩放比例
    void setScale(qreal scale);

    // 设置是否显示毫米
    void setMetricMode(bool metric);

    // 根据视图更新位置和大小
    void updatePositionAndSize();

    // 设置用于辅助显示的两条参考线（基于场景坐标的边界矩形）
    // 水平标尺使用 rect.left()/rect.right()，垂直标尺使用 rect.top()/rect.bottom()
    void setGuidesFromSceneRect(const QRectF& sceneRect);

    // 清除参考线
    void clearGuides();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Qt::Orientation m_orientation;
    int m_length;          // 标尺长度（mm）
    qreal m_scale;         // 标尺比例尺（像素/mm）
    bool m_metric;         // 是否显示公制单位
    QGraphicsView *m_view; // 关联的视图

    // 引导线（场景坐标）。水平标尺使用X坐标，垂直标尺使用Y坐标
    bool m_hasGuides = false;
    qreal m_guide1 = 0.0;  // left/top 边
    qreal m_guide2 = 0.0;  // right/bottom 边

    void drawHorizontalRuler(QPainter &painter);
    void drawVerticalRuler(QPainter &painter);

    // 计算视图到场景的坐标映射
    QPointF mapViewToScene(const QPoint &viewPoint);
};

#endif // RULER_H