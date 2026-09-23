// Pure, Win32-free decision for the dock's DWM window-border colour
// (mirrored 1:1 in taskbar-quick-pin.wh.cpp).
//
// Windows 11 (build 22000+) paints a thin border around a window's rounded
// frame. DWMWA_BORDER_COLOR recolours it: DWMWA_COLOR_NONE removes the border
// entirely, DWMWA_COLOR_DEFAULT restores the system default. The "hideDockBorder"
// user setting drives this so the grey outline Windows draws around the dock can
// be made invisible without touching any of the dock's own painting.

#ifndef TASKBAR_QUICK_PIN_DOCK_BORDER_H
#define TASKBAR_QUICK_PIN_DOCK_BORDER_H

#ifndef DWMWA_COLOR_NONE
#define DWMWA_COLOR_NONE 0xFFFFFFFEu
#endif
#ifndef DWMWA_COLOR_DEFAULT
#define DWMWA_COLOR_DEFAULT 0xFFFFFFFFu
#endif

// hideBorder ON  -> DWMWA_COLOR_NONE    : DWM draws NO border (grey outline gone).
// hideBorder OFF -> DWMWA_COLOR_DEFAULT : the system default border.
static inline unsigned int DockBorderColor(bool hideBorder) {
    return hideBorder ? DWMWA_COLOR_NONE : DWMWA_COLOR_DEFAULT;
}

#endif  // TASKBAR_QUICK_PIN_DOCK_BORDER_H
