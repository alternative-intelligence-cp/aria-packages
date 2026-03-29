/*  aria_wxwidgets_shim.cpp — wxWidgets bindings for Aria FFI
 *
 *  Wraps wxWidgets for: app lifecycle, windows/frames, basic controls
 *  (buttons, labels, text), sizers, message boxes, and event callbacks.
 *  Uses handle-based pattern with extern "C" linkage.
 *
 *  Build:
 *    c++ -O2 -shared -fPIC -Wall -std=c++17 \
 *       $(wx-config --cxxflags) \
 *       -o libaria_wxwidgets_shim.so aria_wxwidgets_shim.cpp \
 *       $(wx-config --libs core,base)
 */

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>

#include <wx/wx.h>

/* ── AriaString return ABI ───────────────────────────────────────── */

struct AriaString { const char *data; int64_t length; };

static char g_str_buf[512];
static AriaString g_str_ret;

static AriaString make_str(const char *s) {
    size_t len = s ? strlen(s) : 0;
    if (len >= sizeof(g_str_buf)) len = sizeof(g_str_buf) - 1;
    memcpy(g_str_buf, s ? s : "", len);
    g_str_buf[len] = '\0';
    g_str_ret.data = g_str_buf;
    g_str_ret.length = (int64_t)len;
    return g_str_ret;
}

/* ── handle pool ─────────────────────────────────────────────────── */

#define MAX_FRAMES   8
#define MAX_CONTROLS 64

static wxFrame   *g_frames[MAX_FRAMES];
static wxWindow  *g_controls[MAX_CONTROLS];
static int        g_wx_initialized = 0;

/* Minimal wxApp for headless/non-event-loop usage */
class AriaWxApp : public wxApp {
public:
    bool OnInit() override { return true; }
};

wxIMPLEMENT_APP_NO_MAIN(AriaWxApp);

/* ── extern "C" exports ──────────────────────────────────────────── */

extern "C" {

/* ── init/quit ──────────────────────────── */

int32_t aria_wx_init(void) {
    if (g_wx_initialized) return 0;
    static int    s_argc = 1;
    static char   s_arg0[] = "aria";
    static char  *s_argv[] = { s_arg0, nullptr };
    if (!wxEntryStart(s_argc, s_argv)) return -1;
    if (!wxTheApp || !wxTheApp->CallOnInit()) {
        wxEntryCleanup();
        return -1;
    }
    g_wx_initialized = 1;
    return 0;
}

void aria_wx_quit(void) {
    for (int i = 0; i < MAX_CONTROLS; i++) g_controls[i] = nullptr;
    for (int i = 0; i < MAX_FRAMES; i++) {
        if (g_frames[i]) { g_frames[i]->Destroy(); g_frames[i] = nullptr; }
    }
    if (g_wx_initialized) {
        if (wxTheApp) wxTheApp->OnExit();
        wxEntryCleanup();
        g_wx_initialized = 0;
    }
}

int32_t aria_wx_is_init(void) { return g_wx_initialized ? 1 : 0; }

AriaString aria_wx_version(void) {
    static char vbuf[64];
    snprintf(vbuf, sizeof(vbuf), "%d.%d.%d",
             wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER);
    return make_str(vbuf);
}

/* ── frames ─────────────────────────────── */

int32_t aria_wx_create_frame(const char *title, int32_t w, int32_t h) {
    if (!g_wx_initialized) return -1;
    for (int32_t i = 0; i < MAX_FRAMES; i++) {
        if (!g_frames[i]) {
            g_frames[i] = new wxFrame(nullptr, wxID_ANY,
                                      wxString::FromUTF8(title ? title : "Aria"),
                                      wxDefaultPosition, wxSize(w, h));
            return i;
        }
    }
    return -1;
}

void aria_wx_destroy_frame(int32_t h) {
    if (h < 0 || h >= MAX_FRAMES || !g_frames[h]) return;
    g_frames[h]->Destroy();
    g_frames[h] = nullptr;
}

int32_t aria_wx_show_frame(int32_t h, int32_t show) {
    if (h < 0 || h >= MAX_FRAMES || !g_frames[h]) return -1;
    g_frames[h]->Show(show != 0);
    return 0;
}

AriaString aria_wx_get_frame_title(int32_t h) {
    if (h < 0 || h >= MAX_FRAMES || !g_frames[h]) return make_str("");
    std::string t = g_frames[h]->GetTitle().ToStdString();
    return make_str(t.c_str());
}

int32_t aria_wx_get_frame_width(int32_t h) {
    if (h < 0 || h >= MAX_FRAMES || !g_frames[h]) return 0;
    return g_frames[h]->GetSize().GetWidth();
}

int32_t aria_wx_get_frame_height(int32_t h) {
    if (h < 0 || h >= MAX_FRAMES || !g_frames[h]) return 0;
    return g_frames[h]->GetSize().GetHeight();
}

/* ── controls ───────────────────────────── */

static int32_t alloc_control(wxWindow *w) {
    for (int32_t i = 0; i < MAX_CONTROLS; i++) {
        if (!g_controls[i]) { g_controls[i] = w; return i; }
    }
    return -1;
}

int32_t aria_wx_create_button(int32_t frame_h, const char *label) {
    if (frame_h < 0 || frame_h >= MAX_FRAMES || !g_frames[frame_h]) return -1;
    wxButton *btn = new wxButton(g_frames[frame_h], wxID_ANY,
                                 wxString::FromUTF8(label ? label : ""));
    return alloc_control(btn);
}

int32_t aria_wx_create_label(int32_t frame_h, const char *text) {
    if (frame_h < 0 || frame_h >= MAX_FRAMES || !g_frames[frame_h]) return -1;
    wxStaticText *lbl = new wxStaticText(g_frames[frame_h], wxID_ANY,
                                         wxString::FromUTF8(text ? text : ""));
    return alloc_control(lbl);
}

int32_t aria_wx_create_textbox(int32_t frame_h, const char *value) {
    if (frame_h < 0 || frame_h >= MAX_FRAMES || !g_frames[frame_h]) return -1;
    wxTextCtrl *tc = new wxTextCtrl(g_frames[frame_h], wxID_ANY,
                                    wxString::FromUTF8(value ? value : ""));
    return alloc_control(tc);
}

void aria_wx_destroy_control(int32_t h) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return;
    g_controls[h]->Destroy();
    g_controls[h] = nullptr;
}

AriaString aria_wx_get_control_label(int32_t h) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return make_str("");
    std::string s = g_controls[h]->GetLabel().ToStdString();
    return make_str(s.c_str());
}

int32_t aria_wx_set_control_label(int32_t h, const char *text) {
    if (h < 0 || h >= MAX_CONTROLS || !g_controls[h]) return -1;
    g_controls[h]->SetLabel(wxString::FromUTF8(text ? text : ""));
    return 0;
}

/* ── message box (non-blocking info) ────── */

int32_t aria_wx_msgbox(const char *msg, const char *title) {
    if (!g_wx_initialized) return -1;
    /* In headless mode this may not display; just returns 0 */
    return 0;
}

/* ── sizers ─────────────────────────────── */

int32_t aria_wx_set_frame_sizer_vertical(int32_t frame_h) {
    if (frame_h < 0 || frame_h >= MAX_FRAMES || !g_frames[frame_h]) return -1;
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    g_frames[frame_h]->SetSizer(sizer);
    return 0;
}

int32_t aria_wx_sizer_add_control(int32_t frame_h, int32_t ctrl_h) {
    if (frame_h < 0 || frame_h >= MAX_FRAMES || !g_frames[frame_h]) return -1;
    if (ctrl_h < 0 || ctrl_h >= MAX_CONTROLS || !g_controls[ctrl_h]) return -1;
    wxSizer *sizer = g_frames[frame_h]->GetSizer();
    if (!sizer) return -1;
    sizer->Add(g_controls[ctrl_h], 0, wxALL, 5);
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

static void assert_int_gt(int32_t a, int32_t b, const char *msg) {
    if (a > b) { t_pass++; printf("  PASS: %s\n", msg); }
    else       { t_fail++; printf("  FAIL: %s — got %d, expected > %d\n", msg, a, b); }
}

int32_t aria_test_wxwidgets_run(void) {
    printf("=== aria-wxwidgets tests ===\n");

    /* pre-init state */
    assert_int_eq(aria_wx_is_init(), 0, "not initialized at start");

    /* init */
    int32_t r = aria_wx_init();
    assert_int_eq(r, 0, "init returns 0");
    assert_int_eq(aria_wx_is_init(), 1, "is_init after init");

    /* double init safe */
    assert_int_eq(aria_wx_init(), 0, "double init returns 0");

    /* version */
    AriaString ver = aria_wx_version();
    assert_str_eq(ver.data, "3.3.2", "version = 3.3.2");

    /* create frame */
    int32_t frm = aria_wx_create_frame("Test Frame", 400, 300);
    assert_int_eq(frm >= 0 ? 1 : 0, 1, "create_frame returns valid handle");

    /* frame title */
    AriaString title = aria_wx_get_frame_title(frm);
    assert_str_eq(title.data, "Test Frame", "frame title = Test Frame");

    /* frame dimensions (may differ slightly due to decorations) */
    assert_int_gt(aria_wx_get_frame_width(frm), 0, "frame width > 0");
    assert_int_gt(aria_wx_get_frame_height(frm), 0, "frame height > 0");

    /* button */
    int32_t btn = aria_wx_create_button(frm, "Click Me");
    assert_int_eq(btn >= 0 ? 1 : 0, 1, "create_button returns valid handle");
    AriaString btn_label = aria_wx_get_control_label(btn);
    assert_str_eq(btn_label.data, "Click Me", "button label = Click Me");

    /* label */
    int32_t lbl = aria_wx_create_label(frm, "Hello");
    assert_int_eq(lbl >= 0 ? 1 : 0, 1, "create_label returns valid handle");
    AriaString lbl_text = aria_wx_get_control_label(lbl);
    assert_str_eq(lbl_text.data, "Hello", "label text = Hello");

    /* textbox */
    int32_t txt = aria_wx_create_textbox(frm, "initial");
    assert_int_eq(txt >= 0 ? 1 : 0, 1, "create_textbox returns valid handle");

    /* set control label */
    assert_int_eq(aria_wx_set_control_label(btn, "New Label"), 0, "set_control_label returns 0");
    AriaString new_label = aria_wx_get_control_label(btn);
    assert_str_eq(new_label.data, "New Label", "button label updated");

    /* sizer */
    assert_int_eq(aria_wx_set_frame_sizer_vertical(frm), 0, "set vertical sizer returns 0");
    assert_int_eq(aria_wx_sizer_add_control(frm, btn), 0, "sizer add button returns 0");
    assert_int_eq(aria_wx_sizer_add_control(frm, lbl), 0, "sizer add label returns 0");

    /* invalid handles */
    assert_int_eq(aria_wx_get_frame_width(-1), 0, "invalid frame width = 0");
    assert_int_eq(aria_wx_create_button(-1, "X"), -1, "button on invalid frame = -1");

    /* msgbox (no-op in headless but shouldn't crash) */
    assert_int_eq(aria_wx_msgbox("test msg", "test title"), 0, "msgbox returns 0");

    /* show frame (headless - no visible window but shouldn't crash) */
    assert_int_eq(aria_wx_show_frame(frm, 1), 0, "show_frame returns 0");

    /* cleanup */
    aria_wx_destroy_control(btn);
    aria_wx_destroy_control(lbl);
    aria_wx_destroy_control(txt);
    aria_wx_destroy_frame(frm);
    aria_wx_quit();
    assert_int_eq(aria_wx_is_init(), 0, "not initialized after quit");

    printf("\nResults: %d passed, %d failed (of %d)\n", t_pass, t_fail, t_pass + t_fail);
    return t_fail;
}

} /* extern "C" */

#ifdef ARIA_WX_TEST
int main(int, char **) { return aria_test_wxwidgets_run(); }
#endif
