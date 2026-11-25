# Plan: Wire up Save menu (wuac)

## Objective
Make File > Save / Save As write the URL, Node ID and window position to a `.cfg` file, and reduce the existing auto-save to tracking recently used files only.

## Scope
- In scope: `wuac` project (`mainwindow.cpp/.h/.ui`); Save, Save As, `.cfg` load helper, recent-files list.
- Out of scope: `qtcon-ua-sub`; security settings; chart settings.

## Approach
1. Connect `actionSave` -> `saveConfig` and `actionSave_As` -> `saveConfigAs`; track `m_cfgPath`. Save with no path falls back to Save As.
2. Save As uses `QFileDialog::getSaveFileName` (filter `Config files (*.cfg)`, append `.cfg` if missing).
3. Write with `QSettings(path, QSettings::IniFormat)`: `connection/url`, `connection/nodeId`, `window/geometry` (position and size) and `window/pos`; call `sync()` and show a `QMessageBox` if `status() != NoError`.
4. Reduce auto-save: the default `QSettings()` keeps only `recent/files` (QStringList, max 10, most recent first, deduplicated, missing files dropped on load). Remove the default-settings read/write of geometry, URL and Node ID from the constructor and `closeEvent`.
5. Add `addRecentFile(path)` (called after Save / Save As) and `loadConfig(path)` (applies URL, Node ID, geometry; sets `m_cfgPath`).
6. Add a File > Recent submenu rebuilt from `recent/files` that calls `loadConfig`.

## Tasks
- [ ] Add `#include <QFileDialog>`, slots, `m_cfgPath`, recent helpers to `mainwindow.h/.cpp`
- [ ] Implement `saveConfig`, `saveConfigAs`, `loadConfig`
- [ ] Implement recent-files list and Recent submenu (`mainwindow.ui`)
- [ ] Strip URL/Node ID/geometry auto-save from constructor and `closeEvent`

## Validation
- [ ] wuac builds
- [ ] Save writes a `.cfg` containing URL, Node ID and position
- [ ] Default ini contains only `recent/files`
- [ ] Restart does not auto-restore URL/Node ID/position
- [ ] Selecting a recent entry reloads the saved config

## Risks and open questions
- "Position" is assumed to mean window position/size, not chart position.
- Recent submenu is included as the way to use the recent list; drop it if unwanted.

## Completion criteria
- [ ] All validation items pass
