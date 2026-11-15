/*
 * tphp/tui - src/tui_app.c - Task 7 App 层补全 + 测试面
 *
 * App 核心生命周期（frame/quit/setTitle/setSize/destroy 等 17 个方法）留在
 * tui.c；本文件补：截图/系统对话框/openUrl、主题与查询、拖放查询，以及
 * 测试面（输入注入 + lastRect/getRect，支撑无头窗口的确定性交互测试）。
 *
 * PNG 写出：stb_image_write 已随 CUI 软渲染编入 CUI 静态库且符号全局导出
 * （cui_render_sw.c 使用 stbi_write_png），此处 extern 声明复用，不 vendor 第二份。
 */
#include "tui_util.h"

#include <cui/cui_render.h>

#include <stdio.h>
#include <string.h>

#define RAISE(vm, cls, ...) tphp_vm_raise(vm, cls, __VA_ARGS__)

/* ---- 截图 / PNG ---- */

/* 渲染尺寸 = 逻辑尺寸 × dpi（向上取整并留 1px 余量，避免舍入欠采样） */
static bool shot_dims(cui_app *app, int *bw, int *bh) {
    cui_vec2 sz = cui_app_size(app);
    /* dpi 走 ui：app 持有 ctx —— 由调用方传入 ui 更直接，这里经 app 内部取 */
    if (sz.x <= 0 || sz.y <= 0) return false;
    *bw = (int)sz.x + 1;
    *bh = (int)sz.y + 1;
    return true;
}

static TVal nm_save_png(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::savePng()";
    const char *path = tui_arg_str(vm, args, n, 0, who);
    cui_app *app = tui_need_app(vm, (TObj *)self, who);
    int bw = 0, bh = 0;
    if (!shot_dims(app, &bw, &bh)) return TV_FALSE;
    cui_ctx *ui = tui_need_ui(vm, (TObj *)self, who);
    float dpi = cui_dpi_scale(ui);
    int pw = (int)((float)bw * dpi) + 1;
    int ph = (int)((float)bh * dpi) + 1;
    size_t bytes = (size_t)pw * (size_t)ph * 4;
    uint8_t *rgba = tm_malloc(bytes);
    if (!cui_app_screenshot(app, rgba, pw, ph)) {
        tm_free(rgba);
        return TV_FALSE;
    }
    int ok = cui_write_png(path, rgba, pw, ph, pw * 4);
    tm_free(rgba);
    return ok ? TV_TRUE : TV_FALSE;
}

/* screenshotRgba → ['w'=>,'h'=>,'data'=>binary string] */
static TVal nm_screenshot_rgba(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::screenshotRgba()";
    cui_app *app = tui_need_app(vm, (TObj *)self, who);
    cui_ctx *ui = tui_need_ui(vm, (TObj *)self, who);
    int bw = 0, bh = 0;
    if (!shot_dims(app, &bw, &bh))
        RAISE(vm, "LogicException", "%s(): window has no size yet", who);
    float dpi = cui_dpi_scale(ui);
    int pw = (int)((float)bw * dpi) + 1;
    int ph = (int)((float)bh * dpi) + 1;
    size_t bytes = (size_t)pw * (size_t)ph * 4;
    uint8_t *rgba = tm_malloc(bytes);
    if (!cui_app_screenshot(app, rgba, pw, ph)) {
        tm_free(rgba);
        RAISE(vm, "LogicException", "%s(): screenshot buffer rejected", who);
    }
    TStr *s = tstr_new((const char *)rgba, bytes);
    tm_free(rgba);
    TArr *a = tarr_new();
    tarr_set(a, TV_STR(tstr_new("w", 1)), TV_INT(pw));
    tarr_set(a, TV_STR(tstr_new("h", 1)), TV_INT(ph));
    tarr_set(a, TV_STR(tstr_new("data", 4)), TV_STR(s));
    return TV_ARR(a);
}

/* ---- 系统对话框 / URL ---- */

/* fileDialog(kind=0 open/1 save/2 folder) → 路径字符串，取消返回 false */
static TVal nm_file_dialog(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::fileDialog()";
    cui_app *app = tui_need_app(vm, (TObj *)self, who);
    int64_t kind = tui_opt_int(vm, args, n, 0, 0, who, "kind");
    const char *title = tui_opt_str(vm, args, n, 1, "", who, "title");
    const char *filter_name = tui_opt_str(vm, args, n, 2, "", who, "filterName");
    const char *filter = tui_opt_str(vm, args, n, 3, "", who, "filter");
    const char *default_name = tui_opt_str(vm, args, n, 4, "", who, "defaultName");
    if (kind < 0 || kind > 2)
        RAISE(vm, "ValueError", "%s(): kind must be 0 (open), 1 (save) or 2 (folder)", who);
    cui_file_dialog_opts o;
    memset(&o, 0, sizeof o);
    o.kind = (cui_file_dialog_kind)kind;
    o.title = title[0] ? title : NULL;
    o.filter_name = filter_name[0] ? filter_name : NULL;
    o.filter = filter[0] ? filter : NULL;
    o.default_name = default_name[0] ? default_name : NULL;
    char out[4096];
    out[0] = '\0';
    if (!cui_app_file_dialog(app, &o, out, sizeof out)) return TV_FALSE;
    TStr *s = tstr_new(out, strlen(out));
    return TV_STR(s);
}

static TVal nm_open_url(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::openUrl()";
    (void)self;
    const char *url = tui_arg_str(vm, args, n, 0, who);
    return cui_app_open_url(url) ? TV_TRUE : TV_FALSE;
}

/* ---- 主题 / 查询 ---- */

static TVal nm_set_theme(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::setTheme()";
    int64_t mode = tui_arg_int(vm, args, n, 0, who, "mode");
    if (mode < 0 || mode > 1)   /* 0 light, 1 dark */
        RAISE(vm, "ValueError", "%s(): mode must be 0 (light) or 1 (dark)", who);
    cui_set_theme(tui_need_ui(vm, (TObj *)self, who), (cui_theme_mode)mode);
    return TV_NULL;
}

static TVal nm_set_accent(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::setAccent()";
    int64_t accent = tui_arg_int(vm, args, n, 0, who, "accent");
    if (accent < 0 || accent >= CUI_ACCENT_COUNT)
        RAISE(vm, "ValueError", "%s(): accent must be in 0..%d", who,
              (int)CUI_ACCENT_COUNT - 1);
    cui_set_accent(tui_need_ui(vm, (TObj *)self, who), (cui_accent)accent);
    return TV_NULL;
}

static TVal nm_set_reduced_motion(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::setReducedMotion()";
    bool reduced = tui_arg_bool(vm, args, n, 0, who, "reduced");
    cui_set_reduced_motion(tui_need_ui(vm, (TObj *)self, who), reduced);
    return TV_NULL;
}

static TVal nm_dpi_scale(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::dpiScale()";
    return TV_FLOAT(cui_dpi_scale(tui_need_ui(vm, (TObj *)self, who)));
}

static TVal nm_size(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::size()";
    cui_vec2 sz = cui_app_size(tui_need_app(vm, (TObj *)self, who));
    TArr *a = tarr_new();
    tarr_set(a, TV_STR(tstr_new("w", 1)), TV_INT((int64_t)sz.x));
    tarr_set(a, TV_STR(tstr_new("h", 1)), TV_INT((int64_t)sz.y));
    return TV_ARR(a);
}

static TVal nm_backend(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::backend()";
    const char *b = cui_app_backend(tui_need_app(vm, (TObj *)self, who));
    TStr *s = tstr_new(b, strlen(b));
    return TV_STR(s);
}

/* droppedFiles：本帧拖入的路径数组（拷贝） */
static TVal nm_dropped_files(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::droppedFiles()";
    cui_ctx *ui = tui_need_ui(vm, (TObj *)self, who);
    int cnt = cui_dropped_file_count(ui);
    TArr *a = tarr_new();
    for (int i = 0; i < cnt; i++) {
        const char *p = cui_dropped_file(ui, i);
        if (p) tarr_append(a, TV_STR(tstr_new(p, strlen(p))));
    }
    return TV_ARR(a);
}

/* ---- 测试面：输入注入 + 矩形查询 ---- */

static TVal nm_simulate_mouse(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::simulateMouse()";
    double x = tui_arg_float(vm, args, n, 0, who, "x");
    double y = tui_arg_float(vm, args, n, 1, who, "y");
    cui_input_mouse_move(tui_need_ui(vm, (TObj *)self, who), (float)x, (float)y);
    return TV_NULL;
}

static TVal nm_simulate_mouse_button(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::simulateMouseButton()";
    int64_t button = tui_opt_int(vm, args, n, 0, 0, who, "button");
    bool down = tui_opt_bool(vm, args, n, 1, true, who, "down");
    if (button < 0 || button > 2)
        RAISE(vm, "ValueError", "%s(): button must be 0/1/2 (left/right/middle)", who);
    cui_input_mouse_button(tui_need_ui(vm, (TObj *)self, who),
                           (cui_mouse_button)button, down);
    return TV_NULL;
}

static TVal nm_simulate_key(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::simulateKey()";
    int64_t key = tui_arg_int(vm, args, n, 0, who, "key");
    bool down = tui_opt_bool(vm, args, n, 1, true, who, "down");
    int64_t mods = tui_opt_int(vm, args, n, 2, 0, who, "mods");
    if (key < 0)
        RAISE(vm, "ValueError", "%s(): key must be >= 0 (cui_key enum value)", who);
    cui_input_key(tui_need_ui(vm, (TObj *)self, who), (cui_key)key, down, (int)mods);
    return TV_NULL;
}

static TVal nm_simulate_text(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::simulateText()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_input_text(tui_need_ui(vm, (TObj *)self, who), s);
    return TV_NULL;
}

/* lastRect → ['x'=>,'y'=>,'w'=>,'h'=>]（最近绘制组件的屏幕矩形） */
static TVal nm_last_rect(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::lastRect()";
    cui_rect r = cui_last_rect(tui_need_ui(vm, (TObj *)self, who));
    TArr *a = tarr_new();
    tarr_set(a, TV_STR(tstr_new("x", 1)), TV_FLOAT(r.x));
    tarr_set(a, TV_STR(tstr_new("y", 1)), TV_FLOAT(r.y));
    tarr_set(a, TV_STR(tstr_new("w", 1)), TV_FLOAT(r.w));
    tarr_set(a, TV_STR(tstr_new("h", 1)), TV_FLOAT(r.h));
    return TV_ARR(a);
}

/* ---- 注册 ---- */

void tui_register_app_layer(TClass *cls) {
    tui_cls_add_native(cls, "savePng", (void *)nm_save_png, false);
    tui_cls_add_native(cls, "screenshotRgba", (void *)nm_screenshot_rgba, false);
    tui_cls_add_native(cls, "fileDialog", (void *)nm_file_dialog, false);
    tui_cls_add_native(cls, "openUrl", (void *)nm_open_url, false);
    tui_cls_add_native(cls, "setTheme", (void *)nm_set_theme, false);
    tui_cls_add_native(cls, "setAccent", (void *)nm_set_accent, false);
    tui_cls_add_native(cls, "setReducedMotion", (void *)nm_set_reduced_motion, false);
    tui_cls_add_native(cls, "dpiScale", (void *)nm_dpi_scale, false);
    tui_cls_add_native(cls, "size", (void *)nm_size, false);
    tui_cls_add_native(cls, "backend", (void *)nm_backend, false);
    tui_cls_add_native(cls, "droppedFiles", (void *)nm_dropped_files, false);
    tui_cls_add_native(cls, "simulateMouse", (void *)nm_simulate_mouse, false);
    tui_cls_add_native(cls, "simulateMouseButton", (void *)nm_simulate_mouse_button, false);
    tui_cls_add_native(cls, "simulateKey", (void *)nm_simulate_key, false);
    tui_cls_add_native(cls, "simulateText", (void *)nm_simulate_text, false);
    tui_cls_add_native(cls, "lastRect", (void *)nm_last_rect, false);
}
