# Third-party notices

LeyoChat includes or depends on the following third-party software. The Apache
License 2.0 in the root `LICENSE` file applies only to LeyoChat-owned code and
does not replace these licenses.

## ElaWidgetTools

- Project: https://github.com/Liniyous/ElaWidgetTools
- Copyright: Copyright (c) 2024 Liniyous
- License: MIT
- Local copy: vendored and modified from commit
  `2488f5d4a6bc65154d17aaaa3dc4714ba0ec3aa3`
- License text: `third_party/ElaWidgetTools/LICENSE`
- Provenance and modification notice: `third_party/ElaWidgetTools/UPSTREAM.md`

The bundled `ElaAwesome.ttf` is the font distributed by upstream
ElaWidgetTools. Its embedded metadata identifies it as generated with Fontello
and references the Open Font License. Upstream does not provide a separate
font provenance file. LeyoChat preserves the upstream attribution and includes
the SIL Open Font License 1.1 text at `LICENSES/OFL-1.1.txt`. More specific
upstream provenance remains desirable.

## md4c

- Project: https://github.com/mity/md4c
- Version: 0.5.3
- Copyright: Copyright (c) 2016-2024 Martin Mitáš
- License: MIT
- License text: `third_party/md4c/LICENSE.md`

## Inno Setup Chinese Simplified Translation

- Project: https://github.com/kira-96/Inno-Setup-Chinese-Simplified-Translation
- Copyright: Copyright (c) 2019-2020 kirakira
- License: MIT
- License text: `windows/ChineseSimplified.LICENSE`

## Qt

LeyoChat is built with Qt 6 and normally redistributes dynamically linked Qt
libraries. Qt open-source modules are available under the terms published by
The Qt Company, including LGPLv3 for the modules used by this project. Binary
distributors are responsible for satisfying the applicable Qt and LGPL
requirements, including license notices, replacement/relinking rights, and
corresponding-source obligations where applicable.

- Qt licensing: https://www.qt.io/licensing/open-source-lgpl-obligations
- Qt source: https://code.qt.io/cgit/qt/
- LGPLv3 text: `LICENSES/LGPL-3.0-only.txt`
- GPLv3 text incorporated by LGPLv3: `LICENSES/GPL-3.0-only.txt`

Qt WebEngine also incorporates Chromium and other third-party components.
When WebEngine is included in a binary package, its accompanying credits and
license materials must be distributed with that package.

## Inno Setup

Windows installers may be built with Inno Setup. Inno Setup is not vendored in
this repository. Its license permits commercial use and redistribution subject
to preservation of its notices and accurate attribution.

- Project and license: https://jrsoftware.org/isinfo.php

## Project-generated media

The default LeyoChat sticker animations, window background animations, and
neutral ElaWidgetTools placeholder images are generated deterministically by
`tools/generate_open_source_assets.py`. They do not incorporate external
photographs, illustrations, emoji fonts, or character artwork.
