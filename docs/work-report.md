# Belive Designer 工作报告（第二轮）

日期：2026-10-23  
BekUI 子模块：`cd25cea`  
构建：Release / Ninja / GL，`dist/designer`

## 1. 结论

上一版设计器的数据模型是自定义的：节点属性写在 `style` 里，变量用 `$ref` / `$var`
对象引用，令牌来自自定义 `tokens/*.json`。这套格式**不是 bklv 的视图 JSON 语法**。

本轮按以下真实资料重做了数据层：

- `docs/json-syntax.md`（bklv 视图 / 外观 / 主题 / 文案 JSON 的完整语法）
- `resources/theme/default/` 下的 `theme.json`、`tokens/colors.json`、`tokens/metrics.json`、
  `view/*.json`、`base/*.json`

现在示例工程、变量面板、属性检查器、画布预览都使用 bklv 能直接解析的格式。

## 2. 真实语法与修复

### 2.1 视图 JSON

bklv 的视图节点是**平铺属性**：

```json
{ "type": "Box", "axis": "row", "gap": 12,
  "backgroundColor": "@theme/color.surface",
  "children": [ { "type": "Label", "text": "Hello", "textColor": "@theme/color.text" } ] }
```

旧工程是：

```json
{ "type": "Box", "style": { "direction": "row", "gap": 12,
  "background": { "$ref": "color.surface" } } }
```

两者不兼容。本轮已把 `projects/my_project` 的所有页面/组件转换为平铺 bklv 视图 JSON，
并移除 `style`、`$ref`、`$var`、`id`、`name`、自定义 `page/gameCard/navItem` 类型
（统一落到 bklv 的 `Box`，Label/Image/Button 保持真实类型）。

### 2.2 全局变量

真实全局变量 = 主题 token：

- 颜色：`tokens/colors.json`，拍平为 `color.*`
- 数值：`tokens/metrics.json`，拍平为 `metric.*`
- 引用：颜色 `@theme/color.…`，数值 `@style/metric.…`
- 主题文件由 `theme.json` 的 `files[]` 登记；工程目录以 `theme.json` 为识别依据

变量面板的「全局变量」页签现在显示这些真实 token，并支持把引用拖到属性行。
全局 token 的双击编辑会写回 `tokens/*.json` 并重新装载主题。

### 2.3 局部变量

真实局部变量 = **当前 JSON 顶层**唯一一个 `vars`：

```json
"vars": {
  "colors": { "accent": "@theme/color.accent" },
  "numbers": { "row": 44, "radius": 14 }
}
```

引用是 `&color.accent` / `&number.row`，作用域是整份文档，**不是每个节点各自的 vars**。

变量面板默认显示「局部变量」，只列 `vars.colors` / `vars.numbers`，显示解析后的颜色/数值、
来源文件和引用次数；支持新建、改名、删除，引用会随改名同步更新。
属性检查器的「变量」页签已移除，变量只保留在这一处。

### 2.4 图片和主题

- `@res/…` 优先解析到当前工程的 `resources/`；找不到时回退到程序 `dist/resources/`。
- 预览用 `bklv::setUiScale(canvasZoom)` 后直接 `createFromJSONString`，不再手写属性映射。
- 因此 `@theme/`、`@style/`、`&`、`@i18n/`、`@icon/`、`@res/` 与 bklv 运行时行为一致。

## 3. 界面调整

### 3.1 变量面板

- 标题改为「变量」，顶部有「局部变量 / 全局变量」两个作用域页签。
- 下面有「颜色 / 数值」两个类型页签。
- 默认「局部变量」；没有局部变量时显示空状态。
- 底部详情显示引用字符串、解析值、来源文件和引用次数。

### 3.2 属性检查器

- 移除「变量」页签，只保留「属性 / 样式」。
- 属性字段改为 bklv 原名：`axis`、`direction`、`justifyContent`、`alignItems`、
  `backgroundColor`、`textColor`、`cornerRadius`、`borderThickness`、`clipsToBounds`、
  `scalingType`、`alpha` 等。
- 不再写 `style` 对象；编辑直接写平铺属性。
- 属性行仍支持从变量面板拖入引用或通过 chain 选择局部/全局引用。

### 3.3 打开工程与复制

启动不再自动打开示例工程：

1. 空界面显示欢迎卡片。
2. 「打开工程」：原生文件夹选择器 → 必须包含 `theme.json` → 读取主题/视图 → 打开第一个 view。
3. 「复制并新建工程」：原生选择源工程 → 原生选择目标父目录 → 复制为
   `<源目录名>_copy`（重名自动加序号）→ 更新 `theme.json` 的 name/description → 打开副本。

文件菜单 / `Cmd+O` 同样走原生选择器。

### 3.4 主题与字号

- 默认主题改为浅色；深色主题保留 VS Code Dark+ 配色：`#1E1E1E` 窗口、`#252526` 面板、
  `#007ACC` 强调色、灰阶边框与 `#D4D4D4` 主文字。
- 界面基础字号默认改为大号（`textScale = 1.15`）；设置菜单仍可切换小/标准/大和 ±5%。

### 3.5 打开 default 主题时的空画布 / 空变量面板

默认主题的 `view/` 里第一份文件是 `children_ref_demo.json`，内容只有一个 `Padding`，
既没有 children 也没有局部变量；旧的“取字母序第一个 view + 默认局部变量页签”会让画布
看起来全空、变量面板也没有内容，容易被误判为 JSON 没读取。

修复：

- 入口选择改为评分：优先 `home.json`、`page*`，优先 `Box + children`，降低
  `children_ref/include/list_item/Padding` 的权重。default 主题现在打开 `view/page_json.json`。
- 打开文档时若当前 JSON 没有顶层 `vars`，变量面板自动切到「全局变量」；有局部变量时
  保持默认「局部变量」。
- `base/`、`demo/` 下的外观/组合 JSON 不再显示代码兜底：设计器用
  `AppearanceView::applyAppearanceJson()` 按当前主题编译并直接渲染，`import`、`layers`、
  `box`、`states` 都生效；透明区域加棋盘格衬底。
- 修复检查器根节点不能编辑的问题（`selectedNode` 为空是合法的根指针）：view 根节点的数字/枚举/颜色/引用现在都能改，引用旁的 × 可取消引用。
- 外观检查器所有标量行可编辑；引用值旁增加 ×，写回解析后的字面量。
- 外观节点树支持创建 rect/text/image/shadow/glass/custom 图层、从工程外观 JSON 导入、删除/复制图层；新增/导入前先编译校验，id 冲突直接拒绝，不会留下坏外观。
- 外观文档的节点树递归解析 `layers[]` 和 `import`：支持字符串和数组两种 import，
  导入文件继续展开其 layers/import；循环导入标“循环导入”，缺失/解析失败也会标注。
  导入节点可点击打开源文件并定位到对应层；本文件图层选中后属性检查器滚到对应字段。
- 外观文档的属性检查器改为可编辑：`box`（尺寸/布局/背景/边框）和 `layers[]` 的
  `op/id/fill/stroke/radius/fontSize/inset/data_id/font_kind/...` 都直接写 JSON 并实时重编译。
  颜色走取色器，枚举走下拉，布尔值走开关。
- `theme.json` 在画布中央显示主题清单；`tokens/` 显示颜色/数值变量表；代码页仍可编辑。
- 修复打开文档后使用已 move 的 `std::unique_ptr<Document>` 引起的崩溃。

### 3.6 菜单穿透 bug

`DesignerApp::hovering(rect)` 在任何菜单/弹窗/对话框打开时返回 false。所有面板的 hover
判断统一走它，因此下拉菜单打开时，鼠标不会再点亮/触发下层面板。点击与滚轮本来就由
`overlayInput` 接管。

## 4. bklv 字段审查结果

### 已覆盖

| 范围 | 字段 |
|---|---|
| 公共 | width/height、min/max、margin 四边、grow/shrink、alignSelf、position、top/left/right/bottom、visibility、backgroundColor、borderColor、borderThickness、cornerRadius、alpha、clipsToBounds |
| Box | axis、direction、alignItems、justifyContent、flexWrap、alignContent、flexBasis、gap/gapRow/gapColumn、padding 四边 |
| Label | text、fontSize、textColor、singleLine、lineHeight、horizontalAlign、verticalAlign |
| Image | src、cornerRadius、scalingType、scale、offsetX/offsetY、placeholderColor |
| Button | text、icon、iconFont、variant、enabled |
| 保留键 | type、children；include/bind/bindTo 可在代码页编辑 |

### 未做成独立节点

Slider、Cell/DetailCell/BooleanCell、RecyclerView、Dropdown、Header、TabFrame/TabItem、
SelectorButton、Rectangle、Shell/Pager、Padding 之外的复合控件。原因：它们依赖主题外观、
数据源或 C++ 控制器，光有 JSON 字段无法形成可用编辑体验；本轮不伪造。

`include`、`bind`、`bindTo`、事件/动作、动画/转场没有专门的属性 UI；保留原始 JSON 编辑。

## 5. 验证

实际运行：

- Release 构建 0 error。
- 空启动 + 欢迎页；原生文件夹选择器能唤起。
- `--project projects/my_project` 读取 `theme.json`、tokens、view，打开 `view/home.json`。
- 打开外部真实主题 `imgui_learn/resources/theme/default`：自动打开 `view/page_json.json`，
  全局变量页签自动显示；打开 `base/button/button.bordered.json` 画布直接渲染主题外观 Button。
- bklv 原生渲染示例页面；无 `style` / `$ref` / `$var`。
- 局部变量：`bgColor/metaColor/titleColor/radius` 显示与引用；新建 `color1` 后画布/列表刷新。
- 全局变量：color.* / metric.* 列表与 `@theme` / `@style` 引用。
- 属性检查器只剩两个页签；样式页背景显示 `@theme/color.bg` 引用。
- 代码页签预览/应用平铺 JSON；错误提示。
- 大字号默认；浅色主题默认；深色 VS Code 配色仍可切换。
- 临时验证入口实际复制 `projects/my_project` 到新目录，改名并打开副本。
- 菜单打开时面板 hover 被阻断。

## 6. 已知限制

- 原生文件选择器的完整点击流程无法在无头截图里自动完成；只能验证对话框唤起和复制函数。
- Button 外观由 bklv 主题决定，设计器背景/边框字段对它无效。
- 全局 token 编辑不进入文档撤销栈；文档/局部变量有撤销。
- 代码编辑器窄栏内不折行。
- 复制工程会复制整个目录（跳过 .git/build/dist/.bak/.tmp），大工程可能较慢；没有进度条。
- 工程识别只看 `theme.json`；没有校验 `files[]` 中每个文件是否存在。
- 外观预览使用占位文本（Button/Title/Info 等）；图片、Custom 层和控件运行期数据不参与预览。
- 外观属性检查器编辑已有 box/layer 字段，暂不支持新增/删除/排序图层；states 仍在代码页编辑。
- 节点树递归展开的导入节点点击后打开源文件编辑；导入循环/缺失只做标注。

## 7. 改动文件

- `src/core/model.*`、`src/core/document.*`：工程/主题 token/局部 vars/平铺视图/引用解析。
- `src/ui/preview.cpp`：原生 bklv 预览、@res 路径、uiScale。
- `src/ui/panels.cpp`：变量面板、检查器两页签、平铺字段、欢迎页、菜单 hover 隔离。
- `src/ui/app.cpp`：空启动、原生打开/复制、变量操作、主题/字号、复制流程。
- `src/ui/ui.cpp`：VS Code Dark+ 调色板。
- `src/main.cpp`：默认大字号、空启动。
- `projects/my_project/`：标准 theme/tokens/view/resources 结构。
- `README.md`、`docs/verification.md`、`docs/screenshots/`。
