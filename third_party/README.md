# 第三方库

本目录存放以源码方式嵌入的第三方库，每个库一个子目录。

| 库 | 版本 | 许可证 | 用途 |
|----|------|--------|------|
| md4c | 0.5.3 | MIT | Markdown → HTML 解析器 |
| ElaWidgetTools | 2.0.0-based, modified | MIT | Qt Widgets UI components |

ElaWidgetTools is vendored because LeyoChat carries compatibility changes that
are not present in the referenced upstream commit. See
`ElaWidgetTools/UPSTREAM.md` and `ElaWidgetTools/LICENSE`.
