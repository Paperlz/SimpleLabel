#include "templatecenterdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QPixmap>
#include <QPainter>
#include <QApplication>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QJsonArray>
#include <QMessageBox>
#include <memory>
#include "../core/labelelement.h"

TemplateCenterDialog::TemplateCenterDialog(QWidget *parent)
    : QDialog(parent)
    , m_templateSelected(false)
{
    setWindowTitle(tr("新建标签"));
    setModal(true);
    resize(900, 600);
    
    // 设置模板路径
    const QString baseDir = QCoreApplication::applicationDirPath();
    m_templatesBasePath = QDir(baseDir).filePath("templates");
    m_myTemplatesPath = QDir(m_templatesBasePath).filePath("my");
    
    // 确保目录存在
    QDir().mkpath(m_myTemplatesPath);
    
    // 创建各个模板分类目录
    QStringList categories = {
        "featured",      // 精选
        "basic",         // 基础
        "warehouse",     // 仓储物流
        "electronics",   // 电子产品
        "ecommerce",     // 电商
        "clothing",      // 服饰
        "certificate",   // 合格证
        "supermarket",   // 超市
        "receipt",       // 小票
        "personnel"      // 人员证
    };
    
    for (const QString &category : categories) {
    QDir().mkpath(QDir(m_templatesBasePath).filePath(category));
    }
    
    setupUI();
    loadTemplates(); //预留扩展点，后续可以预扫描各分类目录，预热缩略图缓存，异步加载历史/精选数据等
    
    // 默认选中"新建空白"
    m_categoryList->setCurrentRow(0);
    onCategoryChanged(0);
}

void TemplateCenterDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    
    // 顶部搜索栏
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchLabel = new QLabel(tr("搜索:"));
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("搜索模板..."));
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchEdit);
    mainLayout->addLayout(searchLayout);
    
    // 中间主内容区 - 使用分割器
    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    
    // 左侧分类栏
    m_categoryList = new QListWidget();
    m_categoryList->setMaximumWidth(200);
    m_categoryList->addItem(tr("新建空白"));
    m_categoryList->addItem(tr("我的模板"));
    m_categoryList->addItem(tr("精选"));
    m_categoryList->addItem(tr("基础"));
    m_categoryList->addItem(tr("仓储物流"));
    m_categoryList->addItem(tr("电子产品"));
    m_categoryList->addItem(tr("电商"));
    m_categoryList->addItem(tr("服饰"));
    m_categoryList->addItem(tr("合格证"));
    m_categoryList->addItem(tr("超市"));
    m_categoryList->addItem(tr("小票"));
    m_categoryList->addItem(tr("人员证"));
    m_categoryList->setCurrentRow(0);
    splitter->addWidget(m_categoryList);
    
    // 右侧内容显示区
    m_contentStack = new QStackedWidget();
    
    // 创建"新建空白"页面
    m_blankPage = new QWidget();
    QFormLayout *blankLayout = new QFormLayout(m_blankPage);
    blankLayout->setSpacing(15);
    blankLayout->setContentsMargins(20, 20, 20, 20);
    
    m_nameEdit = new QLineEdit(tr("未命名标签"));
    m_widthSpin = new QSpinBox();
    m_heightSpin = new QSpinBox();
    
    m_widthSpin->setRange(10, 1000);
    m_heightSpin->setRange(10, 1000);
    m_widthSpin->setValue(100);
    m_heightSpin->setValue(75);
    m_widthSpin->setSuffix(" mm");
    m_heightSpin->setSuffix(" mm");
    
    blankLayout->addRow(tr("标签名称:"), m_nameEdit);
    blankLayout->addRow(tr("宽度:"), m_widthSpin);
    blankLayout->addRow(tr("高度:"), m_heightSpin);
    
    // 添加说明文字
    QLabel *infoLabel = new QLabel(tr("创建一个指定尺寸的空白标签"));
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("color: gray; padding-top: 20px;");
    blankLayout->addRow(infoLabel);
    
    m_contentStack->addWidget(m_blankPage);
    
    // 创建模板选择页面
    m_templatePage = new QWidget();
    QVBoxLayout *templateLayout = new QVBoxLayout(m_templatePage);
    templateLayout->setContentsMargins(0, 0, 0, 0);
    templateLayout->setSpacing(8);

    // 仅在“我的模板”页显示的工具条
    m_templateControlsBar = new QWidget(m_templatePage);
    QHBoxLayout *controlsLayout = new QHBoxLayout(m_templateControlsBar);
    controlsLayout->setContentsMargins(8, 8, 8, 8);
    controlsLayout->setSpacing(8);
    m_importButton = new QPushButton(tr("导入模板…"), m_templateControlsBar);
    m_deleteButton = new QPushButton(tr("删除选中模板"), m_templateControlsBar);
    m_deleteButton->setEnabled(false);
    m_openFolderButton = new QPushButton(tr("打开模板文件夹"), m_templateControlsBar);
    controlsLayout->addWidget(m_importButton);
    controlsLayout->addWidget(m_deleteButton);
    controlsLayout->addWidget(m_openFolderButton);
    controlsLayout->addStretch();
    m_templateControlsBar->setVisible(false); // 默认隐藏，切到“我的模板”才显示
    
    m_templateList = new QListWidget();
    m_templateList->setViewMode(QListView::IconMode);
    m_templateList->setIconSize(QSize(150, 150));
    m_templateList->setGridSize(QSize(180, 200));
    m_templateList->setResizeMode(QListView::Adjust);
    m_templateList->setMovement(QListView::Static);
    m_templateList->setSpacing(10);
    m_templateList->setWordWrap(true);
    m_templateList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    templateLayout->addWidget(m_templateControlsBar);
    templateLayout->addWidget(m_templateList);
    m_contentStack->addWidget(m_templatePage);
    
    splitter->addWidget(m_contentStack);
    splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(splitter);
    
    // 底部按钮
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_createButton = m_buttonBox->button(QDialogButtonBox::Ok);
    m_createButton->setText(tr("创建"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    
    mainLayout->addWidget(m_buttonBox);
    
    // 连接信号
    connect(m_categoryList, &QListWidget::currentRowChanged,
            this, &TemplateCenterDialog::onCategoryChanged);
    connect(m_templateList, &QListWidget::itemClicked,
            this, &TemplateCenterDialog::onTemplateSelected);
    connect(m_templateList, &QListWidget::itemDoubleClicked,
            this, &TemplateCenterDialog::onTemplateDoubleClicked);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &TemplateCenterDialog::onSearchTextChanged);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    // 连接宽度和高度变化信号
    connect(m_widthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TemplateCenterDialog::updateCreateButtonState);
    connect(m_heightSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TemplateCenterDialog::updateCreateButtonState);

    // 导入/打开文件夹按钮
    connect(m_importButton, &QPushButton::clicked, this, &TemplateCenterDialog::onImportTemplates);
    connect(m_openFolderButton, &QPushButton::clicked, this, &TemplateCenterDialog::onOpenTemplatesFolder);
    connect(m_deleteButton, &QPushButton::clicked, this, &TemplateCenterDialog::onDeleteTemplates);
    connect(m_templateList, &QListWidget::itemSelectionChanged,
        this, &TemplateCenterDialog::updateMyTemplateControlsState);
}

void TemplateCenterDialog::onCategoryChanged(int row)
{
    m_currentCategory = "";
    
    switch (row) {
        case 0: // 新建空白
            m_contentStack->setCurrentWidget(m_blankPage);
            m_createButton->setEnabled(true);
            m_templateSelected = false;
            m_searchEdit->setEnabled(false);
            break;
            
        case 1: // 我的模板
            m_contentStack->setCurrentWidget(m_templatePage);
            loadMyTemplates();
            m_createButton->setEnabled(false);
            m_searchEdit->setEnabled(true);
            m_templateControlsBar->setVisible(true);
            updateMyTemplateControlsState();
            break;
            
        case 2: // 精选
            m_currentCategory = "featured";
            m_contentStack->setCurrentWidget(m_templatePage);
            loadCategoryTemplates(m_currentCategory);
            m_createButton->setEnabled(false);
            m_searchEdit->setEnabled(true);
            m_templateControlsBar->setVisible(false);
            updateMyTemplateControlsState();
            break;
            
        case 3: // 基础
            m_currentCategory = "basic";
            m_contentStack->setCurrentWidget(m_templatePage);
            loadCategoryTemplates(m_currentCategory);
            m_createButton->setEnabled(false);
            m_searchEdit->setEnabled(true);
            m_templateControlsBar->setVisible(false);
            updateMyTemplateControlsState();
            break;
            
        case 4: // 仓储物流
            m_currentCategory = "warehouse";
            m_contentStack->setCurrentWidget(m_templatePage);
            loadCategoryTemplates(m_currentCategory);
            m_createButton->setEnabled(false);
            m_searchEdit->setEnabled(true);
            m_templateControlsBar->setVisible(false);
            updateMyTemplateControlsState();
            break;
            
        case 5: // 电子产品
            m_currentCategory = "electronics";
            m_contentStack->setCurrentWidget(m_templatePage);
            loadCategoryTemplates(m_currentCategory);
            m_createButton->setEnabled(false);
            m_searchEdit->setEnabled(true);
            m_templateControlsBar->setVisible(false);
            updateMyTemplateControlsState();
            break;
            
        case 6: // 电商
            m_currentCategory = "ecommerce";
            m_contentStack->setCurrentWidget(m_templatePage);
            loadCategoryTemplates(m_currentCategory);
            m_createButton->setEnabled(false);
            m_searchEdit->setEnabled(true);
            m_templateControlsBar->setVisible(false);
            updateMyTemplateControlsState();
            break;
            
        case 7: // 服饰
            m_currentCategory = "clothing";
            m_contentStack->setCurrentWidget(m_templatePage);
            loadCategoryTemplates(m_currentCategory);
            m_createButton->setEnabled(false);
            m_searchEdit->setEnabled(true);
            m_templateControlsBar->setVisible(false);
            updateMyTemplateControlsState();
            break;
            
        case 8: // 合格证
            m_currentCategory = "certificate";
            m_contentStack->setCurrentWidget(m_templatePage);
            loadCategoryTemplates(m_currentCategory);
            m_createButton->setEnabled(false);
            m_searchEdit->setEnabled(true);
            m_templateControlsBar->setVisible(false);
            updateMyTemplateControlsState();
            break;
            
        case 9: // 超市
            m_currentCategory = "supermarket";
            m_contentStack->setCurrentWidget(m_templatePage);
            loadCategoryTemplates(m_currentCategory);
            m_createButton->setEnabled(false);
            m_searchEdit->setEnabled(true);
            m_templateControlsBar->setVisible(false);
            updateMyTemplateControlsState();
            break;
            
        case 10: // 小票
            m_currentCategory = "receipt";
            m_contentStack->setCurrentWidget(m_templatePage);
            loadCategoryTemplates(m_currentCategory);
            m_createButton->setEnabled(false);
            m_searchEdit->setEnabled(true);
            m_templateControlsBar->setVisible(false);
            updateMyTemplateControlsState();
            break;
            
        case 11: // 人员证
            m_currentCategory = "personnel";
            m_contentStack->setCurrentWidget(m_templatePage);
            loadCategoryTemplates(m_currentCategory);
            m_createButton->setEnabled(false);
            m_searchEdit->setEnabled(true);
            m_templateControlsBar->setVisible(false);
            updateMyTemplateControlsState();
            break;
    }
}

void TemplateCenterDialog::onTemplateSelected(QListWidgetItem *item)
{
    if (item) {
        m_templateSelected = true;
        m_selectedTemplatePath = item->data(Qt::UserRole).toString();
        m_createButton->setEnabled(true);
    }
}

void TemplateCenterDialog::onTemplateDoubleClicked(QListWidgetItem *item)
{
    if (item) {
        m_templateSelected = true;
        m_selectedTemplatePath = item->data(Qt::UserRole).toString();
        accept();
    }
}

void TemplateCenterDialog::onSearchTextChanged(const QString &text)
{
    filterTemplates(text);
}

void TemplateCenterDialog::updateCreateButtonState()
{
    // 在新建空白页面，始终允许创建
    if (m_contentStack->currentWidget() == m_blankPage) {
        m_createButton->setEnabled(true);
    }
}

void TemplateCenterDialog::loadTemplates()
{
    // 初始加载时不做任何操作，等待用户选择分类
}

void TemplateCenterDialog::loadMyTemplates()
{
    m_templateList->clear();
    m_allTemplateItems.clear();
    m_templateSelected = false;
    m_templateList->clearSelection();
    
    QDir dir(m_myTemplatesPath);
    QStringList filters;
    filters << "*.lbl";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
    
    if (fileList.isEmpty()) {
        QListWidgetItem *emptyItem = new QListWidgetItem(tr("暂无自定义模板"));
        emptyItem->setFlags(Qt::NoItemFlags);
        QFont font = emptyItem->font();
        font.setItalic(true);
        emptyItem->setFont(font);
        emptyItem->setForeground(Qt::gray);
        m_templateList->addItem(emptyItem);
        updateMyTemplateControlsState();
        return;
    }
    
    for (const QFileInfo &fileInfo : fileList) {
        QString filePath = fileInfo.absoluteFilePath();
        QString fileName = fileInfo.baseName();
        
    // 读取模板文件获取尺寸信息
        QFile file(filePath);
        QString sizeInfo;
        if (file.open(QFile::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonObject obj = doc.object();
            QJsonObject labelInfo = obj["labelInfo"].toObject();
            double width = labelInfo["width"].toDouble();
            double height = labelInfo["height"].toDouble();
            sizeInfo = QString("%1 x %2 mm").arg(width).arg(height);
            file.close();
        }
        
    // 生成真实缩略图
    QPixmap thumbnail = getTemplateThumbnail(filePath, fileName, sizeInfo);
        
    QListWidgetItem *item = new QListWidgetItem(QIcon(thumbnail), 
                                                     fileName + "\n" + sizeInfo);
        item->setData(Qt::UserRole, filePath);
        item->setToolTip(filePath);
        
        m_templateList->addItem(item);
        m_allTemplateItems.append(item);
    }

    updateMyTemplateControlsState();
}

QString TemplateCenterDialog::ensureUniqueFilePath(const QString &dir, const QString &fileName) const
{
    QFileInfo fi(fileName);
    QString base = fi.completeBaseName();
    QString suffix = fi.suffix();
    QString candidate = QDir(dir).filePath(fileName);
    int counter = 1;
    while (QFile::exists(candidate)) {
        QString numbered = QString("%1(%2).%3").arg(base).arg(counter++).arg(suffix);
        candidate = QDir(dir).filePath(numbered);
    }
    return candidate;
}

void TemplateCenterDialog::onImportTemplates()
{
    // 选择一个或多个 .lbl 文件
    QStringList files = QFileDialog::getOpenFileNames(this,
        tr("选择要导入的模板文件"), QString(), tr("标签文件 (*.lbl)"));
    if (files.isEmpty()) return;

    int imported = 0;
    for (const QString &src : files) {
        QFileInfo fi(src);
        if (!fi.exists() || fi.suffix().compare("lbl", Qt::CaseInsensitive) != 0) {
            continue;
        }
        QString dest = ensureUniqueFilePath(m_myTemplatesPath, fi.fileName());
        if (QFile::copy(src, dest)) {
            ++imported;
        }
    }
    if (imported > 0) {
        loadMyTemplates();
    }
}

void TemplateCenterDialog::onOpenTemplatesFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_myTemplatesPath));
}

void TemplateCenterDialog::onDeleteTemplates()
{
    if (m_categoryList->currentRow() != 1 || !m_templateList) {
        return;
    }

    const QList<QListWidgetItem*> selectedItems = m_templateList->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    QStringList targets;
    QStringList names;
    const QDir myDir(m_myTemplatesPath);
    for (QListWidgetItem *item : selectedItems) {
        if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
            continue;
        }
        const QString path = item->data(Qt::UserRole).toString();
        if (path.isEmpty()) {
            continue;
        }
        const QString relative = myDir.relativeFilePath(path);
        if (relative.startsWith("..")) {
            continue; // 防止删除其他目录
        }
        targets.append(path);
        names.append(QFileInfo(path).fileName());
    }

    if (targets.isEmpty()) {
        return;
    }

    QString prompt;
    if (targets.size() == 1) {
        prompt = tr("确定要删除模板“%1”吗？此操作不可恢复。").arg(QFileInfo(targets.first()).completeBaseName());
    } else {
        prompt = tr("确定要删除选中的 %1 个模板吗？此操作不可恢复。\n\n%2")
                     .arg(targets.size())
                     .arg(names.join(QLatin1Char('\n')));
    }

    const auto reply = QMessageBox::question(this, tr("删除模板"), prompt,
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    QStringList failed;
    for (const QString &path : targets) {
        if (QFile::remove(path)) {
            m_thumbnailCache.remove(path);
        } else {
            failed.append(QFileInfo(path).fileName());
        }
    }

    loadMyTemplates();

    if (!failed.isEmpty()) {
        QMessageBox::warning(this, tr("删除模板"),
                             tr("以下模板删除失败：\n%1").arg(failed.join(QLatin1Char('\n'))));
    }
}

void TemplateCenterDialog::loadCategoryTemplates(const QString &category)
{
    m_templateList->clear();
    m_allTemplateItems.clear();
    
    QString categoryPath = QDir(m_templatesBasePath).filePath(category);
    QDir dir(categoryPath);
    QStringList filters;
    filters << "*.lbl";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
    
    if (fileList.isEmpty()) {
        // 显示该分类暂无模板
        QString categoryName;
        if (category == "featured") categoryName = tr("精选");
        else if (category == "basic") categoryName = tr("基础");
        else if (category == "warehouse") categoryName = tr("仓储物流");
        else if (category == "electronics") categoryName = tr("电子产品");
        else if (category == "ecommerce") categoryName = tr("电商");
        else if (category == "clothing") categoryName = tr("服饰");
        else if (category == "certificate") categoryName = tr("合格证");
        else if (category == "supermarket") categoryName = tr("超市");
        else if (category == "receipt") categoryName = tr("小票");
        else if (category == "personnel") categoryName = tr("人员证");
        else categoryName = category;
        
        QListWidgetItem *emptyItem = new QListWidgetItem(tr("该分类暂无模板"));
        emptyItem->setFlags(Qt::NoItemFlags);
        QFont font = emptyItem->font();
        font.setItalic(true);
        emptyItem->setFont(font);
        emptyItem->setForeground(Qt::gray);
        m_templateList->addItem(emptyItem);
        return;
    }
    
    // 加载该分类下的所有模板文件
    for (const QFileInfo &fileInfo : fileList) {
        QString filePath = fileInfo.absoluteFilePath();
        QString fileName = fileInfo.baseName();
        
        // 读取模板文件获取尺寸信息
        QFile file(filePath);
        QString sizeInfo;
        if (file.open(QFile::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonObject obj = doc.object();
            QJsonObject labelInfo = obj["labelInfo"].toObject();
            double width = labelInfo["width"].toDouble();
            double height = labelInfo["height"].toDouble();
            sizeInfo = QString("%1 x %2 mm").arg(width).arg(height);
            file.close();
        }
        
        // 生成真实缩略图
        QPixmap thumbnail = getTemplateThumbnail(filePath, fileName, sizeInfo);
        
        QListWidgetItem *item = new QListWidgetItem(QIcon(thumbnail), 
                                                     fileName + "\n" + sizeInfo);
        item->setData(Qt::UserRole, filePath);
        item->setToolTip(filePath);
        
        m_templateList->addItem(item);
        m_allTemplateItems.append(item);
    }

    updateMyTemplateControlsState();
}

// 生成模板缩略图入口，带缓存与占位符回退
QPixmap TemplateCenterDialog::getTemplateThumbnail(const QString &filePath,
                                                  const QString &displayName,
                                                  const QString &sizeInfo) const
{
    // 使用缓存
    auto it = m_thumbnailCache.constFind(filePath);
    if (it != m_thumbnailCache.constEnd()) {
        return it.value();
    }

    // 尝试渲染真实缩略图
    QPixmap pix = renderTemplateThumbnailFromFile(filePath);
    if (!pix.isNull()) {
        m_thumbnailCache.insert(filePath, pix);
        return pix;
    }

    // 失败则生成占位图
    QPixmap placeholder(150, 150);
    placeholder.fill(QColor(245, 248, 250));
    QPainter p(&placeholder);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(180, 180, 180), 2));
    p.drawRect(10, 10, 130, 130);
    p.setPen(QColor(60, 60, 60));
    QFont ft = p.font(); ft.setBold(true);
    p.setFont(ft);
    p.drawText(QRect(15, 20, 120, 70), Qt::AlignCenter | Qt::TextWordWrap, displayName);
    p.setPen(QColor(90, 90, 90));
    ft.setBold(false); ft.setPointSize(ft.pointSize() - 1);
    p.setFont(ft);
    p.drawText(QRect(15, 95, 120, 40), Qt::AlignCenter, sizeInfo);
    p.end();
    m_thumbnailCache.insert(filePath, placeholder);
    return placeholder;
}

// 使用离屏 QGraphicsScene 根据 .lbl 文件真实渲染缩略图
QPixmap TemplateCenterDialog::renderTemplateThumbnailFromFile(const QString &filePath,
                                                              const QSize &targetSize) const
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly)) return QPixmap();
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isNull() || !doc.isObject()) return QPixmap();
    QJsonObject root = doc.object();

    // 读取尺寸（mm）并转换为像素（沿用主程序缩放常量）
    const double MM_TO_PIXELS = 7.559056; // 与 MainWindow 保持一致，保证元素坐标对应
    QJsonObject labelInfo = root.value("labelInfo").toObject();
    double wmm = labelInfo.value("width").toDouble(100.0);
    double hmm = labelInfo.value("height").toDouble(75.0);
    double wpx = qMax(10.0, wmm * MM_TO_PIXELS);
    double hpx = qMax(10.0, hmm * MM_TO_PIXELS);

    // 创建场景及背景
    auto scene = std::make_unique<QGraphicsScene>();
    QPainterPath path; path.addRect(QRectF(0, 0, wpx, hpx));
    auto *bg = scene->addPath(path, QPen(Qt::NoPen), QBrush(Qt::white));
    bg->setZValue(-1000);

    // 加载元素
    QJsonArray items = root.value("items").toArray();
    for (const auto &it : items) {
        QJsonObject obj = it.toObject();
        auto element = labelelement::createFromJson(obj);
        if (element) {
            element->addToScene(scene.get());
            QPointF pos(obj.value("x").toDouble(), obj.value("y").toDouble());
            element->setPos(pos);
        }
    }

    // 计算源区域（含少量边距）
    QRectF itemsRect = scene->itemsBoundingRect();
    if (!itemsRect.isValid() || itemsRect.isNull()) itemsRect = QRectF(0, 0, wpx, hpx);
    qreal margin = 10.0; // px
    QRectF source = itemsRect.adjusted(-margin, -margin, margin, margin);

    // 目标图像
    QPixmap pix(targetSize);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 将场景内容渲染到目标区域
    scene->render(&painter, QRectF(QPointF(0, 0), QSizeF(targetSize)), source);
    painter.end();
    return pix;
}

void TemplateCenterDialog::filterTemplates(const QString &searchText)
{
    if (searchText.isEmpty()) {
        // 显示所有模板
        for (int i = 0; i < m_templateList->count(); ++i) {
            m_templateList->item(i)->setHidden(false);
        }
    } else {
        // 过滤模板
        for (int i = 0; i < m_templateList->count(); ++i) {
            QListWidgetItem *item = m_templateList->item(i);
            QString itemText = item->text();
            bool matches = itemText.contains(searchText, Qt::CaseInsensitive);
            item->setHidden(!matches);
        }
    }

    updateMyTemplateControlsState();
}

void TemplateCenterDialog::updateMyTemplateControlsState()
{
    if (!m_deleteButton) {
        return;
    }

    const bool myTemplatesActive = (m_categoryList && m_categoryList->currentRow() == 1);
    bool hasValidSelection = false;

    if (myTemplatesActive && m_templateList) {
        const QList<QListWidgetItem*> selectedItems = m_templateList->selectedItems();
        const QDir myDir(m_myTemplatesPath);
        for (QListWidgetItem *item : selectedItems) {
            if (!item) {
                continue;
            }
            if (!(item->flags() & Qt::ItemIsEnabled)) {
                continue;
            }
            const QString path = item->data(Qt::UserRole).toString();
            if (path.isEmpty()) {
                continue;
            }
            const QString relative = myDir.relativeFilePath(path);
            if (relative.startsWith("..")) {
                continue;
            }
            hasValidSelection = true;
            break;
        }
    }

    m_deleteButton->setEnabled(myTemplatesActive && hasValidSelection);
}

QString TemplateCenterDialog::labelName() const
{
    QString name = m_nameEdit->text().trimmed();
    return name.isEmpty() ? tr("未命名标签") : name;
}

int TemplateCenterDialog::labelWidth() const
{
    return m_widthSpin->value();
}

int TemplateCenterDialog::labelHeight() const
{
    return m_heightSpin->value();
}
