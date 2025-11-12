# SimpleLabel / 简单标签

## English

SimpleLabel is a Qt-based label designer and batch printing tool. It helps teams create and manage label templates, preview data-driven designs, and export them to PDF or images with flexible layout controls.

### Features

- Visual editor for barcodes, QR codes, text, images, shapes, and tables.
- Template library with quick previews, tagging, and batch operations.
- Data binding for Excel/MySQL sources with preview navigation.
- Print Center for batch printing, auto layout export, and per-record export.
- Multilingual interface (Chinese / English) with dynamic translation loading.
 - Community-friendly template workflow ready for sharing and reuse.

### Quick Start

```powershell
# Prepare build directory
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Generate translations (English)
py -3 translations\generate_en_us.py
cmake --build build --target compile_qm

# Run (English UI)
.\build\Release\SimpleLabel.exe --lang en
```

### Project Structure

- `src/` – Core application (UI, dialogs, graphics, printing, data bindings).
- `translations/` – TSV mapping, TS/QM resources, translation tools.
- `tools/` – Utility scripts (translation scans, resource extraction).
- `third_party/` – Bundled dependencies (QXlsx, QR Code generator, zxing-cpp).

### Contributing

1. Fork the repository and create a feature branch.
2. Run formatting/lint tools (`clang-format`, etc.) before committing.
3. Ensure translations stay in sync (`generate_en_us.py` + `compile_qm`).
4. Contribute label templates: place `.lbl` files (and optional screenshots) under `templates/community/`, and describe them in `docs/template-gallery.md` or the pull request.
5. Submit a pull request with clear context and testing notes.

### Template Contributions

- Every template should include a short description (target label size, intended use, data source expectations).
- Add a preview image if possible (`png/jpg` under `templates/community/previews/`).
- Keep filenames self-descriptive, e.g. `shipping-4x6.lbl`.
- Note any dependencies (custom fonts, barcode standards) in the PR body or accompanying markdown.

### License

MIT License. See [LICENSE](LICENSE) for details. Third-party libraries retain their original licenses.

---

## 中文介绍

SimpleLabel 是一款基于 Qt 的标签设计与批量打印工具，支持模板管理、数据驱动预览以及多格式导出，可帮助团队快速生成可视化标签。

### 核心特性

- 标签设计器，支持条码、二维码、文本、图形、表格等元素。
- 模板中心提供标签预览、分类管理与批量操作。
- 支持 Excel / MySQL 数据源绑定，可实时预览记录。
- 打印中心提供批量打印、自动排版导出、单记录导出。
- 多语言界面（中文 / 英文），可在启动参数中指定语言。
- CMake + Qt + 现代 C++ 构建，易于扩展和集成。

### 快速开始

```powershell
# 生成构建目录
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 启动（英文界面）
.\build\Release\SimpleLabel.exe 

### 目录说明

- `src/` —— 应用主代码（界面、对话框、图形、打印、数据绑定）。
- `translations/` —— 翻译资源、TS/ QM 文件与辅助脚本。
- `tools/` —— 工具脚本，例如翻译扫描与资源处理。
- `third_party/` —— 外部依赖（QXlsx、QR Code generator、zxing-cpp 等）。

### 参与贡献

1. Fork 仓库并创建特性分支。
2. 提交前运行格式化/静态检查工具（如 clang-format）。
3. 保持翻译同步（执行 `generate_en_us.py` 和 `compile_qm`）。
4. 欢迎上传标签模板：将 `.lbl` 模板（可附截图）放入 `templates/community/`，并在 PR 或 `docs/template-gallery.md` 中写明用途与尺寸。
5. 提交 PR 时请附上修改说明、测试结果与截图（如适用）。

### 模板分享

- 请提供简短说明：标签规格、应用场景、数据源要求等。
- 如有预览图，请放在 `templates/community/previews/` 中并引用。
- 文件名建议体现用途，例如 `shipping-4x6.lbl`。
- 若依赖特定字体或条码标准，请在 PR 描述或附带文档中说明。

### 开源协议

采用 MIT 协议，详见 [LICENSE](LICENSE)。第三方库遵循其原始许可协议。
