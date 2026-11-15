/*
 * tphp/tui - CUI 1.4.0 原生封装（type=ext，Windows win32 后端）
 *
 * 对外暴露单个原生类 Tphp\Tui\App（立即模式，API 形态参考 teb 的对象风格）：
 *
 *   $app = new App(title, width, height, hidden=false, continuous=false);
 *   while ($app->frame()) {            // 渲染上一帧 + 等事件 + 开新帧；关窗返回 false
 *       $app->columnBegin(32, 16);     // 布局：columnBegin(pad,gap) / rowBegin(gap)
 *       $app->heading("Hello", 2);     // 文本：heading / secondary / text / label
 *       $on = $app->toggle("On", $on); // 控件：button() 返回是否被点，toggle() 返回新值
 *       $app->boxEnd();                // 结束最近一次 columnBegin/rowBegin
 *   }
 *   $app->destroy();                   // 幂等；对象释放时 native_dtor 兜底
 *
 * 约定：
 *   - 组件调用只发生在 frame() 返回 true 的绘制阶段内（立即模式每帧重放）；
 *     窗口句柄存 props[0]（TV_INT 地址），destroy 后再调用一律抛 Error（不静默）。
 *   - 单线程：frame() 阻塞等待事件，与 teb run() 同为主线程阻塞式 API。
 *   - 只 vendor 了 win32 后端（cui/src/platform 三个 .c），Linux/macOS 未纳入。
 *
 * 架构（Task 1 拆分）：本文件 = App 核心 + 类构建（tui_register_app_class）；
 * 共享助手在 tui_util.h/.c；6 个域文件（tui_widgets/form/layout/charts/
 * content/app.c）各带一个空注册函数，Task 2-7 的方法在对应域内追加。
 */
#include "pkg.h"       /* tphp_ext_reg_const_str / tphp_ext_reg_const_int */
#include "tui_util.h"  /* Task 1 助手层：参数/App 句柄/类注册/数组转换
                        * （自包含 class.h + value.h + opcode.h + cui_app.h） */

#include <stdint.h>
#include <string.h>

#define RAISE(vm, cls, ...) tphp_vm_raise(vm, cls, __VA_ARGS__)
#define WHO_CTOR  "Tphp\\Tui\\App::__construct"
#define WHO_FRAME "Tphp\\Tui\\App::frame()"
#define WHO_BTN   "Tphp\\Tui\\App::button()"
#define WHO_TOG   "Tphp\\Tui\\App::toggle()"

/* ---- 域注册占位（各域 .c 定义；Task 2-7 的方法在对应域文件内追加） ---- */
extern void tui_register_widgets(TClass *cls);
extern void tui_register_form(TClass *cls);
extern void tui_register_layout(TClass *cls);
extern void tui_register_charts(TClass *cls);
extern void tui_register_content(TClass *cls);
extern void tui_register_app_layer(TClass *cls);

/* ---- App 句柄（props[0] = TV_INT 地址；
 *      tui_app_of / tui_need_app / tui_need_ui 已迁至 tui_util.h） ---- */

static void app_set(TObj *o, cui_app *a) {
    int s = tphp_class_find_prop(o->cls, "app");
    tval_release(&o->props[s]);
    o->props[s] = TV_INT((int64_t)(intptr_t)a);
}

/* ---- native 方法 ---- */

static TVal nm_app_construct(VM *vm, void *self, TVal *args, int n) {
    TObj *o = (TObj *)self;
    const char *title = "CUI";
    int64_t w = 1080, h = 720;
    bool hidden = false, cont = false;

    if (n >= 1) title = tui_arg_str(vm, args, n, 0, WHO_CTOR);
    if (n >= 2) w = tui_arg_int(vm, args, n, 1, WHO_CTOR, "width");
    if (n >= 3) h = tui_arg_int(vm, args, n, 2, WHO_CTOR, "height");
    if (n >= 4) hidden = tui_arg_bool(vm, args, n, 3, WHO_CTOR, "hidden");
    if (n >= 5) cont = tui_arg_bool(vm, args, n, 4, WHO_CTOR, "continuous");
    if (w <= 0 || h <= 0 || w > 65535 || h > 65535)
        RAISE(vm, "ValueError", "%s(): width/height must be in 1..65535", WHO_CTOR);

    cui_app_desc d;
    memset(&d, 0, sizeof d);
    d.title = title;
    d.width = (int)w;
    d.height = (int)h;
    d.hidden = hidden;
    d.continuous = cont;

    cui_app *app = cui_app_create(&d);
    if (!app)
        RAISE(vm, "RuntimeException", "%s(): cannot create a window", WHO_CTOR);
    app_set(o, app);
    return TV_NULL;
}

static TVal nm_app_frame(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_app *a = tui_need_app(vm, (TObj *)self, WHO_FRAME);
    return cui_app_frame(a) ? TV_TRUE : TV_FALSE;
}

static TVal nm_app_quit(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_app_quit(tui_need_app(vm, (TObj *)self, "Tphp\\Tui\\App::quit()"));
    return TV_NULL;
}

static TVal nm_app_set_title(VM *vm, void *self, TVal *args, int n) {
    const char *t = tui_arg_str(vm, args, n, 0, "Tphp\\Tui\\App::setTitle()");
    cui_app_set_title(tui_need_app(vm, (TObj *)self, "Tphp\\Tui\\App::setTitle()"), t);
    return TV_NULL;
}

static TVal nm_app_set_size(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::setSize()";
    int64_t w = tui_arg_int(vm, args, n, 0, who, "width");
    int64_t h = tui_arg_int(vm, args, n, 1, who, "height");
    if (w <= 0 || h <= 0 || w > 65535 || h > 65535)
        RAISE(vm, "ValueError", "%s(): width/height must be in 1..65535", who);
    cui_app_set_size(tui_need_app(vm, (TObj *)self, who), (int)w, (int)h);
    return TV_NULL;
}

static TVal nm_app_destroy(VM *vm, void *self, TVal *args, int n) {
    (void)vm;
    (void)args;
    (void)n;
    TObj *o = (TObj *)self;
    cui_app *a = tui_app_of(o);
    if (a) {
        cui_app_destroy(a);
        app_set(o, NULL);
    }
    return TV_NULL;
}

/* native_dtor：对象释放（含请求收尾）时兜底关窗；无 VM，不抛异常 */
static void app_native_dtor(TObj *o) {
    cui_app *a = tui_app_of(o);
    if (a) {
        cui_app_destroy(a);
        app_set(o, NULL);
    }
}

/* ---- 布局（立即模式 scope：begin/end 成对，与 C 侧 cui_column/cui_row 宏等价） ---- */

static TVal nm_column_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::columnBegin()";
    int64_t pad = n >= 1 ? tui_arg_int(vm, args, n, 0, who, "pad") : 0;
    int64_t gap = n >= 2 ? tui_arg_int(vm, args, n, 1, who, "gap") : 0;
    cui_box b;
    memset(&b, 0, sizeof b);
    b.pad = (float)pad;
    b.gap = (float)gap;
    /* CUI 示例根列同款 .width = CUI_FILL（列在父级内拉满） */
    b.width.kind = CUI_SIZE_GROW;
    b.width.value = 1.0f;
    cui_column_begin(tui_need_ui(vm, (TObj *)self, who), &b);
    return TV_NULL;
}

static TVal nm_row_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::rowBegin()";
    int64_t gap = n >= 1 ? tui_arg_int(vm, args, n, 0, who, "gap") : 0;
    cui_box b;
    memset(&b, 0, sizeof b);
    b.gap = (float)gap;
    cui_row_begin(tui_need_ui(vm, (TObj *)self, who), &b);
    return TV_NULL;
}

static TVal nm_box_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_box_end(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::boxEnd()"));
    return TV_NULL;
}

static TVal nm_spacer(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_spacer(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::spacer()"));
    return TV_NULL;
}

static TVal nm_separator(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_separator(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::separator()"));
    return TV_NULL;
}

/* ---- 文本 ---- */

static TVal nm_heading(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::heading()";
    const char *text = tui_arg_str(vm, args, n, 0, who);
    int64_t level = n >= 2 ? tui_arg_int(vm, args, n, 1, who, "level") : 2;
    cui_heading(tui_need_ui(vm, (TObj *)self, who), text, (int)level);   /* CUI 内部夹到 1..4 */
    return TV_NULL;
}

static TVal nm_secondary(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::secondary()";
    const char *text = tui_arg_str(vm, args, n, 0, who);
    cui_secondary(tui_need_ui(vm, (TObj *)self, who), text);
    return TV_NULL;
}

static TVal nm_text(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::text()";
    const char *text = tui_arg_str(vm, args, n, 0, who);
    cui_text_ex(tui_need_ui(vm, (TObj *)self, who), text, NULL);
    return TV_NULL;
}

static TVal nm_label(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::label()";
    const char *text = tui_arg_str(vm, args, n, 0, who);
    cui_label(tui_need_ui(vm, (TObj *)self, who), text);
    return TV_NULL;
}

/* ---- 控件 ---- */

static TVal nm_button(VM *vm, void *self, TVal *args, int n) {
    const char *label = tui_arg_str(vm, args, n, 0, WHO_BTN);
    int64_t variant = n >= 2 ? tui_arg_int(vm, args, n, 1, WHO_BTN, "variant") : 0;
    if (variant < 0 || variant > 6)   /* cui_button_variant 0..6 */
        RAISE(vm, "ValueError", "%s(): variant must be in 0..6", WHO_BTN);
    cui_button_opts o;
    memset(&o, 0, sizeof o);
    o.variant = (cui_button_variant)variant;
    return cui_button_ex(tui_need_ui(vm, (TObj *)self, WHO_BTN), label, &o)
        ? TV_TRUE : TV_FALSE;
}

static TVal nm_toggle(VM *vm, void *self, TVal *args, int n) {
    const char *label = tui_arg_str(vm, args, n, 0, WHO_TOG);
    bool v = tui_arg_bool(vm, args, n, 1, WHO_TOG, "value");
    /* cui_switch_ex 点击时已原地翻转 *value 并返回 true——只回传，勿再翻 */
    cui_switch_ex(tui_need_ui(vm, (TObj *)self, WHO_TOG), label, &v, NULL);
    return v ? TV_TRUE : TV_FALSE;
}

/* ---- 类构建 helpers（teb 同款模式） ---- */

static TClass *tui_cls_new(const char *fqcn) {
    TClass *cls = tm_malloc(sizeof *cls);
    memset(cls, 0, sizeof *cls);
    cls->name = tstr_intern(fqcn, strlen(fqcn));
    cls->kind = CK_CLASS;
    cls->flattened = true;
    cls->m_ctor = cls->m_dtor = cls->m_get = cls->m_set = cls->m_isset =
        cls->m_unset = cls->m_call = cls->m_callstatic = cls->m_tostring =
        cls->m_invoke = cls->m_clone = cls->m_debuginfo = cls->m_clonestate = -1;
    return cls;
}

/* tui_cls_add_native（方法追加）已迁至 tui_util.c，供各域文件共用 */

static void tui_cls_add_ctor(TClass *cls, void *fn) {
    cls->m_ctor = (int16_t)cls->n_methods;
    tui_cls_add_native(cls, "__construct", fn, false);
}

static void tui_cls_add_prop(TClass *cls, const char *name) {
    TProp *p = tm_realloc(cls->props,
                          sizeof(TProp) * (size_t)(cls->n_props + 1));
    TProp *np = &p[cls->n_props];
    memset(np, 0, sizeof *np);
    np->name = tstr_intern(name, strlen(name));
    np->defval = TV_NULL;
    np->visibility = 2;           /* CVIS_PRIVATE：仅 native 方法可读写 */
    np->slot = (uint16_t)cls->n_props;
    cls->props = p;
    cls->n_props++;
}

static void tui_register_app_class(void) {
    TClass *cls = tui_cls_new("Tphp\\Tui\\App");
    tui_cls_add_prop(cls, "app");
    tui_cls_add_ctor(cls, (void *)nm_app_construct);
    tui_cls_add_native(cls, "frame", (void *)nm_app_frame, false);
    tui_cls_add_native(cls, "quit", (void *)nm_app_quit, false);
    tui_cls_add_native(cls, "setTitle", (void *)nm_app_set_title, false);
    tui_cls_add_native(cls, "setSize", (void *)nm_app_set_size, false);
    tui_cls_add_native(cls, "destroy", (void *)nm_app_destroy, false);
    tui_cls_add_native(cls, "columnBegin", (void *)nm_column_begin, false);
    tui_cls_add_native(cls, "rowBegin", (void *)nm_row_begin, false);
    tui_cls_add_native(cls, "boxEnd", (void *)nm_box_end, false);
    tui_cls_add_native(cls, "spacer", (void *)nm_spacer, false);
    tui_cls_add_native(cls, "separator", (void *)nm_separator, false);
    tui_cls_add_native(cls, "heading", (void *)nm_heading, false);
    tui_cls_add_native(cls, "secondary", (void *)nm_secondary, false);
    tui_cls_add_native(cls, "text", (void *)nm_text, false);
    tui_cls_add_native(cls, "label", (void *)nm_label, false);
    tui_cls_add_native(cls, "button", (void *)nm_button, false);
    tui_cls_add_native(cls, "toggle", (void *)nm_toggle, false);
    cls->native_dtor = app_native_dtor;
    /* 各域占位注册（Task 1 拆分架构）：Task 2-7 的方法在对应域文件内追加 */
    tui_register_widgets(cls);
    tui_register_form(cls);
    tui_register_layout(cls);
    tui_register_charts(cls);
    tui_register_content(cls);
    tui_register_app_layer(cls);
    tphp_class_register_compile(cls);
}

/* ---- init：被 ext registry 调用，首脚本编译前一次 ----
 * 注册符号约定（PLAN/ext-packages.md §2.3）：tphp/<pkg> → tphp_ext_tphp_tui_init */

void tphp_ext_tphp_tui_init(void) {
    tphp_ext_reg_const_str("Tphp\\Tui\\VERSION", "0.1.0");
    /* cui_button_variant（cui.h）：PHP 侧 button($label, $variant) 用 */
    tphp_ext_reg_const_int("Tphp\\Tui\\BUTTON_DEFAULT", 0);
    tphp_ext_reg_const_int("Tphp\\Tui\\BUTTON_SECONDARY", 1);
    tphp_ext_reg_const_int("Tphp\\Tui\\BUTTON_PRIMARY", 2);
    tphp_ext_reg_const_int("Tphp\\Tui\\BUTTON_OUTLINE", 3);
    tphp_ext_reg_const_int("Tphp\\Tui\\BUTTON_GHOST", 4);
    tphp_ext_reg_const_int("Tphp\\Tui\\BUTTON_DANGER", 5);
    tphp_ext_reg_const_int("Tphp\\Tui\\BUTTON_LINK", 6);
    tui_register_app_class();
}
