#include "assetbrowserdialog.h"

#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QBrush>
#include <QPen>
#include <QIcon>
#include <QtMath>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QImageReader>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QSet>

static QPixmap withDevicePixelRatio(const QPixmap& src)
{
    QPixmap pm = src;
    pm.setDevicePixelRatio(1.0);
    return pm;
}

AssetBrowserDialog::AssetBrowserDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("素材库"));
    resize(820, 520);
    buildUi();
    // 外部素材目录：应用程序目录下的 assets 子目录
    m_assetsDir = QCoreApplication::applicationDirPath() + "/assets";
    QDir().mkpath(m_assetsDir);
    // 仅使用外部素材，移除内置素材
    populateExternalAssets();
    updateCategories();
    refreshList();
}

void AssetBrowserDialog::buildUi()
{
    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->addItem(tr("全部"), QString());
    connect(m_categoryCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &AssetBrowserDialog::onCategoryChanged);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("搜索素材…"));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &AssetBrowserDialog::onFilterTextChanged);

    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(128,128));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setGridSize(QSize(140, 160)); // 留出文字与统一单元格尺寸
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(8);
    connect(m_list, &QListWidget::itemActivated, this, &AssetBrowserDialog::onItemActivated);

    m_addButton = new QPushButton(tr("添加到画布"), this);
    m_cancelButton = new QPushButton(tr("取消"), this);
    m_openFolderButton = new QPushButton(tr("打开素材文件夹"), this);
    m_refreshButton = new QPushButton(tr("刷新"), this);
    connect(m_addButton, &QPushButton::clicked, this, &AssetBrowserDialog::onAccept);
    connect(m_cancelButton, &QPushButton::clicked, this, &AssetBrowserDialog::reject);
    connect(m_openFolderButton, &QPushButton::clicked, this, [this]{
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_assetsDir));
    });
    connect(m_refreshButton, &QPushButton::clicked, this, [this]{
        populateExternalAssets();
        updateCategories();
        refreshList();
    });

    QHBoxLayout* top = new QHBoxLayout();
    top->addWidget(new QLabel(tr("分类:"), this));
    top->addWidget(m_categoryCombo, 0);
    top->addSpacing(12);
    top->addWidget(new QLabel(tr("搜索:"), this));
    top->addWidget(m_searchEdit, 1);

    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->addWidget(m_openFolderButton);
    buttons->addWidget(m_refreshButton);
    buttons->addWidget(m_addButton);
    buttons->addWidget(m_cancelButton);

    QVBoxLayout* lay = new QVBoxLayout(this);
    lay->addLayout(top);
    lay->addWidget(m_list, 1);
    lay->addLayout(buttons);

    setLayout(lay);
}

static QPixmap genCanvas(int w, int h, std::function<void(QPainter&)> draw)
{
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    draw(p);
    p.end();
    return withDevicePixelRatio(pm);
}

QPixmap AssetBrowserDialog::genWarningIcon(const QString& text)
{
    const int W = 512, H = 512;
    return genCanvas(W, H, [=](QPainter& p){
        p.setPen(Qt::NoPen);
        QPolygonF tri;
        tri << QPointF(W/2.0, H*0.08) << QPointF(W*0.06, H*0.92) << QPointF(W*0.94, H*0.92);
        p.setBrush(QColor(255, 205, 0));
        p.drawPolygon(tri);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(60,60,60), 12));
        p.drawPolygon(tri);
        // exclamation
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(60,60,60));
        QRectF bar(W*0.48, H*0.28, W*0.04, H*0.40);
        QRectF dot(W*0.46, H*0.72, W*0.08, W*0.08);
        p.drawRoundedRect(bar, 6, 6);
        p.drawEllipse(dot);
        // label
        p.setPen(QPen(QColor(30,30,30)));
        QFont f = p.font(); f.setBold(true); f.setPointSize(28); p.setFont(f);
        p.drawText(QRectF(0, H*0.80, W, H*0.18), Qt::AlignHCenter|Qt::AlignVCenter, text);
    });
}

QPixmap AssetBrowserDialog::genEnergyLabel()
{
    const int W = 384, H = 512;
    return genCanvas(W, H, [=](QPainter& p){
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(0,0,W,H), 24, 24);
        p.setPen(QPen(QColor(40,40,40), 6));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(3,3,W-6,H-6), 24, 24);

        const QVector<QColor> colors = {
            QColor(0, 158, 71), QColor(120, 190, 32), QColor(255, 205, 0),
            QColor(255, 146, 0), QColor(238, 28, 37), QColor(188, 0, 45)
        };
        const int rows = colors.size();
        for (int i=0;i<rows;++i) {
            const qreal y = 60 + i*(H-120)/rows;
            const qreal h = (H-140)/rows;
            const qreal w = W * (0.95 - 0.08*i);
            p.setPen(Qt::NoPen);
            p.setBrush(colors[i]);
            p.drawRoundedRect(QRectF(20, y, w-40, h), 10, 10);
        }
        // A 标
        p.setPen(QPen(QColor(30,30,30)));
        QFont f = p.font(); f.setBold(true); f.setPointSize(40); p.setFont(f);
        p.drawText(QRectF(0,0,W,60), Qt::AlignCenter, QStringLiteral("A"));
    });
}

QPixmap AssetBrowserDialog::genGbMark()
{
    const int W = 512, H = 512;
    return genCanvas(W, H, [=](QPainter& p){
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 122, 204));
        p.drawEllipse(QRectF(0,0,W,H));
        p.setPen(Qt::white);
        QFont f = p.font(); f.setBold(true); f.setPointSize(72); p.setFont(f);
        p.drawText(QRectF(0,0,W,H), Qt::AlignCenter, QStringLiteral("GB"));
    });
}

QPixmap AssetBrowserDialog::genPersonSilhouette()
{
    const int W = 512, H = 512;
    return genCanvas(W, H, [=](QPainter& p){
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(66, 133, 244));
        p.drawRect(QRectF(0,0,W,H));
        // head
        p.setBrush(Qt::white);
        p.drawEllipse(QRectF(W*0.32, H*0.18, W*0.36, W*0.36));
        // body
        QPainterPath path;
        path.moveTo(W*0.18, H*0.88);
        path.quadTo(W*0.50, H*0.60, W*0.82, H*0.88);
        path.lineTo(W*0.18, H*0.88);
        p.fillPath(path, Qt::white);
    });
}

QPixmap AssetBrowserDialog::makeThumb(const QPixmap& src)
{
    // 统一输出固定128x128画布：保持比例缩放并居中，避免不同宽高导致视觉跳动
    const int TW = 128, TH = 128;
    QPixmap canvas(TW, TH);
    canvas.fill(Qt::transparent);
    if (src.isNull()) return canvas;
    QPixmap scaled = src.scaled(TW, TH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int x = (TW - scaled.width()) / 2;
    const int y = (TH - scaled.height()) / 2;
    p.drawPixmap(x, y, scaled);
    p.end();
    return withDevicePixelRatio(canvas);
}

void AssetBrowserDialog::populateBuiltinAssets()
{
    // 保留旧实现，但不再默认调用
    m_all.clear();
    // 警示语
    {
        const QStringList warns = { QStringLiteral("小心触电"), QStringLiteral("易燃"), QStringLiteral("高温烫伤") };
        for (const QString& w : warns) {
            AssetItem it;
            it.id = QStringLiteral("warn_%1").arg(w);
            it.name = w;
            it.category = QStringLiteral("警示语");
            it.full = genWarningIcon(w);
            it.thumbnail = makeThumb(it.full);
            m_all.push_back(std::move(it));
        }
    }
    // 能效
    {
        AssetItem it;
        it.id = QStringLiteral("energy_label");
        it.name = QStringLiteral("能效标签");
        it.category = QStringLiteral("能效");
        it.full = genEnergyLabel();
        it.thumbnail = makeThumb(it.full);
        m_all.push_back(std::move(it));
    }
    // 国标
    {
        AssetItem it;
        it.id = QStringLiteral("gb_mark");
        it.name = QStringLiteral("GB 标识");
        it.category = QStringLiteral("国标");
        it.full = genGbMark();
        it.thumbnail = makeThumb(it.full);
        m_all.push_back(std::move(it));
    }
    // 人物
    {
        AssetItem it;
        it.id = QStringLiteral("person");
        it.name = QStringLiteral("人物");
        it.category = QStringLiteral("人物");
        it.full = genPersonSilhouette();
        it.thumbnail = makeThumb(it.full);
        m_all.push_back(std::move(it));
    }
}

static bool isImageFile(const QString& fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    static const QSet<QString> exts = {"png","jpg","jpeg","bmp","gif","webp","svg"};
    return exts.contains(ext);
}

void AssetBrowserDialog::populateExternalAssets()
{
    // 以外部素材为唯一来源：先清空列表
    m_all.clear();
    // 扫描 m_assetsDir 目录：一级子目录为分类；根目录下文件归类为“自定义”
    QDir base(m_assetsDir);
    if (!base.exists()) return;

    auto loadFile = [this](const QString& path, const QString& category){
        if (!isImageFile(path)) return;
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QImage img = reader.read();
        if (img.isNull()) return;
        QPixmap pm = QPixmap::fromImage(img);
        AssetItem it;
        it.id = path; // 使用绝对路径作为唯一ID
        it.name = QFileInfo(path).completeBaseName();
        it.category = category;
        it.full = withDevicePixelRatio(pm);
        it.thumbnail = makeThumb(it.full);
        m_all.push_back(std::move(it));
    };

    // 根目录文件
    const QFileInfoList rootFiles = base.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : rootFiles) {
        loadFile(fi.absoluteFilePath(), tr("自定义"));
    }

    // 子目录作为分类
    const QFileInfoList subdirs = base.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& di : subdirs) {
        const QString cat = di.fileName();
        QDir d(di.absoluteFilePath());
        const QFileInfoList files = d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fi : files) {
            loadFile(fi.absoluteFilePath(), cat);
        }
    }
}

void AssetBrowserDialog::updateCategories()
{
    // 记录当前选择值
    const QString prev = m_categoryCombo->currentData().toString();
    m_categoryCombo->blockSignals(true);
    m_categoryCombo->clear();
    m_categoryCombo->addItem(tr("全部"), QString());

    QSet<QString> cats;
    for (const auto& it : m_all) {
        if (!it.category.isEmpty()) cats.insert(it.category);
    }
    QStringList list = QStringList(cats.begin(), cats.end());
    std::sort(list.begin(), list.end(), [](const QString& a, const QString& b){ return a.localeAwareCompare(b) < 0; });
    for (const QString& c : list) {
        m_categoryCombo->addItem(c, c);
    }
    // 恢复之前选择
    int idx = m_categoryCombo->findData(prev);
    if (idx < 0) idx = 0;
    m_categoryCombo->setCurrentIndex(idx);
    m_categoryCombo->blockSignals(false);
}

void AssetBrowserDialog::refreshList()
{
    m_list->clear();
    m_currentIndex = -1;
    for (int i=0;i<m_all.size();++i) {
        const auto& it = m_all[i];
        if (!m_currentCategory.isEmpty() && it.category != m_currentCategory) continue;
        if (!m_filter.isEmpty() && !it.name.contains(m_filter, Qt::CaseInsensitive)) continue;
        auto* item = new QListWidgetItem(QIcon(it.thumbnail), it.name, m_list);
        item->setData(Qt::UserRole, i);
        m_list->addItem(item);
    }
}

bool AssetBrowserDialog::hasSelection() const
{
    return m_currentIndex >= 0 && m_currentIndex < m_all.size();
}

AssetItem AssetBrowserDialog::selected() const
{
    if (hasSelection()) return m_all[m_currentIndex];
    return AssetItem{};
}

void AssetBrowserDialog::onCategoryChanged(int index)
{
    Q_UNUSED(index);
    m_currentCategory = m_categoryCombo->currentData().toString();
    refreshList();
}

void AssetBrowserDialog::onFilterTextChanged(const QString& text)
{
    m_filter = text;
    refreshList();
}

void AssetBrowserDialog::onItemActivated(QListWidgetItem* item)
{
    if (!item) return;
    bool ok=false; int idx = item->data(Qt::UserRole).toInt(&ok);
    if (!ok) return;
    m_currentIndex = idx;
    onAccept();
}

void AssetBrowserDialog::onAccept()
{
    // 如果列表有当前项，则以此为准
    if (auto* item = m_list->currentItem()) {
        bool ok=false; int idx = item->data(Qt::UserRole).toInt(&ok);
        if (ok) m_currentIndex = idx;
    }
    if (!hasSelection()) return;
    accept();
}
