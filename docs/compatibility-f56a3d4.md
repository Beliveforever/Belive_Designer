# BekUI f56a3d4 升级兼容性检查

- 旧版本：`cd25cea`
- 新版本：`f56a3d4`（origin/main，27 个提交）
- 构建：Release / Ninja / GL，macOS arm64
- 结果：本工程 **构建通过，无编译错误**；核心视图/外观/主题运行正常。

## 1. API 兼容性

新增 API 是加法，当前设计器没有使用被移除的接口：

- `App` 嵌入模式：`attachHost` / `step` / `draw` / `detachHost`、`TextureBackend`。
- `Context`、`services/`、`json_actions`、`json_data`、`sound_bank`。
- `AppearanceLayer` 新增 `clip`、`slice`。
- `Appearance` 新增契约校验、`sound` override。
- `ViewTemplate::instantiate` 增加可选 `conditions` / `data` 参数。
- `initTextureStore` 参数从 `RenderBackend*` 改为 `TextureBackend*`；设计器未调用。

没有发现当前 `src/` 使用的符号被删除或改名。

## 2. 行为变化

- `App::saveScreenshot` 现在按 `DisplayFramebufferScale` 读像素。Retina 上截图从
  1672×941 变成 3248×1882；这是**截图分辨率变化**，不是窗口布局变化。
- 新默认主题加入 `sound.json`、`schemaVersion`、新 view 示例和音效资源；
  设计器打开主题、预览页面仍正常。
- 新增 JSON 能力：`foreach`、`visibleIf`、`onClick`、vars 数值表达式、
  view `id`、layer `clip` / image `slice`、外观 `requires`。
- `foreach` 依赖 `JsonData` 数据源；设计器用 `createFromJSONString` 预览会报
  “foreach 需要数据源”，这是**运行期数据依赖**，不是崩溃。

## 3. 本轮适配

- 视图预览不再删除合法的 `id` 字段；节点树使用 `id` 显示标签，检查器增加 `ID` 行。
- 外观检查器增加 `clip` 和 image `slice` 字段。
- 视图检查器增加 `onClick` / `visibleIf` / `foreach` 行。
- 其余新能力仍属于部分支持（见 `docs/json-syntax-audit.md`）：
  - `foreach` 没有数据源注入/预览。
  - `visibleIf` 对象形式没有专用编辑器。
  - `onClick` 没有行为注册 UI。
  - vars 表达式没有编辑器求值显示。
  - `requires`、`sound`、`schemaVersion` 没有 UI。

## 4. 回归验证

| 场景 | 结果 |
|---|---|
| 空启动 | 正常 |
| 示例工程 view | 正常渲染 |
| default 主题 view | 正常 |
| 外观 base/button/button.bordered | 正常渲染、节点树/检查器 |
| include 视图 | 正常展开 |
| 外观 import 递归/点击源文件 | 正常 |
| 节点树创建/导入外观 | 正常，导入前编译校验 |
| 深色主题切换 | 正常 |
| 截图 | 正常，Retina 2x 像素 |

## 5. 已知风险

- 新主题新增的 `sound` / `foreach` / `visibleIf` / `onClick` 超出当前设计器 UI 覆盖范围；
  预览与编辑仍部分依赖代码页或运行期数据。
- 截图分辨率变化会影响按旧尺寸校验的文档/CI；若需要固定尺寸，应在截图后缩放。
- 上游主题里带有 `.bak` 跟踪文件（非本工程生成）；本工程设计器会忽略 `.bak`。
