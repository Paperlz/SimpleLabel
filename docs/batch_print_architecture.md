# 批量打印功能架构设计

## 一、功能概述

批量打印允许用户为标签中的条码、二维码、文本元素关联数据源（批量导入或Excel表格），系统自动遍历数据源中的每条记录，生成并打印对应的标签。

**核心场景：**
- 批量打印商品条码标签（从Excel导入商品编码）
- 批量打印物流二维码（从文本批量导入订单号）
- 批量打印会员卡（文本姓名+条码会员号组合）

---

## 二、架构设计

### 2.1 整体架构图

```
┌─────────────────────────────────────────────────────────┐
│                     用户界面层                          │
├─────────────────────────────────────────────────────────┤
│ DatabasePrintWidget (数据源配置UI)                      │
│  ├─ 批量导入页：文本输入 + 分隔符选择                   │
│  └─ 表格导入页：文件选择 + Sheet/表头/起始行            │
│                                                         │
│ BatchPrintDialog (批量打印对话框)                        │
│  ├─ 数据源列表 + 打印范围 + 预览                        │
│  └─ 打印机设置 + 进度显示                               │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│                     业务逻辑层                          │
├─────────────────────────────────────────────────────────┤
│ DataSource (抽象数据源接口)                             │
│  ├─ BatchDataSource (批量导入数据源)                    │
│  │   └─ parse(text, delimiter) → QStringList           │
│  └─ TableDataSource (Excel表格数据源)                   │
│      └─ loadFile(path, sheet, headerRow, startRow)     │
│                                                         │
│ BatchPrintEngine (批量打印引擎)                          │
│  ├─ collectDataElements() → 收集使用数据源的元素       │
│  ├─ generateLabelForData(index) → 临时设置元素数据     │
│  └─ executePrint(printer, range) → 执行打印循环        │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│                     数据模型层                          │
├─────────────────────────────────────────────────────────┤
│ BarcodeElement / QRCodeElement / TextElement            │
│  ├─ m_dataSource : shared_ptr<DataSource>               │
│  ├─ m_useDataSource : bool                              │
│  └─ toJson() / fromJson() 序列化数据源配置              │
│                                                         │
│ BarcodeItem / QRCodeItem / TextItem                     │
│  ├─ setDataSourcePreview(data) → 预览数据               │
│  └─ 显示 [数据源] 标识                                  │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│                     第三方库层                          │
├─────────────────────────────────────────────────────────┤
│ QXlsx / xlsxio (Excel读取)                              │
│ QPrinter / QPainter (Qt打印框架)                        │
└─────────────────────────────────────────────────────────┘
```

### 2.2 核心类设计

#### 2.2.1 DataSource（抽象数据源）

```cpp
// src/core/datasource.h
class DataSource {
public:
    enum Type { Batch, Table };
    
    virtual ~DataSource() = default;
    virtual Type type() const = 0;
    virtual int count() const = 0;
    virtual QString at(int index) const = 0;
    virtual bool isValid() const = 0;
    virtual QString errorString() const = 0;
    
    virtual QJsonObject toJson() const = 0;
    static std::shared_ptr<DataSource> fromJson(const QJsonObject& json);
};
```

#### 2.2.2 BatchDataSource（批量导入）

```cpp
class BatchDataSource : public DataSource {
private:
    QString m_rawText;
    QString m_delimiter; // "\n", " ", 或自定义
    QStringList m_dataList;
    
public:
    void setText(const QString& text, const QString& delimiter);
    void parse(); // 拆分数据
    QStringList dataList() const { return m_dataList; }
};
```

#### 2.2.3 TableDataSource（Excel表格）

```cpp
class TableDataSource : public DataSource {
private:
    QString m_filePath;
    QString m_sheetName;
    int m_headerRow;
    int m_startRow;
    int m_columnIndex; // 使用哪一列数据
    QStringList m_dataList;
    
public:
    bool loadFile(const QString& path);
    QStringList sheetNames() const;
    void setSheet(const QString& name);
    void setStartRow(int row);
    QStringList preview(int maxRows = 10) const;
};
```

#### 2.2.4 BatchPrintEngine（打印引擎）

```cpp
class BatchPrintEngine : public QObject {
    Q_OBJECT
    
public:
    struct PrintTask {
        BarcodeElement* barcodeElement = nullptr;
        QRCodeElement* qrcodeElement = nullptr;
        TextElement* textElement = nullptr;
        std::shared_ptr<DataSource> dataSource;
    };
    
    void setScene(LabelScene* scene);
    void collectDataElements(); // 扫描场景中的数据元素
    bool hasTasks() const { return !m_tasks.isEmpty(); }
    int totalCount() const; // 数据总条数（取最大值）
    
    void executePrint(QPrinter* printer, int startIndex, int endIndex);
    void generatePreview(int index, QPixmap& output);
    
signals:
    void progress(int current, int total);
    void finished(bool success, const QString& message);
    
private:
    LabelScene* m_scene = nullptr;
    QList<PrintTask> m_tasks;
};
```

#### 2.2.5 BatchPrintDialog（批量打印对话框）

```cpp
class BatchPrintDialog : public QDialog {
    Q_OBJECT
    
public:
    BatchPrintDialog(LabelScene* scene, QWidget* parent = nullptr);
    
private slots:
    void onPreviewFirst();
    void onPreviewLast();
    void onPrint();
    
private:
    BatchPrintEngine* m_engine;
    QTableWidget* m_elementsTable; // 显示使用数据源的元素
    QSpinBox* m_startIndexSpin;
    QSpinBox* m_endIndexSpin;
    QLabel* m_previewLabel;
    QProgressBar* m_progressBar;
};
```

---

## 三、开发步骤（13个阶段）

### 阶段1：数据模型基础（2-3小时）
**目标**：定义数据源抽象接口和基础实现

- [ ] 创建 `src/core/datasource.h` 和 `.cpp`
- [ ] 实现 `DataSource` 抽象基类
- [ ] 实现 `BatchDataSource` 类和解析逻辑
- [ ] 编写单元测试验证分隔符拆分

**产出**：可独立测试的数据解析模块

---

### 阶段2：Element模型扩展（1-2小时）
**目标**：让元素模型支持数据源关联

- [ ] 在 `BarcodeElement/QRCodeElement/TextElement` 添加：
  ```cpp
  std::shared_ptr<DataSource> m_dataSource;
  bool m_useDataSource = false;
  ```
- [ ] 扩展 `toJson()` 序列化数据源配置
- [ ] 扩展 `fromJson()` 反序列化并重建数据源
- [ ] 更新 `.lbl` 文件格式版本

**产出**：元素模型支持保存/加载数据源

---

### 阶段3：批量导入实现（2-3小时）
**目标**：完整实现批量导入功能

- [ ] 完善 `BatchDataSource::parse()` 逻辑
- [ ] 支持回车、空格、自定义分隔符
- [ ] 处理空行、前后空白trim
- [ ] 添加数据验证（非空检查）
- [ ] 在 `DatabasePrintWidget` 连接批量输入UI

**产出**：批量导入数据源可用

---

### 阶段4：Excel库集成（3-4小时）
**目标**：集成第三方Excel读取库

- [ ] 选择库：推荐 `QXlsx`（纯Qt实现，MIT协议）
- [ ] 添加到 `third_party/` 并配置CMake
- [ ] 实现 `TableDataSource` 基础读取
- [ ] 测试读取.xlsx文件、获取sheet列表

**产出**：可以读取Excel文件

---

### 阶段5：Excel高级功能（2-3小时）
**目标**：支持sheet选择、表头、起始行

- [ ] 实现 `TableDataSource::setSheet()`
- [ ] 实现 `TableDataSource::setStartRow()`
- [ ] 实现列选择（如果多列需映射到多元素）
- [ ] 实现预览功能（返回前10行）

**产出**：Excel数据源功能完整

---

### 阶段6：UI与数据源绑定（3-4小时）
**目标**：DatabasePrintWidget与数据源交互

- [ ] 连接上传按钮，调用 `QFileDialog` 选择文件
- [ ] 文件选择后调用 `TableDataSource::loadFile()`
- [ ] 动态更新 `sheetCombo` 下拉列表
- [ ] 监听sheet/起始行变化，刷新预览表格
- [ ] 将配置的数据源保存到当前选中的Element

**产出**：用户可在UI配置数据源并预览

---

### 阶段7：Item显示数据源状态（1-2小时）
**目标**：图形项显示数据源指示

- [ ] 在 `BarcodeItem/QRCodeItem/TextItem::paint()` 中
- [ ] 当关联数据源时，绘制 `[数据源]` 小标签
- [ ] 添加 `setDataSourcePreview(index)` 方法
- [ ] 用于临时显示某条数据（不保存到Element）

**产出**：场景中可视化显示哪些元素使用了数据源

---

### 阶段8：批量打印引擎（4-5小时）
**目标**：实现核心打印循环逻辑

- [ ] 创建 `src/core/batchprintengine.h/.cpp`
- [ ] 实现 `collectDataElements()` 扫描场景
- [ ] 实现 `executePrint()` 打印循环：
  ```cpp
  for (int i = start; i <= end; ++i) {
      // 1. 临时设置所有数据元素为第i条数据
      // 2. 调用 scene->render() 渲染到printer
      // 3. 恢复元素原始状态
      // 4. 发射进度信号
  }
  ```
- [ ] 处理多元素共享数据源的情况
- [ ] 添加取消机制

**产出**：可以执行批量打印的核心引擎

---

### 阶段9：批量打印对话框UI（2-3小时）
**目标**：创建用户友好的打印配置界面

- [ ] 创建 `src/dialogs/batchprintdialog.h/.cpp`
- [ ] 添加元素列表表格（显示类型+数据源+数量）
- [ ] 添加打印范围选择（全部/自定义范围）
- [ ] 添加份数选择
- [ ] 添加打印机选择下拉
- [ ] 添加预览按钮和预览区域

**产出**：批量打印配置对话框

---

### 阶段10：预览功能（2-3小时）
**目标**：支持打印前预览

- [ ] 实现 `BatchPrintEngine::generatePreview(index)`
- [ ] 临时应用数据，渲染到QPixmap
- [ ] 在对话框中显示首页/末页/指定页预览
- [ ] 添加上一页/下一页导航

**产出**：用户可预览任意数据对应的标签

---

### 阶段11：MainWindow集成（1-2小时）
**目标**：添加批量打印入口

- [ ] 在文件菜单添加"批量打印..."动作
- [ ] 实现 `MainWindow::batchPrint()` 槽函数
- [ ] 检查场景中是否有数据源元素
- [ ] 如果没有，提示用户先配置；有则打开对话框
- [ ] 连接打印完成信号，显示结果消息

**产出**：完整的批量打印流程可用

---

### 阶段12：数据验证与错误处理（2-3小时）
**目标**：健壮性提升

- [ ] 实现 `DataSource::isValid()` 验证
- [ ] 检查文件是否存在、sheet是否有效
- [ ] 检查数据是否为空、格式是否正确
- [ ] 打印时跳过无效数据并记录
- [ ] 在UI中显示验证错误提示
- [ ] 添加日志记录（可选）

**产出**：异常情况下不崩溃，友好提示

---

### 阶段13：性能优化与体验优化（2-3小时）
**目标**：大数据量场景优化

- [ ] 添加打印进度条和取消按钮
- [ ] Excel数据缓存避免重复读取
- [ ] 大数据量时预览表格分页显示
- [ ] 保存最近使用的数据源路径
- [ ] 添加快捷键支持（Ctrl+Shift+P）
- [ ] 编写用户使用文档

**产出**：用户体验良好的最终版本

---

## 四、技术要点

### 4.1 Excel库选择

| 库名 | 优点 | 缺点 | 推荐度 |
|------|------|------|--------|
| **QXlsx** | 纯Qt实现，MIT协议，易集成 | 仅支持.xlsx | ⭐⭐⭐⭐⭐ |
| xlsxio | 轻量级，C实现 | 需要额外依赖 | ⭐⭐⭐ |
| LibXL | 功能强大 | 商业授权 | ⭐⭐ |

**推荐方案**：使用 QXlsx，仓库：https://github.com/QtExcel/QXlsx

### 4.2 数据应用策略

**临时应用模式**（推荐）：
```cpp
// 保存原始数据
QString originalData = item->data();

// 临时应用数据源数据
item->setDataTemporary(dataSource->at(i));

// 渲染
scene->render(&painter);

// 恢复原始数据
item->restoreData(originalData);
```

**优点**：不破坏用户设计，打印后场景恢复原样

### 4.3 性能考量

- **大数据量**：1000条数据 × 复杂标签 ≈ 30秒
- **优化点**：
  - 使用QPrinter的分页机制，避免重复初始化
  - 缓存数据源解析结果
  - 打印线程化（后期优化）
  - 预览时降低渲染分辨率

### 4.4 文件格式兼容

扩展 `.lbl` 格式示例：
```json
{
  "version": "1.1",
  "elements": [
    {
      "type": "barcode",
      "data": "默认数据",
      "dataSource": {
        "enabled": true,
        "type": "table",
        "filePath": "C:/data.xlsx",
        "sheet": "Sheet1",
        "startRow": 2,
        "columnIndex": 0
      }
    }
  ]
}
```

---

## 五、风险与注意事项

### 5.1 已知风险

1. **Excel文件格式多样性**：不同版本、合并单元格、公式计算
   - 缓解：初期仅支持简单表格，逐步增强
   
2. **大数据量性能**：数千条数据打印耗时长
   - 缓解：添加进度提示和取消机制

3. **打印机驱动兼容性**：不同打印机行为差异
   - 缓解：充分测试，提供打印预览

### 5.2 开发注意事项

- 每个阶段独立提交，便于回滚
- 优先实现核心流程，UI细节后期优化
- 保持向后兼容，旧版.lbl文件可正常打开
- 充分测试边界情况（空数据、单条、大量）

---

## 六、测试计划

### 6.1 单元测试
- DataSource解析准确性（各种分隔符）
- Element序列化/反序列化完整性
- BatchPrintEngine数据应用正确性

### 6.2 集成测试
- 批量导入 → 预览 → 打印流程
- Excel导入 → 多sheet选择 → 打印流程
- 多元素共享数据源场景

### 6.3 场景测试
| 场景 | 数据量 | 元素类型 | 预期结果 |
|------|--------|----------|----------|
| 商品条码 | 100条 | Barcode | 打印100张标签 |
| 物流二维码 | 500条 | QRCode | 批量生成快递单 |
| 会员卡 | 1000条 | Text+Barcode | 混合元素打印 |
| 空数据 | 0条 | Any | 提示无数据 |

---

## 七、时间估算

| 阶段 | 工作量 | 依赖 | 优先级 |
|------|--------|------|--------|
| 阶段1-3（批量导入） | 1-2天 | 无 | P0 核心 |
| 阶段4-5（Excel） | 1-2天 | 阶段1 | P0 核心 |
| 阶段6-7（UI绑定） | 1天 | 阶段2 | P0 核心 |
| 阶段8-10（打印引擎） | 2天 | 阶段1-7 | P0 核心 |
| 阶段11（集成） | 0.5天 | 阶段8-10 | P0 核心 |
| 阶段12-13（优化） | 1天 | 阶段11 | P1 增强 |

**总计**：5-7个工作日（含测试）

---

## 八、后续扩展方向

1. **数据库数据源**：支持连接MySQL/SQLite查询数据
2. **变量字段**：一个标签多个字段映射（姓名+编号+日期）
3. **条件打印**：根据数据内容决定是否打印
4. **模板复用**：保存常用数据源配置为模板
5. **打印统计**：记录打印历史和数据追溯

---

**文档版本**: v1.0  
**创建日期**: 2025年11月1日  
**作者**: GitHub Copilot  
**更新记录**: 初始版本
