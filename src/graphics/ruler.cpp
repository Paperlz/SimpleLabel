#include "ruler.h"
#include <QPainter>
#include <QPaintEvent>
#include <QGraphicsView>
#include <QScrollBar>
#include <QtMath>
#include <cmath>
#include <array>

namespace {
constexpr int kHorizontalRulerThickness = 30;
constexpr int kVerticalRulerThickness = 33;

int positiveModulo(int value, int modulus)
{
    if (modulus <= 0) {
        return 0;
    }
    int remainder = value % modulus;
    if (remainder < 0) {
        remainder += modulus;
    }
    return remainder;
}
}

Ruler::Ruler(Qt::Orientation orientation, QGraphicsView *view)
    : QWidget(view)
    , m_orientation(orientation)
    , m_length(100)
    , m_scale(7.559056) // 默认比例尺 (96/25.4 * 2)
    , m_metric(true)
    , m_view(view)
{
    // 设置透明背景
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setMouseTracking(true);

    // 添加事件过滤器以监听视图变化
    view->viewport()->installEventFilter(this);

    // 初始化位置和大小
    updatePositionAndSize();
}

void Ruler::setGuidesFromSceneRect(const QRectF &sceneRect)
{
    if (m_orientation == Qt::Horizontal) {
        m_guide1 = sceneRect.left();
        m_guide2 = sceneRect.right();
    } else {
        m_guide1 = sceneRect.top();
        m_guide2 = sceneRect.bottom();
    }
    m_hasGuides = true;
    update();
}

void Ruler::clearGuides()
{
    if (m_hasGuides) {
        m_hasGuides = false;
        update();
    }
}

void Ruler::setLength(int length)
{
    if (m_length != length) {
        m_length = length;
        update();
    }
}

void Ruler::setScale(qreal scale)
{
    if (m_scale != scale) {
        m_scale = scale;
        update();
    }
}

void Ruler::setMetricMode(bool metric)
{
    if (m_metric != metric) {
        m_metric = metric;
        update();
    }
}

void Ruler::updatePositionAndSize()
{
    // 获取视图的可见区域
    QRect viewportRect = m_view->viewport()->rect();

    if (m_orientation == Qt::Horizontal) {
        // 水平标尺
        setFixedHeight(kHorizontalRulerThickness);
        setFixedWidth(viewportRect.width());
        move(viewportRect.left(), 0);
    } else {
        // 垂直标尺
        setFixedWidth(kVerticalRulerThickness);
        setFixedHeight(viewportRect.height());
        move(0, viewportRect.top());
    }
}

bool Ruler::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view->viewport() &&
        (event->type() == QEvent::Resize ||
         event->type() == QEvent::Paint)) {
        updatePositionAndSize();
    }
    return false;
}

void Ruler::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // 绘制背景
    painter.fillRect(rect(), QColor("#F0F0F0"));

    // 绘制标尺
    if (m_orientation == Qt::Horizontal) {
        drawHorizontalRuler(painter);
    } else {
        drawVerticalRuler(painter);
    }

    // 绘制边框
    painter.setPen(QPen(Qt::gray));
    painter.drawRect(0, 0, width() - 1, height() - 1);
}

void Ruler::drawHorizontalRuler(QPainter &painter)
{
    // 设置刻度颜色和画笔
    QPen pen(Qt::black);
    pen.setWidth(1);
    painter.setPen(pen);

    // 获取字体度量以计算文本宽度
    QFontMetrics fm(painter.font());

    // 计算起始和结束点（场景坐标）
    QPointF sceneStart = mapViewToScene(QPoint(0, 0));
    QPointF sceneEnd = mapViewToScene(QPoint(width(), 0));

    const qreal zoom = std::abs(m_view->transform().m11());
    const qreal pixelsPerMm = m_scale * qMax<qreal>(zoom, 0.0001);
    constexpr qreal kMinMajorSpacingPx = 70.0;

    // 根据当前缩放选择合适的主刻度间隔，保证数字之间像素距离足够
    const std::array<int, 9> majorCandidates = {1, 2, 5, 10, 20, 50, 100, 200, 500};
    int majorStep = majorCandidates.back();
    for (int candidate : majorCandidates) {
        if (candidate * pixelsPerMm >= kMinMajorSpacingPx) {
            majorStep = candidate;
            break;
        }
    }

    int mediumStep = 0;
    if (majorStep >= 10) {
        mediumStep = majorStep / 5;
    } else if (majorStep >= 5) {
        mediumStep = qMax(1, majorStep / 5);
    } else if (majorStep >= 2) {
        mediumStep = majorStep / 2;
    }

    int startMM = static_cast<int>(std::floor(sceneStart.x() / m_scale));
    int endMM = static_cast<int>(std::ceil(sceneEnd.x() / m_scale));

    for (int mm = startMM; mm <= endMM; ++mm) {
        double scenePos = mm * m_scale;
        QPointF viewPos = m_view->mapFromScene(scenePos, 0);
        int x = qRound(viewPos.x());

        if (x < 0 || x > width()) {
            continue;
        }

        const bool isMajor = positiveModulo(mm, majorStep) == 0;
        const bool isMedium = !isMajor && mediumStep > 0 && positiveModulo(mm, mediumStep) == 0;

        if (isMajor) {
            painter.drawLine(x, height() - 1, x, height() - 10);

            QString num = QString::number(mm);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
            int textWidth = fm.horizontalAdvance(num);
#else
            int textWidth = fm.width(num);
#endif
            QRectF textRect(x - textWidth / 2.0, 2.0, textWidth, height() - 12.0);
            painter.drawText(textRect, Qt::AlignCenter, num);
        } else if (isMedium) {
            painter.drawLine(x, height() - 1, x, height() - 7);
        } else {
            painter.drawLine(x, height() - 1, x, height() - 4);
        }
    }

    // 绘制两条参考虚线（选中项左右边）
    if (m_hasGuides) {
        QPen guidePen(QColor(0, 120, 215)); // Windows 选中蓝
        guidePen.setStyle(Qt::DashLine);
        guidePen.setWidth(1);
        painter.setPen(guidePen);

        int x1 = qRound(static_cast<double>(m_view->mapFromScene(QPointF(m_guide1, 0)).x()));
        if (x1 >= 0 && x1 <= width()) {
            painter.drawLine(x1, height(), x1, 0);
        }

        int x2 = qRound(static_cast<double>(m_view->mapFromScene(QPointF(m_guide2, 0)).x()));
        if (x2 >= 0 && x2 <= width()) {
            painter.drawLine(x2, height(), x2, 0);
        }
    }
}

void Ruler::drawVerticalRuler(QPainter &painter)
{
    // 设置刻度颜色和画笔
    QPen pen(Qt::black);
    pen.setWidth(1);
    painter.setPen(pen);

    // 获取字体度量以计算文本宽度
    QFontMetrics fm(painter.font());

    QPointF sceneStart = mapViewToScene(QPoint(0, 0));
    QPointF sceneEnd = mapViewToScene(QPoint(0, height()));

    const qreal zoom = std::abs(m_view->transform().m22());
    const qreal pixelsPerMm = m_scale * qMax<qreal>(zoom, 0.0001);
    constexpr qreal kMinMajorSpacingPx = 70.0;

    // 根据当前缩放选择合适的主刻度间隔，保证数字之间像素距离足够
    const std::array<int, 9> majorCandidates = {1, 2, 5, 10, 20, 50, 100, 200, 500};
    int majorStep = majorCandidates.back();
    for (int candidate : majorCandidates) {
        if (candidate * pixelsPerMm >= kMinMajorSpacingPx) {
            majorStep = candidate;
            break;
        }
    }

    int mediumStep = 0;
    if (majorStep >= 10) {
        mediumStep = majorStep / 5;
    } else if (majorStep >= 5) {
        mediumStep = qMax(1, majorStep / 5);
    } else if (majorStep >= 2) {
        mediumStep = majorStep / 2;
    }

    int startMM = static_cast<int>(std::floor(sceneStart.y() / m_scale));
    int endMM = static_cast<int>(std::ceil(sceneEnd.y() / m_scale));

    for (int mm = startMM; mm <= endMM; ++mm) {
        double scenePos = mm * m_scale;
        QPointF viewPos = m_view->mapFromScene(0, scenePos);
        int y = qRound(viewPos.y());

        if (y < 0 || y > height()) {
            continue;
        }

        const bool isMajor = positiveModulo(mm, majorStep) == 0;
        const bool isMedium = !isMajor && mediumStep > 0 && positiveModulo(mm, mediumStep) == 0;

        if (isMajor) {
            painter.drawLine(width() - 1, y, width() - 10, y);

            painter.save();
            painter.translate(width() - 26, y);
            painter.rotate(-90);

            QString num = QString::number(mm);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
            int textWidth = fm.horizontalAdvance(num);
#else
            int textWidth = fm.width(num);
#endif
            QRectF textRect(-textWidth / 2.0, -fm.height() / 2.0, textWidth, fm.height());
            painter.drawText(textRect, Qt::AlignCenter, num);
            painter.restore();
        } else if (isMedium) {
            painter.drawLine(width() - 1, y, width() - 7, y);
        } else {
            painter.drawLine(width() - 1, y, width() - 4, y);
        }
    }

    if (m_hasGuides) {
        QPen guidePen(QColor(0, 120, 215));
        guidePen.setStyle(Qt::DashLine);
        guidePen.setWidth(1);
        painter.setPen(guidePen);

        int y1 = qRound(static_cast<double>(m_view->mapFromScene(QPointF(0, m_guide1)).y()));
        if (y1 >= 0 && y1 <= height()) {
            painter.drawLine(0, y1, width(), y1);
        }

        int y2 = qRound(static_cast<double>(m_view->mapFromScene(QPointF(0, m_guide2)).y()));
        if (y2 >= 0 && y2 <= height()) {
            painter.drawLine(0, y2, width(), y2);
        }
    }
}

QPointF Ruler::mapViewToScene(const QPoint &viewPoint)
{
    return m_view->mapToScene(viewPoint);
}