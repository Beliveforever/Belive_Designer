# 实施验证

运行环境：macOS 26.6.2 (arm64)，CMake 4.4.3，Ninja，GL 后端，Release 构建。  
BekUI 子模块：`f56a3d4`。

## 构建

```bash
cmake -S . -B build/gl -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/gl
```

结果：`dist/designer` 链接成功，0 error。

## 已实际验证

| 流程 | 方式 | 结果 |
|---|---|---|
| 空启动 | 不带 `--project` 启动 | 空界面 + 打开/复制工程欢迎页，不再自动打开示例工程 |
| 原生打开工程 | 欢迎页「打开工程」 | 成功唤起 SDL/macOS 原生文件夹选择器 |
| 复制并新建工程 | 临时验证入口实际复制 | 生成 `my_project_copy`，更新 theme.json name/description，打开副本 |
| 打开主题工程 | `--project projects/my_project` | 读取 theme.json、tokens、view，打开 `view/home.json` |
| 打开外部主题 | `--project imgui_learn/resources/theme/default` | 自动打开 `view/page_json.json`（不再落到空白 Padding），全局变量页签自动显示 |
| 外观 JSON | 项目树打开 `base/button/button.bordered.json` | 画布直接渲染；节点树递归展开 import/layers；点击导入节点打开源文件并定位图层 |
| 根节点数字编辑 | 选中 view 根节点，检查器 width 改 900 并保存 | JSON 写入 `"width": 900` |
| 根节点取消引用 | style 页背景 `@theme/color.bg` 点 × | 写回解析值 `#0F172A` |
| 外观属性编辑 | 检查器把 box.width 改为 320 并保存 | JSON 写入 `"width": 320`，预览重编译 |
| 导入节点编辑 | 点击节点树的导入节点 | 打开源文件并选中对应 layer |
| 外观创建图层 | 节点树 + → 矩形层 | 新图层写入 JSON、选中并预览 |
| 外观导入图层 | 节点树 + → 导入外观… → 选择 JSON | 校验编译通过后追加 import 层；id 冲突会拒绝并提示 |
| import 递归 | 打开 `base/box.component.json` | `import` 数组两项都展开，嵌套 glass 层继续展开 |
| 外观 children | 打开 `base/children_demo.json` | 子视图实际创建；节点树递归显示 children |
| 视图 include | 打开 `view/json_include_main.json` | 相对 resources 解析并展开预览 |
| foreach 预览 | 打开 `view/json_foreach.json` | 注入 3 个示例项并渲染；画布提示示例数据 |
| onClick 预览 | 打开 `view/json_action.json` | 空行为注册，预览不报未注册 |
| bind/visibleIf 预览 | 打开 `view/json_binding.json` | 注入示例字段/可见条件 |
| 主题 schema/sound | 打开 `theme.json` | 清单显示 schemaVersion/sound/behavior |
| 主题/变量 JSON | 打开 `tokens/colors.json` / `theme.json` | 中央显示变量表/主题清单，代码页可编辑 |
| bklv 原生视图 | 画布渲染 | 页面由 `type` + 平铺属性 + children 实例化；无自定义 style 包装 |
| 全局变量 | 「全局变量」页签 | 显示 color.* / metric.* 的键、值、来源文件、引用数 |
| 局部变量 | 「局部变量」页签（默认） | 显示当前 JSON 的 vars.colors / vars.numbers、解析值、引用数 |
| 新建局部变量 | 变量面板 + | 写入根 vars.colors/numbers，画布与列表刷新 |
| 局部变量引用 | 属性行 chain / 拖拽 | 写入 `&color.…` / `&number.…`，画布更新 |
| 全局变量引用 | 属性行 chain / 拖拽 | 写入 `@theme/color.…` / `@style/metric.…`，画布更新 |
| 属性检查器 | 选中 Label/Box/Image | 只有「属性 / 样式」两个页签；字段与 bklv 属性表一致 |
| 代码预览 | 左侧代码页签 + 编辑 + 应用 | 顶层 vars 与平铺属性均可编辑；解析错误显示在面板底部 |
| 画布点选 | 点击画布节点 | 节点树 / 检查器 / 画布选中联动 |
| 撤销/重做 | 编辑后快捷键 | 文档属性、节点、局部变量、代码应用均有撤销 |
| 浅色主题 | 默认 | 启动即为浅色主题 |
| 深色主题 | 设置 / 调色板图标 | VS Code Dark+ 配色（#1E1E1E / #252526 / #007ACC 等） |
| 大基础字号 | 默认 | 界面默认 1.15×；设置菜单仍可切小/标准/大与 ±5% |
| 菜单悬停隔离 | 打开菜单后鼠标移动 | 面板 hover 被 `hovering()` 阻断；点击/滚轮由 overlay 接管，不再穿透 |
| 1280×800 | 窗口截图 | 面板、画布、检查器不重叠 |

截图：`docs/screenshots/welcome.png`、`main.png`、`local-vars.png`、`global-vars.png`、
`style.png`、`code.png`、`light.png`、`compact.png`、`default-theme.png`、
`default-appearance.png`。

## 已知边界

- 原生文件选择器的完整人工点击流程无法在无头截图中自动完成；目录选择器已验证能唤起，
  复制函数通过临时入口验证了实际复制/改名/打开。
- bklv 的高层控件 Slider、Cell 家族、RecyclerView、Dropdown、Header、TabFrame、
  SelectorButton、Shell/Pager 需要主题外观或运行期数据，本轮未做成节点；Button 已接入。
- 视图 JSON 的 `include`、`bind`、`bindTo` 未做属性编辑器；编辑原始 JSON 仍可用。
- Button 外观由 bklv 主题决定，设计器的背景/边框字段对它不生效。
- 全局 token 编辑不进入文档撤销栈；局部变量和文档属性有撤销。
- 代码编辑器在左侧窄栏，长行不折行。
- 外观属性检查器编辑已有 box/layer 字段，暂不支持新增/删除/排序图层；states 在代码页编辑。
- 递归展开的 import 内容只读；循环导入/缺失文件只标注，不继续展开。
