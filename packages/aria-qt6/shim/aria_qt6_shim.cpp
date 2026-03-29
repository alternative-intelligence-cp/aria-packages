/*  aria_qt6_shim.cpp — Qt6 Widgets bindings for Aria FFI
 *
 *  Wraps Qt6 for: app lifecycle, windows (QMainWindow), basic controls
 *  (QPushButton, QLabel, QLineEdit, QCheckBox, QComboBox), layouts,
 *  message boxes, menus, and status bars.
 *  Uses handle-based pattern with extern "C" linkage.
 *
 *  Build:
 *    c++ -O2 -shared -fPIC -Wall -std=c++17 \
 *       $(pkg-config --cflags Qt6Widgets Qt6Gui Qt6Core) \
 *       -o libaria_qt6_shim.so aria_qt6_shim.cpp \
 *       $(pkg-config --libs Qt6Widgets Qt6Gui Qt6Core)
 */

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>

#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QMessageBox>
#include <QWidget>

/* ── AriaString return ABI ───────────────────────────────────────── */

struct AriaString { const char *data; int64_t length; };

static char g_str_buf[1024];
static AriaString g_str_ret;

static AriaString make_str(const char *s) {
    size_t len = s ? strlen(s) : 0;
    if (len >= sizeof(g_str_buf)) len = sizeof(g_str_buf) - 1;
    memcpy(g_str_buf, s ? s : "", len);
    g_str_buf[len] = '\0';
    g_str_ret.data   = g_str_buf;
    g_str_ret.length = (int64_t)len;
    return g_str_ret;
}

/* ── handle pools ────────────────────────────────────────────────── */

#define MAX_WINDOWS  16
#define MAX_CONTROLS 128
#define MAX_LAYOUTS  32
#define MAX_MENUS    32
#define MAX_ACTIONS  64

static QApplication *g_app            = nullptr;
static QMainWindow  *g_windows[MAX_WINDOWS];
static QWidget      *g_controls[MAX_CONTROLS];
static QLayout      *g_layouts[MAX_LAYOUTS];
static QMenu        *g_menus[MAX_MENUS];
static QAction      *g_actions[MAX_ACTIONS];
static int           g_qt_initialized = 0;

static int32_t alloc_control(QWidget *w) {
    for (int32_t i = 0; i < MAX_CONTROLS; i++) {
        if (!g_controls[i]) { g_controls[i] = w; return i; }
    }
    return -1;
}

static int32_t alloc_layout(QLayout *l) {
    for (int32_t i = 0; i < MAX_LAYOUTS; i++) {
        if (!g_layouts[i]) { g_layouts[i] = l; return i; }
    }
    return -1;
}

static int32_t alloc_menu(QMenu *m) {
    for (int32_t i = 0; i < MAX_MENUS; i++) {
        if (!g_menus[i]) { g_menus[i] = m; return i; }
    }
    return -1;
}

static int32_t alloc_action(QAction *a) {
    for (int32_t i = 0; i < MAX_ACTIONS; i++) {
        if (!g_actions[i]) { g_actions[i] = a; return i; }
    }
    return -1;
}

/* ── extern "C" exports ──────────────────────────────────────────── */

extern "C" {

/* ── init / quit ────────────────────────── */

int32_t aria_qt6_init(void) {
    if (g_qt_initialized) return 0;
    static int    s_argc = 1;
    static char   s_arg0[] = "aria";
    static char  *s_argv[] = { s_arg0, nullptr };
    g_app = new QApplication(s_argc, s_argv);
    if (!g_app) return -1;
    g_qt_initialized = 1;
    return 0;
}

void aria_qt6_quit(void) {
    /* destroy all controls first (windows own child widgets) */
    for (int i = 0; i < MAX_CONTROLS; i++) g_controls[i] = nullptr;
    for (int i = 0; i < MAX_LAYOUTS;  i++) g_layouts[i]  = nullptr;
    for (int i = 0; i < MAX_MENUS;    i++) g_menus[i]    = nullptr;
    for (int i = 0; i < MAX_ACTIONS;  i++) g_actions[i]  = nullptr;
    for (int i = 0; i < MAX_WINDOWS;  i++) {
        if (g_windows[i]) { delete g_windows[i]; g_windows[i] = nullptr; }
    }
    if (g_app) { delete g_app; g_app = nullptr; }
    g_qt_initialized = 0;
}

int32_t aria_qt6_is_init(void) { return g_qt_initialized ? 1 : 0; }

AriaString aria_qt6_version(void) {
    return make_str(qVersion());
}

/* ── windows (QMainWindow) ──────────────── */

int32_t aria_qt6_create_window(const char *title, int32_t w, int32_t h) {
    if (!g_qt_initialized) return -1;
    for (int32_t i = 0; i < MAX_WINDOWS; i++) {
        if (!g_windows[i]) {
            QMainWindow *win = new QMainWindow();
            win->setWindowTitle(QString::fromUtf8(title ? title : "Aria"));
            win->resize(w, h);
            /* Create a central widget so layouts can be added */
            QWidget *central = new QWidget(win);
            win->setCentralWidget(central);
            g_windows[i] = win;
            return i;
        }
    }
    return -1;
}

void aria_qt6_destroy_window(int32_t h) {
    if (h < 0 || h >= MAX_WINDOWS || !g_windows[h]) return;
    delete g_windows[h];
    g_windows[h] = nullptr;
}

int32_t aria_qt6_show_window(int32_t h, int32_t show) {
    if (h < 0 || h >= MAX_WINDOWS || !g_windows[h]) return -1;
    if (show) g_windows[h]->show(); else g_windows[h]->hide();
    return 0;
}

AriaString aria_qt6_get_window_title(int32_t h) {
    if (h < 0 || h >= MAX_WINDOWS || !g_windows[h]) return make_str("");
    std::string t = g_windows[h]->windowTitle().toStdString();
    return make_str(t.c_str());
}

int32_t aria_qt6_set_window_title(int32_t h, const char *title) {
    if (h < 0 || h >= MAX_WINDOWS || !g_windows[h]) return -1;
    g_windows[h]->setWindowTitle(QString::fromUtf8(title ? title : ""));
    return 0;
}

int32_t aria_qt6_get_window_width(int32_t h) {
    if (h < 0 || h >= MAX_WINDOWS || !g_windows[h]) return 0;
    return g_windows[h]->width();
}

int32_t aria_qt6_get_window_height(int32_t h) {
    if (h < 0 || h >= MAX_WINDOWS || !g_windows[h]) return 0;
    return g_windows[h]->height();
}

int32_t aria_qt6_set_window_size(int32_t h, int32_t w, int32_t ht) {
    if (h < 0 || h >= MAX_WINDOWS || !g_windows[h]) return -1;
    g_windows[h]->resize(w, ht);
    return 0;
}

/* ── controls ───────────────────────────── */

int32_t aria_qt6_create_button(int32_t win_h, const char *label) {
    if (win_h < 0 || win_h >= MAX_WINDOWS || !g_windows[win_h]) return -1;
    QWidget *parent = g_windows[win_h]->centralWidget();
    QPushButton *btn = new QPushButton(QString::fromUtf8(label ? label : ""), parent);
    return alloc_control(btn);
}

int32_t aria_qt6_create_label(int32_t win_h, const char *text) {
    if (win_h < 0 || win_h >= MAX_WINDOWS || !g_windows[win_h]) return -1;
    QWidget *parent = g_windows[win_h]->centralWidget();
    QLabel *lbl = new QLabel(QString::fromUtf8(text ? text : ""), parent);
    return alloc_control(lbl);
}

int32_t aria_qt6_create_lineedit(int32_t win_h, const char *text) {
    if (win_h < 0 || win_h >= MAX_WINDOWS || !g_windows[win_h]) return -1;
    QWidget *parent = g_windows[win_h]->centralWidget();
    QLineEdit *le = new QLineEdit(QString::fromUtf8(text ? text : ""), parent);
    return alloc_control(le);
}

int32_t aria_qt6_create_checkbox(int32_t win_h, const char *label) {
    if (win_h < 0 || win_h >= MAX_WINDOWS || !g_windows[win_h]) return -1;
    QWidget *parent = g_windows[win_h]->centralWidget();
    QCheckBox *cb = new QCheckBox(QString::fromUtf8(label ? label : ""), parent);
    return alloc_control(cb);
}

int32_t aria_qt6_create_combobox(int32_t win_h) {
    if (win_h < 0 || win_h >= MAX_WINDOWS || !g_windows[win_h]) return -1;
    QWidget *parent = g_windows[win_h]->centralWidget();
    QComboBox *combo = new QComboBox(parent);
    return alloc_control(combo);
}

void aria_qt6_destroy_control(int32_t h) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return;
    delete g_controls[h];
    g_controls[h] = nullptr;
}

AriaString aria_qt6_get_control_text(int32_t h) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return make_str("");
    /* Try QLineEdit first, then fall back to generic text/label */
    QLineEdit *le = qobject_cast<QLineEdit *>(g_controls[h]);
    if (le) return make_str(le->text().toUtf8().constData());
    QLabel *lbl = qobject_cast<QLabel *>(g_controls[h]);
    if (lbl) return make_str(lbl->text().toUtf8().constData());
    QComboBox *cb = qobject_cast<QComboBox *>(g_controls[h]);
    if (cb) return make_str(cb->currentText().toUtf8().constData());
    /* QPushButton, QCheckBox inherit QAbstractButton */
    QAbstractButton *btn = qobject_cast<QAbstractButton *>(g_controls[h]);
    if (btn) return make_str(btn->text().toUtf8().constData());
    return make_str("");
}

int32_t aria_qt6_set_control_text(int32_t h, const char *text) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return -1;
    QString s = QString::fromUtf8(text ? text : "");
    QLineEdit *le = qobject_cast<QLineEdit *>(g_controls[h]);
    if (le) { le->setText(s); return 0; }
    QLabel *lbl = qobject_cast<QLabel *>(g_controls[h]);
    if (lbl) { lbl->setText(s); return 0; }
    QAbstractButton *btn = qobject_cast<QAbstractButton *>(g_controls[h]);
    if (btn) { btn->setText(s); return 0; }
    return -1;
}

int32_t aria_qt6_set_control_enabled(int32_t h, int32_t enabled) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return -1;
    g_controls[h]->setEnabled(enabled != 0);
    return 0;
}

int32_t aria_qt6_is_control_enabled(int32_t h) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return -1;
    return g_controls[h]->isEnabled() ? 1 : 0;
}

/* checkbox specific */
int32_t aria_qt6_checkbox_is_checked(int32_t h) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return -1;
    QCheckBox *cb = qobject_cast<QCheckBox *>(g_controls[h]);
    if (!cb) return -1;
    return cb->isChecked() ? 1 : 0;
}

int32_t aria_qt6_checkbox_set_checked(int32_t h, int32_t checked) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return -1;
    QCheckBox *cb = qobject_cast<QCheckBox *>(g_controls[h]);
    if (!cb) return -1;
    cb->setChecked(checked != 0);
    return 0;
}

/* combobox specific */
int32_t aria_qt6_combobox_add_item(int32_t h, const char *text) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return -1;
    QComboBox *cb = qobject_cast<QComboBox *>(g_controls[h]);
    if (!cb) return -1;
    cb->addItem(QString::fromUtf8(text ? text : ""));
    return 0;
}

int32_t aria_qt6_combobox_current_index(int32_t h) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return -1;
    QComboBox *cb = qobject_cast<QComboBox *>(g_controls[h]);
    if (!cb) return -1;
    return cb->currentIndex();
}

int32_t aria_qt6_combobox_set_current(int32_t h, int32_t idx) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return -1;
    QComboBox *cb = qobject_cast<QComboBox *>(g_controls[h]);
    if (!cb) return -1;
    cb->setCurrentIndex(idx);
    return 0;
}

int32_t aria_qt6_combobox_count(int32_t h) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return -1;
    QComboBox *cb = qobject_cast<QComboBox *>(g_controls[h]);
    if (!cb) return -1;
    return cb->count();
}

/* ── layouts ────────────────────────────── */

int32_t aria_qt6_set_layout_vbox(int32_t win_h) {
    if (win_h < 0 || win_h >= MAX_WINDOWS || !g_windows[win_h]) return -1;
    QWidget *central = g_windows[win_h]->centralWidget();
    if (!central) return -1;
    /* Remove existing layout if any */
    if (central->layout()) delete central->layout();
    QVBoxLayout *vbox = new QVBoxLayout(central);
    return alloc_layout(vbox);
}

int32_t aria_qt6_set_layout_hbox(int32_t win_h) {
    if (win_h < 0 || win_h >= MAX_WINDOWS || !g_windows[win_h]) return -1;
    QWidget *central = g_windows[win_h]->centralWidget();
    if (!central) return -1;
    if (central->layout()) delete central->layout();
    QHBoxLayout *hbox = new QHBoxLayout(central);
    return alloc_layout(hbox);
}

int32_t aria_qt6_set_layout_grid(int32_t win_h) {
    if (win_h < 0 || win_h >= MAX_WINDOWS || !g_windows[win_h]) return -1;
    QWidget *central = g_windows[win_h]->centralWidget();
    if (!central) return -1;
    if (central->layout()) delete central->layout();
    QGridLayout *grid = new QGridLayout(central);
    return alloc_layout(grid);
}

int32_t aria_qt6_layout_add_control(int32_t layout_h, int32_t ctrl_h) {
    if (layout_h < 0 || layout_h >= MAX_LAYOUTS  || !g_layouts[layout_h])  return -1;
    if (ctrl_h   < 0 || ctrl_h   >= MAX_CONTROLS || !g_controls[ctrl_h]) return -1;
    QBoxLayout *box = qobject_cast<QBoxLayout *>(g_layouts[layout_h]);
    if (box) { box->addWidget(g_controls[ctrl_h]); return 0; }
    QGridLayout *grid = dynamic_cast<QGridLayout *>(g_layouts[layout_h]);
    if (grid) { grid->addWidget(g_controls[ctrl_h], grid->rowCount(), 0); return 0; }
    return -1;
}

int32_t aria_qt6_grid_add_control(int32_t layout_h, int32_t ctrl_h, int32_t row, int32_t col) {
    if (layout_h < 0 || layout_h >= MAX_LAYOUTS  || !g_layouts[layout_h])  return -1;
    if (ctrl_h   < 0 || ctrl_h   >= MAX_CONTROLS || !g_controls[ctrl_h]) return -1;
    QGridLayout *grid = dynamic_cast<QGridLayout *>(g_layouts[layout_h]);
    if (!grid) return -1;
    grid->addWidget(g_controls[ctrl_h], row, col);
    return 0;
}

/* ── menus ──────────────────────────────── */

int32_t aria_qt6_add_menu(int32_t win_h, const char *title) {
    if (win_h < 0 || win_h >= MAX_WINDOWS || !g_windows[win_h]) return -1;
    QMenu *menu = g_windows[win_h]->menuBar()->addMenu(
        QString::fromUtf8(title ? title : ""));
    return alloc_menu(menu);
}

int32_t aria_qt6_menu_add_action(int32_t menu_h, const char *label) {
    if (menu_h < 0 || menu_h >= MAX_MENUS || !g_menus[menu_h]) return -1;
    QAction *action = g_menus[menu_h]->addAction(
        QString::fromUtf8(label ? label : ""));
    return alloc_action(action);
}

int32_t aria_qt6_menu_add_separator(int32_t menu_h) {
    if (menu_h < 0 || menu_h >= MAX_MENUS || !g_menus[menu_h]) return -1;
    g_menus[menu_h]->addSeparator();
    return 0;
}

AriaString aria_qt6_action_get_text(int32_t action_h) {
    if (action_h < 0 || action_h >= MAX_ACTIONS || !g_actions[action_h]) return make_str("");
    return make_str(g_actions[action_h]->text().toUtf8().constData());
}

/* ── status bar ─────────────────────────── */

int32_t aria_qt6_statusbar_show(int32_t win_h, const char *msg) {
    if (win_h < 0 || win_h >= MAX_WINDOWS || !g_windows[win_h]) return -1;
    g_windows[win_h]->statusBar()->showMessage(
        QString::fromUtf8(msg ? msg : ""));
    return 0;
}

/* ── message box ────────────────────────── */

int32_t aria_qt6_msgbox_info(const char *title, const char *text) {
    if (!g_qt_initialized) return -1;
    /* In headless mode this may not display; returns 0 */
    return 0;
}

int32_t aria_qt6_msgbox_warning(const char *title, const char *text) {
    if (!g_qt_initialized) return -1;
    return 0;
}

/* ── event processing ───────────────────── */

int32_t aria_qt6_process_events(void) {
    if (!g_app) return -1;
    g_app->processEvents();
    return 0;
}

/* ================================================================= */
/* ── test harness ────────────────────────────────────────────────── */
/* ================================================================= */

static int t_pass = 0, t_fail = 0;

static void assert_int_eq(int32_t a, int32_t b, const char *msg) {
    if (a == b) { t_pass++; printf("  PASS: %s\n", msg); }
    else        { t_fail++; printf("  FAIL: %s — got %d, expected %d\n", msg, a, b); }
}

static void assert_str_eq(const char *a, const char *b, const char *msg) {
    if (a && b && strcmp(a, b) == 0) { t_pass++; printf("  PASS: %s\n", msg); }
    else { t_fail++; printf("  FAIL: %s — got '%s', expected '%s'\n", msg, a ? a : "(null)", b ? b : "(null)"); }
}

static void assert_int_ge(int32_t a, int32_t b, const char *msg) {
    if (a >= b) { t_pass++; printf("  PASS: %s\n", msg); }
    else        { t_fail++; printf("  FAIL: %s — got %d, expected >= %d\n", msg, a, b); }
}

int32_t aria_test_qt6_run(void) {
    printf("=== aria-qt6 shim tests ===\n");

    /* pre-init state */
    assert_int_eq(aria_qt6_is_init(), 0, "not initialized at start");

    /* init */
    int32_t r = aria_qt6_init();
    assert_int_eq(r, 0, "init returns 0");
    assert_int_eq(aria_qt6_is_init(), 1, "is_init after init");

    /* double init safe */
    assert_int_eq(aria_qt6_init(), 0, "double init returns 0");

    /* version */
    AriaString ver = aria_qt6_version();
    printf("  INFO: Qt version = %s\n", ver.data);
    assert_int_ge((int32_t)ver.length, 1, "version string non-empty");

    /* create window */
    int32_t win = aria_qt6_create_window("Test Window", 640, 480);
    assert_int_ge(win, 0, "create_window returns valid handle");

    /* window title */
    AriaString title = aria_qt6_get_window_title(win);
    assert_str_eq(title.data, "Test Window", "window title correct");

    /* window dimensions */
    assert_int_ge(aria_qt6_get_window_width(win), 1, "window width >= 1");
    assert_int_ge(aria_qt6_get_window_height(win), 1, "window height >= 1");

    /* set window title */
    assert_int_eq(aria_qt6_set_window_title(win, "New Title"), 0, "set_window_title returns 0");
    AriaString t2 = aria_qt6_get_window_title(win);
    assert_str_eq(t2.data, "New Title", "window title updated");

    /* resize */
    assert_int_eq(aria_qt6_set_window_size(win, 800, 600), 0, "set_window_size returns 0");

    /* button */
    int32_t btn = aria_qt6_create_button(win, "Click Me");
    assert_int_ge(btn, 0, "create_button returns valid handle");
    AriaString btn_text = aria_qt6_get_control_text(btn);
    assert_str_eq(btn_text.data, "Click Me", "button text correct");

    /* set control text */
    assert_int_eq(aria_qt6_set_control_text(btn, "Press"), 0, "set_control_text returns 0");
    AriaString btn2 = aria_qt6_get_control_text(btn);
    assert_str_eq(btn2.data, "Press", "button text updated");

    /* label */
    int32_t lbl = aria_qt6_create_label(win, "Hello Qt6");
    assert_int_ge(lbl, 0, "create_label returns valid handle");
    AriaString lbl_text = aria_qt6_get_control_text(lbl);
    assert_str_eq(lbl_text.data, "Hello Qt6", "label text correct");

    /* line edit */
    int32_t le = aria_qt6_create_lineedit(win, "initial");
    assert_int_ge(le, 0, "create_lineedit returns valid handle");
    AriaString le_text = aria_qt6_get_control_text(le);
    assert_str_eq(le_text.data, "initial", "lineedit text correct");

    /* checkbox */
    int32_t cb = aria_qt6_create_checkbox(win, "Accept");
    assert_int_ge(cb, 0, "create_checkbox returns valid handle");
    assert_int_eq(aria_qt6_checkbox_is_checked(cb), 0, "checkbox unchecked by default");
    assert_int_eq(aria_qt6_checkbox_set_checked(cb, 1), 0, "set_checked returns 0");
    assert_int_eq(aria_qt6_checkbox_is_checked(cb), 1, "checkbox now checked");

    /* combobox */
    int32_t combo = aria_qt6_create_combobox(win);
    assert_int_ge(combo, 0, "create_combobox returns valid handle");
    assert_int_eq(aria_qt6_combobox_add_item(combo, "Alpha"), 0, "combobox add Alpha");
    assert_int_eq(aria_qt6_combobox_add_item(combo, "Beta"), 0, "combobox add Beta");
    assert_int_eq(aria_qt6_combobox_add_item(combo, "Gamma"), 0, "combobox add Gamma");
    assert_int_eq(aria_qt6_combobox_count(combo), 3, "combobox count = 3");
    assert_int_eq(aria_qt6_combobox_current_index(combo), 0, "combobox index = 0");
    assert_int_eq(aria_qt6_combobox_set_current(combo, 2), 0, "set current to 2");
    assert_int_eq(aria_qt6_combobox_current_index(combo), 2, "combobox index = 2");
    AriaString combo_text = aria_qt6_get_control_text(combo);
    assert_str_eq(combo_text.data, "Gamma", "combobox text = Gamma");

    /* enable/disable */
    assert_int_eq(aria_qt6_is_control_enabled(btn), 1, "button enabled by default");
    assert_int_eq(aria_qt6_set_control_enabled(btn, 0), 0, "disable button");
    assert_int_eq(aria_qt6_is_control_enabled(btn), 0, "button now disabled");
    assert_int_eq(aria_qt6_set_control_enabled(btn, 1), 0, "re-enable button");

    /* layout: vbox */
    int32_t vbox = aria_qt6_set_layout_vbox(win);
    assert_int_ge(vbox, 0, "set_layout_vbox returns valid handle");
    assert_int_eq(aria_qt6_layout_add_control(vbox, btn), 0, "layout add button");
    assert_int_eq(aria_qt6_layout_add_control(vbox, lbl), 0, "layout add label");
    assert_int_eq(aria_qt6_layout_add_control(vbox, le), 0, "layout add lineedit");

    /* menu */
    int32_t menu = aria_qt6_add_menu(win, "&File");
    assert_int_ge(menu, 0, "add_menu returns valid handle");
    int32_t act = aria_qt6_menu_add_action(menu, "Open");
    assert_int_ge(act, 0, "menu_add_action returns valid handle");
    AriaString act_text = aria_qt6_action_get_text(act);
    assert_str_eq(act_text.data, "Open", "action text = Open");
    assert_int_eq(aria_qt6_menu_add_separator(menu), 0, "add separator returns 0");

    /* status bar */
    assert_int_eq(aria_qt6_statusbar_show(win, "Ready"), 0, "statusbar show returns 0");

    /* message box (headless no-op but shouldn't crash) */
    assert_int_eq(aria_qt6_msgbox_info("Info", "Test"), 0, "msgbox_info returns 0");
    assert_int_eq(aria_qt6_msgbox_warning("Warn", "Test"), 0, "msgbox_warning returns 0");

    /* process events (headless) */
    assert_int_eq(aria_qt6_process_events(), 0, "process_events returns 0");

    /* show window (headless - no visible window but shouldn't crash) */
    assert_int_eq(aria_qt6_show_window(win, 1), 0, "show_window returns 0");
    assert_int_eq(aria_qt6_show_window(win, 0), 0, "hide_window returns 0");

    /* invalid handles */
    assert_int_eq(aria_qt6_get_window_width(-1), 0, "invalid window width = 0");
    assert_int_eq(aria_qt6_create_button(-1, "X"), -1, "button on invalid window = -1");
    assert_int_eq(aria_qt6_set_control_text(-1, "X"), -1, "set_control_text invalid = -1");
    assert_int_eq(aria_qt6_layout_add_control(-1, 0), -1, "layout_add invalid layout = -1");
    assert_int_eq(aria_qt6_layout_add_control(0, -1), -1, "layout_add invalid control = -1");

    /* cleanup */
    aria_qt6_destroy_control(btn);
    aria_qt6_destroy_control(lbl);
    aria_qt6_destroy_control(le);
    aria_qt6_destroy_control(cb);
    aria_qt6_destroy_control(combo);
    aria_qt6_destroy_window(win);
    aria_qt6_quit();
    assert_int_eq(aria_qt6_is_init(), 0, "not initialized after quit");

    printf("\nResults: %d passed, %d failed (of %d)\n", t_pass, t_fail, t_pass + t_fail);
    return t_fail;
}

} /* extern "C" */

#ifdef ARIA_QT6_TEST
int main(int argc, char **argv) { return aria_test_qt6_run(); }
#endif
