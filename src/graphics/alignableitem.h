#ifndef ALIGNABLEITEM_H
#define ALIGNABLEITEM_H

#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QPen>

/**
 * @brief 可对齐图形项的混入接口
 * 
 * 此类提供对齐功能的接口，其他图形项可以继承此接口来获得对齐能力。
 * 支持水平和垂直中心线对齐，当图形项靠近时自动吸附对齐。
 */
class AlignableItem
{
public:
    AlignableItem();
    virtual ~AlignableItem() = default;

    // 纯虚函数，需要在子类中实现
    virtual QGraphicsItem* asGraphicsItem() = 0;
    virtual const QGraphicsItem* asGraphicsItem() const = 0;

    // 对外公开：获取用于对齐/参考线的“内容矩形”的场景坐标矩形
    // 该矩形会将 alignmentRectLocal() 从局部坐标映射到场景坐标，
    // 不包含手柄或额外边距，用于更精准的对齐与标尺参考线。
    QRectF alignmentSceneRect() const;

    // 锁定控制：锁定后禁止拖动（并可用于上层判断）
    void setLocked(bool locked);
    bool isLocked() const { return m_locked; }
    
    // 对齐功能开关：关闭后不进行吸附与不绘制对齐线
    void setAlignmentEnabled(bool enabled);
    bool isAlignmentEnabled() const { return m_alignmentEnabled; }

    // 上层控制：在外部（如场景或选择框）驱动对齐逻辑时调用
    void handleMouseMoveForAlignment();
    void handleMouseReleaseForAlignment();

protected:
    /**
     * @brief 检查并执行对齐操作
     * 
     * 检查当前图形项与场景中其他图形项的对齐关系，
     * 如果距离小于阈值则自动吸附对齐。
     */
    void checkAlignment();

    /**
     * @brief 绘制对齐辅助线
     * 
     * @param painter 绘图对象
     * @param p1 辅助线起点
     * @param p2 辅助线终点
     */
    void drawAlignmentLine(QPainter *painter, const QPointF& p1, const QPointF& p2);

    /**
     * @brief 在鼠标移动时调用此方法
     * 
     * 应该在子类的 mouseMoveEvent 中调用此方法
     */
    /**
     * @brief 在绘制时调用此方法绘制对齐线
     * 
     * 应该在子类的 paint 方法中调用此方法
     * 
     * @param painter 绘图对象
     */
    void paintAlignmentLines(QPainter *painter);

    // 提供用于对齐/辅助线计算的“内容矩形”（局部坐标）
    // 默认返回 boundingRect()；各派生类可重写以避免包含手柄/额外边距
    virtual QRectF alignmentRectLocal() const { return asGraphicsItem() ? asGraphicsItem()->boundingRect() : QRectF(); }
    virtual bool shouldIgnoreForAlignment(QGraphicsItem* candidate) const { Q_UNUSED(candidate); return false; }

private:
    /**
     * @brief 检查水平对齐
     * 
     * @param currentRect 当前图形项的场景矩形
     * @param otherRect 其他图形项的场景矩形
     * @return 如果发生对齐返回true
     */
    bool checkHorizontalAlignment(const QRectF& currentRect, const QRectF& otherRect);    /**
     * @brief 检查垂直对齐
     * 
     * @param currentRect 当前图形项的场景矩形
     * @param otherRect 其他图形项的场景矩形
     * @return 如果发生对齐返回true
     */
    bool checkVerticalAlignment(const QRectF& currentRect, const QRectF& otherRect);
    // 边缘对齐由 checkHorizontalAlignment / checkVerticalAlignment 内部处理

    /**
     * @brief 检查与标签的水平对齐
     * 
     * @param currentRect 当前图形项的场景矩形
     * @param labelRect 标签的场景矩形
     * @return 如果发生对齐返回true
     */
    bool checkLabelHorizontalAlignment(const QRectF& currentRect, const QRectF& labelRect);

    /**
     * @brief 检查与标签的垂直对齐
     * 
     * @param currentRect 当前图形项的场景矩形
     * @param labelRect 标签的场景矩形
     * @return 如果发生对齐返回true
     */
    bool checkLabelVerticalAlignment(const QRectF& currentRect, const QRectF& labelRect);

    /**
     * @brief 获取图形项在场景中的矩形
     * 
     * @param item 图形项
     * @return 场景中的矩形
     */
    QRectF getSceneRect(QGraphicsItem* item) const;

private:
    bool m_showAlignmentLine;     // 是否显示对齐线
    QPointF m_alignmentP1;        // 对齐线起点
    QPointF m_alignmentP2;        // 对齐线终点
    
    // 支持多条对齐线
    bool m_showHorizontalLine;    // 是否显示水平对齐线
    bool m_showVerticalLine;      // 是否显示垂直对齐线
    QPointF m_horizontalP1;       // 水平对齐线起点
    QPointF m_horizontalP2;       // 水平对齐线终点
    QPointF m_verticalP1;         // 垂直对齐线起点
    QPointF m_verticalP2;         // 垂直对齐线终点
    
    // 对齐阈值（像素）: 减小以降低吸附“力度”。
    // 如需进一步配置，可将其改为静态可变成员并提供 setter。
    static constexpr qreal ALIGNMENT_THRESHOLD = 5.0;  // 原为 10.0

    // 锁定状态
    bool m_locked = false;
    
    // 是否启用对齐功能
    bool m_alignmentEnabled = true;
};

#endif // ALIGNABLEITEM_H
