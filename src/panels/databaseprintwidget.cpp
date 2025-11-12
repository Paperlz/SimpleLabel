#include "databaseprintwidget.h"

#include "../core/datasource.h"
#include "../core/batchdatasource.h"
#include "../core/tabledatasource.h"
#include "../core/mysqldatasource.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QtGlobal>
#include <QChar>
#include <QVariant>

namespace {

constexpr int kPreviewRowLimit = 50;
constexpr int kMySqlPreviewRowLimit = 50;

QString excelColumnName(int index)
{
    QString name;
    int value = index;
    while (value >= 0) {
        const int remainder = value % 26;
        name.prepend(QChar('A' + remainder));
        value = (value / 26) - 1;
    }
    return name;
}

}

DatabasePrintWidget::DatabasePrintWidget(QWidget *parent)
    : QGroupBox(parent)
{
    setTitle(tr("数据库批量打印"));

    setFlat(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 8, 10, 8);

    auto *controlRow = new QWidget(this);
    auto *controlLayout = new QHBoxLayout(controlRow);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(6);

    m_enableCheck = new QCheckBox(tr("启用数据源"), controlRow);
    m_applyButton = new QPushButton(tr("应用到当前元素"), controlRow);

    controlLayout->addWidget(m_enableCheck);
    controlLayout->addStretch();
    controlLayout->addWidget(m_applyButton);

    mainLayout->addWidget(controlRow);

    m_sourceModeCombo = new QComboBox(this);
    m_sourceModeCombo->addItem(tr("批量导入"));
    m_sourceModeCombo->addItem(tr("表格"));
    m_sourceModeCombo->addItem(tr("MySQL 数据库"));
    m_sourceModeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);

    auto *sourceForm = new QFormLayout();
    sourceForm->setContentsMargins(0, 0, 0, 0);
    sourceForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    sourceForm->addRow(tr("数据源"), m_sourceModeCombo);
    mainLayout->addLayout(sourceForm);

    m_sourceStack = new QStackedWidget(this);
    m_sourceStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    mainLayout->addWidget(m_sourceStack);

    // 批量导入页
    auto *batchPage = new QWidget(this);
    auto *batchLayout = new QVBoxLayout(batchPage);
    batchLayout->setSpacing(6);
    batchLayout->setContentsMargins(0, 0, 0, 0);

    m_batchInputEdit = new QPlainTextEdit(batchPage);
    m_batchInputEdit->setPlaceholderText(tr("一行一个值，或使用下方的分隔符拆分"));
    m_batchInputEdit->setMinimumHeight(120);
    m_batchInputEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    batchLayout->addWidget(m_batchInputEdit);

    auto *delimiterRowWidget = new QWidget(batchPage);
    auto *delimiterLayout = new QHBoxLayout(delimiterRowWidget);
    delimiterLayout->setContentsMargins(0, 0, 0, 0);
    delimiterLayout->setSpacing(6);

    m_delimiterCombo = new QComboBox(batchPage);
    m_delimiterCombo->addItem(tr("回车"));
    m_delimiterCombo->addItem(tr("空格"));
    m_delimiterCombo->addItem(tr("自定义"));
    delimiterLayout->addWidget(m_delimiterCombo);

    m_customDelimiterEdit = new QLineEdit(batchPage);
    m_customDelimiterEdit->setPlaceholderText(tr("请输入自定义分隔符"));
    m_customDelimiterEdit->setEnabled(false);
    m_customDelimiterEdit->setMaxLength(5);
    m_customDelimiterEdit->setFixedWidth(80);
    delimiterLayout->addWidget(m_customDelimiterEdit);

    delimiterLayout->addStretch();

    auto *delimiterForm = new QFormLayout();
    delimiterForm->setContentsMargins(0, 0, 0, 0);
    delimiterForm->addRow(tr("分隔符"), delimiterRowWidget);
    batchLayout->addLayout(delimiterForm);

    m_sourceStack->addWidget(batchPage);

    // 表格导入页
    auto *tablePage = new QWidget(this);
    auto *tableLayout = new QVBoxLayout(tablePage);
    tableLayout->setSpacing(6);
    tableLayout->setContentsMargins(0, 0, 0, 0);

    auto *fileRowWidget = new QWidget(tablePage);
    auto *fileLayout = new QHBoxLayout(fileRowWidget);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    fileLayout->setSpacing(6);

    m_uploadButton = new QPushButton(tr("上传文件..."), tablePage);
    fileLayout->addWidget(m_uploadButton);

    m_selectedFileEdit = new QLineEdit(tablePage);
    m_selectedFileEdit->setPlaceholderText(tr("尚未选择文件"));
    m_selectedFileEdit->setReadOnly(true);
    m_selectedFileEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    fileLayout->addWidget(m_selectedFileEdit);

    auto *tableForm = new QFormLayout();
    tableForm->setContentsMargins(0, 0, 0, 0);
    tableForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    tableForm->addRow(tr("文件"), fileRowWidget);

    m_sheetCombo = new QComboBox(tablePage);
    m_sheetCombo->setEditable(false);
    m_sheetCombo->addItem(tr("Sheet1"));
    m_sheetCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    tableForm->addRow(tr("Sheet"), m_sheetCombo);

    m_headerCombo = new QComboBox(tablePage);
    m_headerCombo->setEditable(false);
    m_headerCombo->addItem(tr("第1行"), 1);
    tableForm->addRow(tr("表头"), m_headerCombo);

    m_columnCombo = new QComboBox(tablePage);
    m_columnCombo->setEditable(false);
    m_columnCombo->addItem(tr("A"), 0);
    tableForm->addRow(tr("列"), m_columnCombo);

    m_startRowSpin = new QSpinBox(tablePage);
    m_startRowSpin->setRange(1, 9999);
    m_startRowSpin->setValue(2);
    tableForm->addRow(tr("起始行"), m_startRowSpin);

    m_endRowSpin = new QSpinBox(tablePage);
    m_endRowSpin->setRange(0, 9999);
    m_endRowSpin->setSpecialValueText(tr("最后一行"));
    m_endRowSpin->setValue(0);
    tableForm->addRow(tr("结束行"), m_endRowSpin);

    tableLayout->addLayout(tableForm);

    auto *previewLabel = new QLabel(tr("内容预览"), tablePage);
    tableLayout->addWidget(previewLabel);

    m_previewTable = new QTableWidget(tablePage);
    m_previewTable->setColumnCount(1);
    m_previewTable->setHorizontalHeaderLabels({tr("行 数据")});
    m_previewTable->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    m_previewTable->setMinimumHeight(140);
    m_previewTable->horizontalHeader()->setStretchLastSection(true);
    m_previewTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_previewTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    tableLayout->addWidget(m_previewTable);

    m_sourceStack->addWidget(tablePage);

    // MySQL 数据源页
    auto *mysqlPage = new QWidget(this);
    auto *mysqlLayout = new QVBoxLayout(mysqlPage);
    mysqlLayout->setSpacing(6);
    mysqlLayout->setContentsMargins(0, 0, 0, 0);

    auto *connectionForm = new QFormLayout();
    connectionForm->setContentsMargins(0, 0, 0, 0);
    connectionForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_mysqlHostEdit = new QLineEdit(mysqlPage);
    m_mysqlHostEdit->setPlaceholderText(tr("例如 localhost"));
    m_mysqlHostEdit->setText(QStringLiteral("localhost"));
    connectionForm->addRow(tr("主机"), m_mysqlHostEdit);

    m_mysqlPortSpin = new QSpinBox(mysqlPage);
    m_mysqlPortSpin->setRange(1, 65535);
    m_mysqlPortSpin->setValue(3306);
    connectionForm->addRow(tr("端口"), m_mysqlPortSpin);

    m_mysqlDatabaseEdit = new QLineEdit(mysqlPage);
    m_mysqlDatabaseEdit->setPlaceholderText(tr("数据库名称"));
    connectionForm->addRow(tr("数据库"), m_mysqlDatabaseEdit);

    m_mysqlUserEdit = new QLineEdit(mysqlPage);
    connectionForm->addRow(tr("用户名"), m_mysqlUserEdit);

    m_mysqlPasswordEdit = new QLineEdit(mysqlPage);
    m_mysqlPasswordEdit->setEchoMode(QLineEdit::Password);
    connectionForm->addRow(tr("密码"), m_mysqlPasswordEdit);

    mysqlLayout->addLayout(connectionForm);

    auto *connectionButtonsRow = new QHBoxLayout();
    connectionButtonsRow->setContentsMargins(0, 0, 0, 0);
    connectionButtonsRow->setSpacing(6);
    m_mysqlTestButton = new QPushButton(tr("测试连接"), mysqlPage);
    m_mysqlRefreshTablesButton = new QPushButton(tr("刷新表"), mysqlPage);
    connectionButtonsRow->addWidget(m_mysqlTestButton);
    connectionButtonsRow->addWidget(m_mysqlRefreshTablesButton);
    connectionButtonsRow->addStretch();
    mysqlLayout->addLayout(connectionButtonsRow);

    auto *mysqlForm = new QFormLayout();
    mysqlForm->setContentsMargins(0, 0, 0, 0);
    mysqlForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_mysqlTableCombo = new QComboBox(mysqlPage);
    m_mysqlTableCombo->setEditable(false);
    m_mysqlTableCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    mysqlForm->addRow(tr("数据表"), m_mysqlTableCombo);

    m_mysqlColumnCombo = new QComboBox(mysqlPage);
    m_mysqlColumnCombo->setEditable(false);
    m_mysqlColumnCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    mysqlForm->addRow(tr("列"), m_mysqlColumnCombo);

    m_mysqlFilterEdit = new QLineEdit(mysqlPage);
    m_mysqlFilterEdit->setPlaceholderText(tr("可选 WHERE 条件，例如 status = 1"));
    mysqlForm->addRow(tr("筛选条件"), m_mysqlFilterEdit);

    mysqlLayout->addLayout(mysqlForm);

    m_mysqlPreviewButton = new QPushButton(tr("加载预览"), mysqlPage);
    mysqlLayout->addWidget(m_mysqlPreviewButton, 0, Qt::AlignLeft);

    auto *mysqlPreviewLabel = new QLabel(tr("内容预览"), mysqlPage);
    mysqlLayout->addWidget(mysqlPreviewLabel);

    m_mysqlPreviewTable = new QTableWidget(mysqlPage);
    m_mysqlPreviewTable->setColumnCount(2);
    m_mysqlPreviewTable->setHorizontalHeaderLabels({tr("序号"), tr("值")});
    m_mysqlPreviewTable->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    m_mysqlPreviewTable->setMinimumHeight(140);
    m_mysqlPreviewTable->horizontalHeader()->setStretchLastSection(true);
    m_mysqlPreviewTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_mysqlPreviewTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_mysqlPreviewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    mysqlLayout->addWidget(m_mysqlPreviewTable);

    m_sourceStack->addWidget(mysqlPage);

    connect(m_sourceModeCombo, &QComboBox::currentIndexChanged,
            this, &DatabasePrintWidget::onSourceModeChanged);
    connect(m_delimiterCombo, &QComboBox::currentIndexChanged,
            this, &DatabasePrintWidget::onDelimiterChanged);
    connect(m_uploadButton, &QPushButton::clicked,
        this, &DatabasePrintWidget::onUploadFileClicked);
    connect(m_sheetCombo, &QComboBox::currentIndexChanged,
        this, &DatabasePrintWidget::onSheetChanged);
    connect(m_headerCombo, &QComboBox::currentIndexChanged,
        this, &DatabasePrintWidget::onHeaderChanged);
    connect(m_columnCombo, &QComboBox::currentIndexChanged,
        this, &DatabasePrintWidget::onColumnChanged);
    connect(m_startRowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &DatabasePrintWidget::onStartRowChanged);
    connect(m_endRowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &DatabasePrintWidget::onEndRowChanged);
    connect(m_enableCheck, &QCheckBox::toggled,
        this, &DatabasePrintWidget::dataSourceEnabledChanged);
    connect(m_applyButton, &QPushButton::clicked,
        this, &DatabasePrintWidget::applyRequested);
    connect(m_mysqlTestButton, &QPushButton::clicked,
        this, &DatabasePrintWidget::onMySqlTestConnection);
    connect(m_mysqlRefreshTablesButton, &QPushButton::clicked,
        this, &DatabasePrintWidget::onMySqlRefreshTables);
    connect(m_mysqlTableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &DatabasePrintWidget::onMySqlTableChanged);
    connect(m_mysqlColumnCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &DatabasePrintWidget::onMySqlColumnChanged);
    connect(m_mysqlPreviewButton, &QPushButton::clicked,
        this, &DatabasePrintWidget::onMySqlPreviewClicked);

    onSourceModeChanged(0);
    onDelimiterChanged(0);
    clearTablePreview();
    clearMySqlPreview();
}

void DatabasePrintWidget::onSourceModeChanged(int index)
{
    m_sourceStack->setCurrentIndex(index);
}

void DatabasePrintWidget::onDelimiterChanged(int index)
{
    const bool customSelected = (index == 2);
    m_customDelimiterEdit->setEnabled(customSelected);
    if (!customSelected) {
        m_customDelimiterEdit->clear();
    }
}

bool DatabasePrintWidget::isDataSourceEnabled() const
{
    return m_enableCheck && m_enableCheck->isChecked();
}

void DatabasePrintWidget::setDataSourceEnabled(bool enabled)
{
    if (!m_enableCheck) {
        return;
    }

    QSignalBlocker blocker(m_enableCheck);
    m_enableCheck->setChecked(enabled);
}

std::shared_ptr<DataSource> DatabasePrintWidget::createDataSource(QString *errorMessage) const
{
    if (!m_sourceModeCombo) {
        return nullptr;
    }

    const int mode = m_sourceModeCombo->currentIndex();
    if (mode == 0) {
        auto batch = std::make_shared<BatchDataSource>();
        const QString text = m_batchInputEdit ? m_batchInputEdit->toPlainText() : QString();
        QString delimiter = currentDelimiterText();
        batch->setText(text, delimiter);
        batch->parse();
        if (!batch->isValid()) {
            if (errorMessage) {
                QString err = batch->errorString();
                if (err.isEmpty()) {
                    err = tr("未能解析出有效数据");
                }
                *errorMessage = err;
            }
            return nullptr;
        }
        return batch;
    }

    if (mode == 1) {
        if (!m_tableSource) {
            if (errorMessage) {
                *errorMessage = tr("请先选择并配置 Excel 数据源");
            }
            return nullptr;
        }

        auto table = std::make_shared<TableDataSource>(*m_tableSource);
        table->setSheet(m_tableSource->currentSheet());
        table->setHeaderRow(m_tableSource->headerRow());
        table->setStartRow(m_tableSource->startRow());
        table->setEndRow(m_tableSource->endRow());
        table->setColumnIndex(m_tableSource->columnIndex());

        if (!table->refresh()) {
            if (errorMessage) {
                QString err = table->errorString();
                if (err.isEmpty()) {
                    err = tr("未能读取 Excel 数据");
                }
                *errorMessage = err;
            }
            return nullptr;
        }

        return table;
    }

    if (mode == 2) {
        auto self = const_cast<DatabasePrintWidget*>(this);
        if (!self->ensureMySqlSource()) {
            if (errorMessage) {
                *errorMessage = tr("请先配置 MySQL 数据源");
            }
            return nullptr;
        }

        self->syncMySqlSourceFromUi();
        auto mysqlSource = self->m_mysqlSource;
        if (!mysqlSource) {
            if (errorMessage) {
                *errorMessage = tr("请先配置 MySQL 数据源");
            }
            return nullptr;
        }

        auto mysql = std::make_shared<MySqlDataSource>(*mysqlSource);
        if (!mysql->refresh()) {
            if (errorMessage) {
                QString err = mysql->errorString();
                if (err.isEmpty()) {
                    err = tr("未能读取 MySQL 数据");
                }
                *errorMessage = err;
            }
            return nullptr;
        }

        return mysql;
    }

    return nullptr;
}

void DatabasePrintWidget::setDataSource(const std::shared_ptr<DataSource>& source)
{
    if (!m_sourceModeCombo) {
        return;
    }

    QSignalBlocker blockMode(m_sourceModeCombo);
    QSignalBlocker blockDelimiter(m_delimiterCombo);
    QSignalBlocker blockCustom(m_customDelimiterEdit);
    QSignalBlocker blockText(m_batchInputEdit);
    clearInputs();

    if (!source) {
        return;
    }

    if (source->type() == DataSource::Batch) {
        auto batch = std::dynamic_pointer_cast<BatchDataSource>(source);
        if (!batch) {
            return;
        }

        m_sourceModeCombo->setCurrentIndex(0);
        onSourceModeChanged(0);
        if (m_batchInputEdit) {
            m_batchInputEdit->setPlainText(batch->rawText());
        }

        if (!m_delimiterCombo) {
            return;
        }

        const QString delimiter = batch->delimiter();
        if (delimiter == "\n" || delimiter == "\\n") {
            m_delimiterCombo->setCurrentIndex(0);
        } else if (delimiter == " ") {
            m_delimiterCombo->setCurrentIndex(1);
        } else {
            m_delimiterCombo->setCurrentIndex(2);
            if (m_customDelimiterEdit) {
                m_customDelimiterEdit->setEnabled(true);
                m_customDelimiterEdit->setText(delimiter);
            }
        }
        onDelimiterChanged(m_delimiterCombo->currentIndex());
    } else if (source->type() == DataSource::Table) {
        auto table = std::dynamic_pointer_cast<TableDataSource>(source);
        if (!table) {
            return;
        }

        m_tableSource = std::make_shared<TableDataSource>(*table);
        if (!m_tableSource->filePath().isEmpty() && !QFileInfo::exists(m_tableSource->filePath())) {
            QMessageBox::warning(this, tr("数据源"), tr("Excel 文件不存在: %1").arg(m_tableSource->filePath()));
        }

        m_sourceModeCombo->setCurrentIndex(1);
        onSourceModeChanged(1);

        if (m_selectedFileEdit) {
            m_selectedFileEdit->setText(m_tableSource->filePath());
        }

        if (!m_tableSource->refresh()) {
            QMessageBox::warning(this, tr("数据源"), m_tableSource->errorString());
        }

        updateTableControlsFromSource();
        refreshTablePreview();
    } else if (source->type() == DataSource::MySql) {
        auto mysql = std::dynamic_pointer_cast<MySqlDataSource>(source);
        if (!mysql) {
            return;
        }

        m_mysqlSource = std::make_shared<MySqlDataSource>(*mysql);

        m_sourceModeCombo->setCurrentIndex(2);
        onSourceModeChanged(2);

        updateMySqlControlsFromSource(false);

        if (m_mysqlSource && m_mysqlSource->isValid()) {
            refreshMySqlPreview();
        } else {
            clearMySqlPreview();
        }
    }
}

void DatabasePrintWidget::clearInputs()
{
    if (m_sourceModeCombo) {
        m_sourceModeCombo->setCurrentIndex(0);
        onSourceModeChanged(0);
    }
    if (m_batchInputEdit) {
        m_batchInputEdit->clear();
    }
    if (m_delimiterCombo) {
        m_delimiterCombo->setCurrentIndex(0);
    }
    onDelimiterChanged(m_delimiterCombo ? m_delimiterCombo->currentIndex() : 0);
    if (m_customDelimiterEdit) {
        m_customDelimiterEdit->clear();
    }
    if (m_selectedFileEdit) {
        m_selectedFileEdit->clear();
    }
    if (m_sheetCombo) {
        m_sheetCombo->clear();
        m_sheetCombo->addItem(tr("Sheet1"));
    }
    if (m_headerCombo) {
        m_headerCombo->clear();
        m_headerCombo->addItem(tr("第1行"), 1);
    }
    if (m_columnCombo) {
        m_columnCombo->clear();
        m_columnCombo->addItem(tr("A"), 0);
    }
    if (m_startRowSpin) {
        m_startRowSpin->setValue(2);
    }
    if (m_endRowSpin) {
        m_endRowSpin->setRange(0, 9999);
        m_endRowSpin->setValue(0);
    }
    m_tableSource.reset();
    clearTablePreview();

    if (m_mysqlHostEdit) {
        m_mysqlHostEdit->setText(QStringLiteral("localhost"));
    }
    if (m_mysqlPortSpin) {
        m_mysqlPortSpin->setValue(3306);
    }
    if (m_mysqlDatabaseEdit) {
        m_mysqlDatabaseEdit->clear();
    }
    if (m_mysqlUserEdit) {
        m_mysqlUserEdit->clear();
    }
    if (m_mysqlPasswordEdit) {
        m_mysqlPasswordEdit->clear();
    }
    if (m_mysqlFilterEdit) {
        m_mysqlFilterEdit->clear();
    }
    if (m_mysqlTableCombo) {
        QSignalBlocker blockTable(m_mysqlTableCombo);
        m_mysqlTableCombo->clear();
    }
    if (m_mysqlColumnCombo) {
        QSignalBlocker blockColumn(m_mysqlColumnCombo);
        m_mysqlColumnCombo->clear();
    }
    if (m_mysqlTableCombo) {
        m_mysqlTableCombo->setCurrentIndex(-1);
    }
    if (m_mysqlColumnCombo) {
        m_mysqlColumnCombo->setCurrentIndex(-1);
    }
    m_mysqlSource.reset();
    clearMySqlPreview();
}

QString DatabasePrintWidget::currentDelimiterText() const
{
    if (!m_delimiterCombo) {
        return QStringLiteral("\n");
    }

    switch (m_delimiterCombo->currentIndex()) {
        case 0:
            return QStringLiteral("\n");
        case 1:
            return QStringLiteral(" ");
        case 2:
            return m_customDelimiterEdit ? m_customDelimiterEdit->text() : QString();
        default:
            break;
    }
    return QStringLiteral("\n");
}

void DatabasePrintWidget::onUploadFileClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("选择Excel文件"),
        QString(),
        tr("Excel 文件 (*.xlsx *.xls)"));

    if (filePath.isEmpty()) {
        return;
    }

    auto table = std::make_shared<TableDataSource>();
    if (!table->loadFile(filePath)) {
        QMessageBox::warning(this, tr("Excel 导入"), table->errorString());
        return;
    }

    table->setHeaderRow(1);
    table->setStartRow(2);
    table->setEndRow(0);
    table->setColumnIndex(0);

    if (!table->refresh()) {
        QMessageBox::warning(this, tr("Excel 导入"), table->errorString());
        return;
    }

    m_tableSource = table;

    if (m_selectedFileEdit) {
        m_selectedFileEdit->setText(filePath);
    }

    if (m_sourceModeCombo) {
        m_sourceModeCombo->setCurrentIndex(1);
        onSourceModeChanged(1);
    }

    updateTableControlsFromSource();
    refreshTablePreview();
}

void DatabasePrintWidget::onSheetChanged(int index)
{
    if (m_updatingTableControls || !m_tableSource || !m_sheetCombo) {
        return;
    }

    const QString sheetName = m_sheetCombo->itemText(index);
    if (sheetName.isEmpty()) {
        return;
    }

    m_tableSource->setSheet(sheetName);
    syncTableSourceFromUi();

    if (!m_tableSource->refresh()) {
        QMessageBox::warning(this, tr("Excel 导入"), m_tableSource->errorString());
        return;
    }

    updateTableControlsFromSource();
    refreshTablePreview();
}

void DatabasePrintWidget::onHeaderChanged(int index)
{
    if (m_updatingTableControls || !m_tableSource || !m_headerCombo) {
        return;
    }

    const QVariant data = m_headerCombo->itemData(index);
    const int headerRow = data.isValid() ? data.toInt() : (index + 1);
    m_tableSource->setHeaderRow(headerRow);

    if (headerRow > 0 && m_startRowSpin && m_startRowSpin->value() <= headerRow) {
        QSignalBlocker blockStart(m_startRowSpin);
        m_startRowSpin->setValue(headerRow + 1);
    }

    syncTableSourceFromUi();

    if (!m_tableSource->refresh()) {
        QMessageBox::warning(this, tr("Excel 导入"), m_tableSource->errorString());
        return;
    }

    updateTableControlsFromSource();
    refreshTablePreview();
}

void DatabasePrintWidget::onColumnChanged(int index)
{
    if (m_updatingTableControls || !m_tableSource || !m_columnCombo) {
        return;
    }

    const QVariant data = m_columnCombo->itemData(index);
    int column = data.isValid() ? data.toInt() : index;
    m_tableSource->setColumnIndex(column);
    syncTableSourceFromUi();

    if (!m_tableSource->refresh()) {
        QMessageBox::warning(this, tr("Excel 导入"), m_tableSource->errorString());
        return;
    }

    updateTableControlsFromSource(true);
    refreshTablePreview();
}

void DatabasePrintWidget::onStartRowChanged(int value)
{
    if (m_updatingTableControls || !m_tableSource) {
        return;
    }

    if (m_endRowSpin && m_endRowSpin->value() != 0 && value > m_endRowSpin->value()) {
        QSignalBlocker blockEnd(m_endRowSpin);
        m_endRowSpin->setValue(value);
    }

    syncTableSourceFromUi();

    if (!m_tableSource->refresh()) {
        QMessageBox::warning(this, tr("Excel 导入"), m_tableSource->errorString());
        return;
    }

    updateTableControlsFromSource(true);
    refreshTablePreview();
}

void DatabasePrintWidget::onEndRowChanged(int value)
{
    if (m_updatingTableControls || !m_tableSource) {
        return;
    }

    if (value != 0 && m_startRowSpin && value < m_startRowSpin->value()) {
        QSignalBlocker blockStart(m_startRowSpin);
        m_startRowSpin->setValue(value);
    }

    syncTableSourceFromUi();

    if (!m_tableSource->refresh()) {
        QMessageBox::warning(this, tr("Excel 导入"), m_tableSource->errorString());
        return;
    }

    updateTableControlsFromSource(true);
    refreshTablePreview();
}

void DatabasePrintWidget::clearTablePreview()
{
    if (!m_previewTable) {
        return;
    }

    QSignalBlocker block(m_previewTable);
    m_previewTable->setRowCount(0);
}

void DatabasePrintWidget::refreshTablePreview()
{
    if (!m_previewTable) {
        return;
    }

    clearTablePreview();

    if (!m_tableSource) {
        return;
    }

    const QStringList rows = m_tableSource->preview(kPreviewRowLimit);
    if (rows.isEmpty()) {
        return;
    }

    QSignalBlocker block(m_previewTable);
    m_previewTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        m_previewTable->setItem(i, 0, new QTableWidgetItem(rows.at(i)));
    }
}

void DatabasePrintWidget::syncTableSourceFromUi()
{
    if (!m_tableSource) {
        return;
    }

    if (m_sheetCombo) {
        m_tableSource->setSheet(m_sheetCombo->currentText());
    }
    if (m_headerCombo) {
        const QVariant data = m_headerCombo->currentData();
        int headerRow = data.isValid() ? data.toInt() : (m_headerCombo->currentIndex() + 1);
        m_tableSource->setHeaderRow(headerRow);
    }
    if (m_startRowSpin) {
        m_tableSource->setStartRow(m_startRowSpin->value());
    }
    if (m_endRowSpin) {
        m_tableSource->setEndRow(m_endRowSpin->value());
    }
    if (m_columnCombo) {
        const QVariant data = m_columnCombo->currentData();
        int column = data.isValid() ? data.toInt() : m_columnCombo->currentIndex();
        m_tableSource->setColumnIndex(column);
    }
}

void DatabasePrintWidget::updateTableControlsFromSource(bool preserveSelection)
{
    if (!m_tableSource) {
        clearTablePreview();
        return;
    }

    m_updatingTableControls = true;

    if (m_selectedFileEdit) {
        m_selectedFileEdit->setText(m_tableSource->filePath());
    }

    if (m_sheetCombo) {
        QSignalBlocker blockSheet(m_sheetCombo);
        m_sheetCombo->clear();
        const QStringList sheets = m_tableSource->sheetNames();
        if (!sheets.isEmpty()) {
            m_sheetCombo->addItems(sheets);
            int sheetIndex = m_sheetCombo->findText(m_tableSource->currentSheet());
            if (sheetIndex < 0) {
                sheetIndex = 0;
            }
            m_sheetCombo->setCurrentIndex(sheetIndex);
        }
    }

    const int maxRow = qMax(1, m_tableSource->lastSheetRow());
    populateHeaderCombo(maxRow);
    updateRowSpins(maxRow);
    populateColumnCombo(preserveSelection);

    m_updatingTableControls = false;
}

void DatabasePrintWidget::populateHeaderCombo(int maxRow)
{
    if (!m_headerCombo) {
        return;
    }

    QSignalBlocker blockHeader(m_headerCombo);
    m_headerCombo->clear();
    m_headerCombo->addItem(tr("无表头"), 0);

    const int currentHeader = m_tableSource ? m_tableSource->headerRow() : 0;
    const int limit = qMin(maxRow, qMax(30, currentHeader));
    for (int row = 1; row <= limit; ++row) {
        m_headerCombo->addItem(tr("第%1行").arg(row), row);
    }
    int index = m_headerCombo->findData(currentHeader);
    if (index < 0) {
        index = (currentHeader == 0) ? 0 : 1;
    }
    m_headerCombo->setCurrentIndex(index);
}

void DatabasePrintWidget::populateColumnCombo(bool preserveSelection)
{
    if (!m_columnCombo) {
        return;
    }

    const int desiredColumn = preserveSelection && m_tableSource ? m_tableSource->columnIndex() : 0;

    QSignalBlocker blockColumn(m_columnCombo);
    m_columnCombo->clear();

    if (!m_tableSource) {
        m_columnCombo->addItem(QStringLiteral("A"), 0);
        m_columnCombo->setCurrentIndex(0);
        return;
    }

    const int columnCount = m_tableSource->columnCount();
    const QStringList headers = m_tableSource->columnHeaders();
    for (int col = 0; col < columnCount; ++col) {
        const QString headerText = (col < headers.size()) ? headers.at(col) : QString();
        m_columnCombo->addItem(columnLabelForIndex(col, headerText), col);
    }

    if (m_columnCombo->count() == 0) {
        m_columnCombo->addItem(QStringLiteral("A"), 0);
    }

    int index = m_columnCombo->findData(desiredColumn);
    if (index < 0) {
        index = 0;
    }
    m_columnCombo->setCurrentIndex(index);
}

void DatabasePrintWidget::updateRowSpins(int maxRow)
{
    if (m_startRowSpin) {
        QSignalBlocker blockStart(m_startRowSpin);
        m_startRowSpin->setRange(1, qMax(1, maxRow));
        m_startRowSpin->setValue(qBound(1, m_tableSource ? m_tableSource->startRow() : 1, qMax(1, maxRow)));
    }

    if (m_endRowSpin) {
        QSignalBlocker blockEnd(m_endRowSpin);
        m_endRowSpin->setRange(0, qMax(1, maxRow));
        if (m_tableSource && m_tableSource->endRow() > 0) {
            m_endRowSpin->setValue(qBound(1, m_tableSource->endRow(), qMax(1, maxRow)));
        } else {
            m_endRowSpin->setValue(0);
        }
    }
}

QString DatabasePrintWidget::columnLabelForIndex(int columnIndex, const QString &headerText)
{
    const QString base = excelColumnName(columnIndex);
    if (headerText.trimmed().isEmpty()) {
        return base;
    }
    return QStringLiteral("%1 (%2)").arg(base, headerText.trimmed());
}

void DatabasePrintWidget::clearMySqlPreview()
{
    if (!m_mysqlPreviewTable) {
        return;
    }
    QSignalBlocker block(m_mysqlPreviewTable);
    m_mysqlPreviewTable->setRowCount(0);
}

void DatabasePrintWidget::refreshMySqlPreview()
{
    if (!m_mysqlPreviewTable || !m_mysqlSource) {
        return;
    }

    const QStringList rows = m_mysqlSource->preview(kMySqlPreviewRowLimit);
    QSignalBlocker block(m_mysqlPreviewTable);
    m_mysqlPreviewTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const QString row = rows.at(i);
        const int tabIndex = row.indexOf(QLatin1Char('\t'));
        QString indexText = QString::number(i + 1);
        QString valueText = row;
        if (tabIndex >= 0) {
            indexText = row.left(tabIndex);
            valueText = row.mid(tabIndex + 1);
        }
        m_mysqlPreviewTable->setItem(i, 0, new QTableWidgetItem(indexText));
        m_mysqlPreviewTable->setItem(i, 1, new QTableWidgetItem(valueText));
    }
}

void DatabasePrintWidget::syncMySqlSourceFromUi()
{
    if (!ensureMySqlSource()) {
        return;
    }

    if (m_mysqlHostEdit) {
        m_mysqlSource->setHost(m_mysqlHostEdit->text());
    }
    if (m_mysqlPortSpin) {
        m_mysqlSource->setPort(m_mysqlPortSpin->value());
    }
    if (m_mysqlDatabaseEdit) {
        m_mysqlSource->setDatabase(m_mysqlDatabaseEdit->text());
    }
    if (m_mysqlUserEdit) {
        m_mysqlSource->setUsername(m_mysqlUserEdit->text());
    }
    if (m_mysqlPasswordEdit) {
        m_mysqlSource->setPassword(m_mysqlPasswordEdit->text());
    }
    if (m_mysqlFilterEdit) {
        m_mysqlSource->setFilter(m_mysqlFilterEdit->text());
    }
    if (m_mysqlTableCombo) {
        if (m_mysqlTableCombo->currentIndex() >= 0) {
            m_mysqlSource->setTableName(m_mysqlTableCombo->currentText());
        } else {
            m_mysqlSource->setTableName(QString());
        }
    }
    if (m_mysqlColumnCombo) {
        if (m_mysqlColumnCombo->currentIndex() >= 0) {
            m_mysqlSource->setColumnName(m_mysqlColumnCombo->currentText());
        } else {
            m_mysqlSource->setColumnName(QString());
        }
    }
}

void DatabasePrintWidget::updateMySqlControlsFromSource(bool loadTables)
{
    if (!m_mysqlSource) {
        clearMySqlPreview();
        return;
    }

    m_updatingMySqlControls = true;

    if (m_mysqlHostEdit) {
        m_mysqlHostEdit->setText(m_mysqlSource->host());
    }
    if (m_mysqlPortSpin) {
        m_mysqlPortSpin->setValue(m_mysqlSource->port());
    }
    if (m_mysqlDatabaseEdit) {
        m_mysqlDatabaseEdit->setText(m_mysqlSource->database());
    }
    if (m_mysqlUserEdit) {
        m_mysqlUserEdit->setText(m_mysqlSource->username());
    }
    if (m_mysqlPasswordEdit) {
        m_mysqlPasswordEdit->setText(m_mysqlSource->password());
    }
    if (m_mysqlFilterEdit) {
        m_mysqlFilterEdit->setText(m_mysqlSource->filter());
    }

    if (loadTables) {
        if (!m_mysqlSource->loadTables()) {
            QMessageBox::warning(this, tr("MySQL 数据源"), m_mysqlSource->errorString());
        }
    }

    populateMySqlTableCombo(m_mysqlSource->tables(), true);
    const QString tableName = m_mysqlSource->tableName();
    if (!tableName.isEmpty()) {
        if (!m_mysqlSource->columnsForTable(tableName).isEmpty()) {
            populateMySqlColumnCombo(m_mysqlSource->columnsForTable(tableName), true);
        }
    }

    if (m_mysqlTableCombo) {
        if (m_mysqlTableCombo->currentIndex() >= 0) {
            m_mysqlSource->setTableName(m_mysqlTableCombo->currentText());
        } else {
            m_mysqlSource->setTableName(QString());
        }
    }
    if (m_mysqlColumnCombo) {
        if (m_mysqlColumnCombo->currentIndex() >= 0) {
            m_mysqlSource->setColumnName(m_mysqlColumnCombo->currentText());
        } else {
            m_mysqlSource->setColumnName(QString());
        }
    }

    m_updatingMySqlControls = false;
}

bool DatabasePrintWidget::ensureMySqlSource()
{
    if (!m_mysqlSource) {
        m_mysqlSource = std::make_shared<MySqlDataSource>();
    }
    return static_cast<bool>(m_mysqlSource);
}

void DatabasePrintWidget::populateMySqlTableCombo(const QStringList &tables, bool preserveSelection)
{
    if (!m_mysqlTableCombo) {
        return;
    }

    QStringList uniqueTables;
    for (const QString &table : tables) {
        const QString trimmed = table.trimmed();
        if (!trimmed.isEmpty() && !uniqueTables.contains(trimmed, Qt::CaseInsensitive)) {
            uniqueTables.append(trimmed);
        }
    }
    if (uniqueTables.isEmpty() && !m_mysqlSource->tableName().isEmpty()) {
        uniqueTables.append(m_mysqlSource->tableName());
    }

    QSignalBlocker block(m_mysqlTableCombo);
    const QString previous = preserveSelection ? m_mysqlTableCombo->currentText() : QString();
    m_mysqlTableCombo->clear();
    m_mysqlTableCombo->addItems(uniqueTables);

    QString desired = m_mysqlSource ? m_mysqlSource->tableName() : QString();
    if (desired.isEmpty() && preserveSelection) {
        desired = previous;
    }

    int index = desired.isEmpty() ? -1 : m_mysqlTableCombo->findText(desired, Qt::MatchFixedString);
    if (index < 0 && m_mysqlTableCombo->count() > 0) {
        index = 0;
    }
    m_mysqlTableCombo->setCurrentIndex(index);
}

void DatabasePrintWidget::populateMySqlColumnCombo(const QStringList &columns, bool preserveSelection)
{
    if (!m_mysqlColumnCombo) {
        return;
    }

    QStringList uniqueColumns;
    for (const QString &column : columns) {
        const QString trimmed = column.trimmed();
        if (!trimmed.isEmpty() && !uniqueColumns.contains(trimmed, Qt::CaseInsensitive)) {
            uniqueColumns.append(trimmed);
        }
    }
    if (uniqueColumns.isEmpty() && !m_mysqlSource->columnName().isEmpty()) {
        uniqueColumns.append(m_mysqlSource->columnName());
    }

    QSignalBlocker block(m_mysqlColumnCombo);
    const QString previous = preserveSelection ? m_mysqlColumnCombo->currentText() : QString();
    m_mysqlColumnCombo->clear();
    m_mysqlColumnCombo->addItems(uniqueColumns);

    QString desired = m_mysqlSource ? m_mysqlSource->columnName() : QString();
    if (desired.isEmpty() && preserveSelection) {
        desired = previous;
    }

    int index = desired.isEmpty() ? -1 : m_mysqlColumnCombo->findText(desired, Qt::MatchFixedString);
    if (index < 0 && m_mysqlColumnCombo->count() > 0) {
        index = 0;
    }
    m_mysqlColumnCombo->setCurrentIndex(index);
}

void DatabasePrintWidget::onMySqlTestConnection()
{
    if (!ensureMySqlSource()) {
        return;
    }
    syncMySqlSourceFromUi();
    if (m_mysqlSource->testConnection()) {
        QMessageBox::information(this, tr("MySQL 数据源"), tr("连接成功"));
    } else {
        QMessageBox::warning(this, tr("MySQL 数据源"), m_mysqlSource->errorString());
    }
}

void DatabasePrintWidget::onMySqlRefreshTables()
{
    if (!ensureMySqlSource()) {
        return;
    }

    syncMySqlSourceFromUi();
    if (!m_mysqlSource->loadTables()) {
        QMessageBox::warning(this, tr("MySQL 数据源"), m_mysqlSource->errorString());
        return;
    }

    populateMySqlTableCombo(m_mysqlSource->tables(), false);
    const int currentIndex = m_mysqlTableCombo ? m_mysqlTableCombo->currentIndex() : -1;
    onMySqlTableChanged(currentIndex);
}

void DatabasePrintWidget::onMySqlTableChanged(int index)
{
    if (m_updatingMySqlControls) {
        return;
    }

    if (!m_mysqlTableCombo || index < 0) {
        return;
    }

    const QString tableName = m_mysqlTableCombo->itemText(index).trimmed();
    if (tableName.isEmpty()) {
        if (m_mysqlSource) {
            m_mysqlSource->setTableName(QString());
        }
        populateMySqlColumnCombo(QStringList(), false);
        return;
    }

    if (!ensureMySqlSource()) {
        return;
    }

    m_mysqlSource->setTableName(tableName);

    if (!m_mysqlSource->loadColumns(tableName)) {
        QMessageBox::warning(this, tr("MySQL 数据源"), m_mysqlSource->errorString());
        return;
    }

    populateMySqlColumnCombo(m_mysqlSource->columnsForTable(tableName), false);
    if (m_mysqlColumnCombo && m_mysqlColumnCombo->currentIndex() >= 0) {
        m_mysqlSource->setColumnName(m_mysqlColumnCombo->currentText());
    }
}

void DatabasePrintWidget::onMySqlColumnChanged(int index)
{
    if (m_updatingMySqlControls) {
        return;
    }

    if (!m_mysqlColumnCombo || index < 0) {
        return;
    }

    if (!ensureMySqlSource()) {
        return;
    }

    const QString columnName = m_mysqlColumnCombo->itemText(index).trimmed();
    m_mysqlSource->setColumnName(columnName);
}

void DatabasePrintWidget::onMySqlPreviewClicked()
{
    if (!ensureMySqlSource()) {
        return;
    }

    syncMySqlSourceFromUi();
    if (!m_mysqlSource->refresh()) {
        QMessageBox::warning(this, tr("MySQL 数据源"), m_mysqlSource->errorString());
        clearMySqlPreview();
        return;
    }

    refreshMySqlPreview();
}
