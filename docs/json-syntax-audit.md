# bklv JSON 语法支持审计

对照：`BekUI/docs/json-syntax.md`（966 行，四类 JSON + 扩展层 + children + Recycler）  
结论：**不能确认“全部支持”**。核心的视图/主题/外观语法已覆盖，但手册中仍有明确未做项。

状态：✅ 已支持　◐ 部分支持　✗ 未支持

## 1. 通用取值/引用

| 语法 | 状态 | 说明 |
|---|---|---|
| 数字 / `"12"` / `auto` / `50%` | ✅ | bklv 实例化；尺寸字段检查器可输入 |
| `#RRGGBB[AA]` / `@theme/color` | ✅ | 取色器 + 文本；预览按 bklv 解析 |
| `@style/metric` | ✅ | 变量面板/拖拽/检查器 |
| `@res/` | ✅ | 预览重写到工程 `resources/` |
| `@icon/` | ◐ | 预览由 bklv 解析；检查器没有图标选择器 |
| `@i18n/` | ◐ | 预览用程序自带 i18n；**不加载/编辑工程自己的 i18n 目录** |
| `vars` / `&color` / `&number` | ✅ | 局部变量面板、检查器引用、预览 |
| `$` 键 | ✅ | 作为普通保留键保留；不做编辑 UI |
| `json_policy` / 诊断 | ✗ | 没有策略设置/诊断面板；错误只经日志/预览提示 |

## 2. 视图描述 JSON

| 语法 | 状态 | 说明 |
|---|---|---|
| `type` / `children` | ✅ | 画布、节点树、代码页 |
| `include` | ◐ | 预览会展开（含循环检测）；节点树不展开，检查器只显示说明 |
| `bind` / `bindTo` | ✗ | 需要运行期数据源；设计器不绑定/预览数据 |
| `id` | ✅ | 预览保留；节点树标签/检查器可编辑 |
| `visibleIf` / `foreach` / `onClick` | ◐ | 预览交给 bklv；检查器只有字符串行，无数据源/行为注册 UI |
| vars 数值表达式 | ◐ | 预览 bklv 求值；检查器显示原始值 |
| Box/Row 容器字段 | ✅ | axis/direction/align*/justify/gap*/padding*/flex*/margin 等 |
| 公共字段 | ✅ | 尺寸/定位/可见性/背景/边框/圆角/alpha/裁剪 |
| Label / Image / Button 字段 | ✅ | 检查器覆盖手册字段 |
| AppearanceView 内容槽 | ◐ | 预览支持 text/icon/info；检查器没有通用内容槽编辑 |
| ScrollingFrame / HScrollingFrame / ProgressSpinner / Padding | ◐ | bklv 可预览；检查器只有通用 Box 字段，没有各自专属 UI |
| Recycler | ✗ | 需要 C++ 注册数据源/binder；设计器没有数据源面板 |
| 新增类型菜单 | ◐ | 只有 Box/Label/Image/Button；其它类型靠原始 JSON |

## 3. 控件外观 JSON

| 语法 | 状态 | 说明 |
|---|---|---|
| `import` 字符串/数组、递归、循环保护 | ✅ | 节点树递归展开；点击导入节点打开源文件并定位图层 |
| 层 import 字符串/数组 | ✅ | 展开；点击可打开源文件 |
| `box` | ◐ | 尺寸/布局/背景/边框/圆角/grow/shrink/measureContent/padding/margin；`padding`/`margin` 单值/数组可编辑 |
| `layers` 通用字段 | ◐ | op/id/fill/stroke/radius/strokeWidth/color/fontSize/inset/offset/align/valign/opacity/clip/width/height 等；**gradient 仅预览，无编辑 UI** |
| `states` | ◐ | 预览初值；没有状态切换/关键帧编辑器 |
| `transitions` | ◐ | 预览初值；没有事件/通道编辑与播放控制 |
| `children` | ◐ | 预览建子视图；节点树递归；检查器没有子节点属性编辑 |
| `glass` | ✅ | 预览；fill/opacity/radius/inset 可编辑 |
| image `slice` 九宫格 | ✅ | 预览；检查器可编辑 slice 四项 |
| 外观 `requires` / sound override | ✗ | 无 UI |
| 扩展层 | ◐ | bklv 注册后可预览；检查器没有扩展参数模式 UI |
| 新增/删除/复制图层 | ✅ | 节点树 + 菜单；导入前编译校验 |
| 排序图层 | ✗ | 新 rect/glass 插在内容层前，其余追加；无拖拽排序 |

## 4. 主题 JSON

| 语法 | 状态 | 说明 |
|---|---|---|
| `theme.json` / `files[]` | ✅ | 打开工程、显示清单、复制工程更新 name/description |
| `tokens/colors.json` / `metrics.json` | ✅ | 全局变量表、编辑、重载主题 |
| `theme.json behavior` / `schemaVersion` / `sound` | ✗ | 无 UI；sound 资源不参与编辑 |
| `themes.json` 多主题清单 | ✗ | 设计器以单个 `theme.json` 为工程 |
| `colorScheme` | ◐ | 读取/显示；不驱动设计器主题 |

## 5. 文案 JSON

| 语法 | 状态 | 说明 |
|---|---|---|
| i18n 加载/编辑 | ✗ | 没有 i18n 文件树或键值编辑器；预览使用程序资源 |
| `$files` | ✗ | 同上 |

## 6. 扩展/运行时

| 能力 | 状态 |
|---|---|
| Recycler 数据源/itemBinder/onEnd | ✗ |
| 事件/动作/bind 运行期行为 | ✗ |
| JSON 错误策略/诊断面板 | ✗ |
| 多主题切换（themes.json） | ✗ |

## 本轮补齐

- 外观检查器 `box.backgroundColor` 键修正（此前错误写成 `background`）。
- 外观 `children` 预览实际建子视图；节点树递归展开 `children`。
- 外观 `import` 支持字符串/数组递归，循环/缺失/解析失败标注。
- 视图 `include` 预览展开（相对工程/主题 resources 解析，循环检测）；节点树暂不展开。
- 默认主题改为浅色。
- 节点树选中外观图层后检查器自动滚到对应字段。
- 修复 view 根节点不能编辑（空 pointer 是合法根节点）：数字/枚举/颜色/引用均可改。
- 引用属性增加 × 取消引用；view 与外观都支持。
- 点击导入节点打开源文件并定位到对应图层。
- 适配 BekUI f56a3d4：保留 view `id`、检查器 ID 行、layer `clip`/`slice`、`onClick`/`visibleIf`/`foreach` 行。

## 结论

**核心的视图/外观/主题编辑与预览已支持；但手册中的 Recycler 数据、`bind`/`bindTo`、工程 i18n、`states`/`transitions` 编辑、gradient 编辑、扩展层参数 UI、include 节点树、JSON 错误策略仍是缺口。**

如果要求“全部语法都可编辑”，目前还不能确认；需要按上表继续补。
