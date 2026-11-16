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
#define WHO_BTN   "Tphp\\Tui\\Widgets::button()"
#define WHO_TOG   "Tphp\\Tui\\Widgets::toggle()"

/* ---- 域注册占位（各域 .c 定义；Task 2-7 的方法在对应域文件内追加） ---- */
extern void tui_register_widgets(TClass *cls);
extern void tui_register_form(TClass *cls);
extern void tui_register_layout(TClass *cls);
extern void tui_register_charts(TClass *cls);
extern void tui_register_content(TClass *cls);
extern void tui_register_app_layer(TClass *cls);

/* 阶段 B：5 个域类，全局单例模板（编译期注册一次；子对象经 tui_make_child 建）。
 * App 上仍挂全部方法（旧 API 零改动），域类挂同一批 impl（新 API）。 */
static TClass *g_layout_cls, *g_form_cls, *g_widgets_cls,
              *g_charts_cls, *g_content_cls;

/* 供 App 域访问器取类模板（B.5，定义在下方 domain accessor 处使用） */
TClass *tui_domain_cls(int which);

/* ---- App 句柄（props[0] = TV_INT 地址；
 *      tui_app_of / tui_need_app / tui_need_ui 已迁至 tui_util.h） ---- */

static void app_set(TObj *o, cui_app *a) {
    int s = tphp_class_find_prop(o->cls, "app");
    tval_release(&o->props[s]);
    o->props[s] = TV_INT((int64_t)(intptr_t)a);
}

/* ---- 阶段 B：域子对象访问 ----
 * 缓存槽 dom0..dom4 各持一个 T_OBJ 引用（rc 归槽）。访问器返回时另 addref
 * 给 VM（native 返回值所有权转移，vm.c 直接接管不再 addref）。 */
static const char *DOM_SLOTS[5] = { "dom0", "dom1", "dom2", "dom3", "dom4" };

static void domain_release_all(TObj *o) {
    for (int i = 0; i < 5; i++) {
        int s = tphp_class_find_prop(o->cls, DOM_SLOTS[i]);
        if (s >= 0) {
            tval_release(&o->props[s]);
            o->props[s] = TV_NULL;   /* 防 native_dtor 后引擎二次 release */
        }
    }
}

static TVal nm_domain_accessor(VM *vm, void *self, int which,
                               const char *who) {
    TObj *o = (TObj *)self;
    tui_need_app(vm, o, who);        /* 窗口已销毁则抛 Error */
    TClass *cls = tui_domain_cls(which);
    if (!cls) RAISE(vm, "Error", "%s: domain class not registered", who);
    int s = tphp_class_find_prop(o->cls, DOM_SLOTS[which]);
    if (s < 0) RAISE(vm, "Error", "%s: cache slot missing", who);
    TVal cached = o->props[s];
    TObj *child;
    if (cached.t == T_OBJ && cached.u.p) {
        child = (TObj *)cached.u.p;
    } else {
        child = tui_make_child(vm, o, cls);   /* rc=1 归槽 */
        o->props[s] = TV_OBJ(child);
    }
    tval_addref(TV_OBJ(child));              /* +1 归返回值，VM 接管 */
    return TV_OBJ(child);
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
        RAISE(vm, "ValueError", "%s: width/height must be in 1..65535", WHO_CTOR);

    cui_app_desc d;
    memset(&d, 0, sizeof d);
    d.title = title;
    d.width = (int)w;
    d.height = (int)h;
    d.hidden = hidden;
    d.continuous = cont;

    cui_app *app = cui_app_create(&d);
    if (!app)
        RAISE(vm, "RuntimeException", "%s: cannot create a window", WHO_CTOR);
    app_set(o, app);
    return TV_NULL;
}

static TVal nm_app_frame(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_app *a = tui_need_app(vm, (TObj *)self, WHO_FRAME);
    /* 阶段 A：上一段若有 SCOPE_IF begin 漏配对 End，node 栈已失衡，
     * 继续渲染会随机 AV（coverage.md §3）。渲染前检出并抛 Error，不静默。
     * 先清标记再抛：一次报告、catch 后可恢复，避免重复抛同一错污染后续帧。 */
    const char *unclosed = tui_scopeif_unclosed((TObj *)self);
    if (unclosed) {
        tui_scopeif_clear((TObj *)self);
        RAISE(vm, "Error", "%s: unclosed scope from previous frame: %s() without matching End()",
              WHO_FRAME, unclosed);
    }
    /* 阶段 B.4：上一段的 Layout 容器 begin/end 是否配平（depth 归零） */
    int ds = tphp_class_find_prop(((TObj *)self)->cls, "dom0");
    if (ds >= 0) {
        TVal dv = ((TObj *)self)->props[ds];
        if (dv.t == T_OBJ && dv.u.p) {
            TObj *layout = (TObj *)dv.u.p;
            int depth = tui_prop_int(layout, "depth");
            if (depth != 0) {
                /* 失配意味着 CUI node 栈已失衡，本 App 不可恢复——清 depth 仅为
                 * 避免 destroy 前的重复检测；调用方应销毁实例而非继续 frame()。 */
                tui_prop_set_int(layout, "depth", 0);
                RAISE(vm, "Error", "%s: %d unclosed Layout container(s) in previous frame "
                      "(window state corrupted; destroy and recreate)",
                      WHO_FRAME, depth);
            }
        }
    }
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
        RAISE(vm, "ValueError", "%s: width/height must be in 1..65535", who);
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
        tui_scopeif_clear(o);      /* 清 SCOPE_IF 残留，避免污染下一实例 */
        domain_release_all(o);    /* 阶段 B：释放 5 个域子对象引用 */
        cui_app_destroy(a);
        app_set(o, NULL);
    }
    return TV_NULL;
}

/* native_dtor：对象释放（含请求收尾）时兜底关窗；无 VM，不抛异常 */
static void app_native_dtor(TObj *o) {
    cui_app *a = tui_app_of(o);
    if (a) {
        tui_scopeif_clear(o);
        cui_app_destroy(a);
        app_set(o, NULL);
    }
    domain_release_all(o);   /* 即便窗口已 destroy，域槽仍可能持有引用 */
}

/* ---- 布局（立即模式 scope：begin/end 成对，与 C 侧 cui_column/cui_row 宏等价） ---- */

/* Layout 容器 depth 配平 helper 在 tui_util.c（tui_depth_inc/dec），各域共用。 */

static TVal nm_column_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Layout::columnBegin()";
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
    tui_depth_inc((TObj *)self);
    return TV_NULL;
}

static TVal nm_row_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Layout::rowBegin()";
    int64_t gap = n >= 1 ? tui_arg_int(vm, args, n, 0, who, "gap") : 0;
    cui_box b;
    memset(&b, 0, sizeof b);
    b.gap = (float)gap;
    cui_row_begin(tui_need_ui(vm, (TObj *)self, who), &b);
    tui_depth_inc((TObj *)self);
    return TV_NULL;
}

static TVal nm_box_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\Layout::boxEnd()";
    tui_depth_dec(vm, (TObj *)self, who);
    cui_box_end(tui_need_ui(vm, (TObj *)self, who));
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
    int64_t variant = tui_opt_enum(vm, args, n, 1, 0, WHO_BTN, "variant",
                                   TUI_LC_BUTTON_VARIANT);
    if (variant < 0 || variant > 6)   /* cui_button_variant 0..6 */
        RAISE(vm, "ValueError", "%s: variant must be in 0..6", WHO_BTN);
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

/* ---- 阶段 B：App 域访问器 ---- */

static TVal nm_app_layout(VM *vm, void *self, TVal *args, int n) {
    /* layout(LayoutType): 校验实参是 LayoutType 枚举（或 int 兼容），返回 Layout 域对象。
     * 参数当前仅做契约校验（提示调用者 intended 容器类型），begin() 时再实际使用。 */
    const char *who = "Tphp\\Tui\\App::layout()";
    if (n >= 1)
        tui_opt_enum(vm, args, n, 0, 0, who, "type", TUI_LC_LAYOUT_TYPE);
    return nm_domain_accessor(vm, self, 0, who);
}
static TVal nm_app_form(VM *vm, void *self, TVal *args, int n) {
    (void)args; (void)n;
    return nm_domain_accessor(vm, self, 1, "Tphp\\Tui\\App::form()");
}
static TVal nm_app_widgets(VM *vm, void *self, TVal *args, int n) {
    (void)args; (void)n;
    return nm_domain_accessor(vm, self, 2, "Tphp\\Tui\\App::widgets()");
}
static TVal nm_app_charts(VM *vm, void *self, TVal *args, int n) {
    (void)args; (void)n;
    return nm_domain_accessor(vm, self, 3, "Tphp\\Tui\\App::charts()");
}
static TVal nm_app_content(VM *vm, void *self, TVal *args, int n) {
    (void)args; (void)n;
    return nm_domain_accessor(vm, self, 4, "Tphp\\Tui\\App::content()");
}

/* ---- 类构建 helpers 已迁至 tui_util.c（供各域共用） ---- */

/* ---- 枚举类（backed int）----
 * 运行时的枚举实例就是 TObj，语义全部由 cls->kind==CK_ENUM + cls->cases[]
 * 驱动（vm.c 的 ::cases/::from/::tryFrom、C::Case、->name/->value 分支）。
 * 单例由 class.c 的 ClsRt.case_objs 惰性创建并 addref，这里只填静态模板。
 * 两个枚举共用本函数，避免重复机械。 */
typedef struct { const char *name; int64_t val; } TuiEnumCase;

static TClass *tui_register_enum(const char *fqcn,
                                 const TuiEnumCase *cases, int n) {
    TClass *cls = tui_cls_new(fqcn);
    cls->kind = CK_ENUM;
    cls->backed = true;
    cls->backing_type = T_INT;
    cls->cases = tm_malloc(sizeof(TEnumCase) * (size_t)n);
    memset(cls->cases, 0, sizeof(TEnumCase) * (size_t)n);
    for (int i = 0; i < n; i++) {
        cls->cases[i].name = tstr_intern(cases[i].name, strlen(cases[i].name));
        cls->cases[i].backing = TV_INT(cases[i].val);
        cls->cases[i].obj = NULL;          /* 单例惰性建（ClsRt 侧表） */
    }
    cls->n_cases = n;
    tphp_class_register_compile(cls);
    return cls;
}

/* cui_button_variant（cui.h）：button($label, $variant) 的取值 */
static void tui_register_button_variant(void) {
    static const TuiEnumCase cases[] = {
        { "Default",   0 },
        { "Secondary", 1 },
        { "Primary",   2 },
        { "Outline",   3 },
        { "Ghost",     4 },
        { "Danger",    5 },
        { "Link",      6 },
    };
    tui_register_enum("Tphp\\Tui\\ButtonVariant", cases,
                      (int)(sizeof cases / sizeof cases[0]));
}

/* cui_font role（cui.h CUI_FONT_*）：fontSetRole/fontAddFallback 的 role 取值 */
static void tui_register_font_role(void) {
    static const TuiEnumCase cases[] = {
        { "Body",    0 },   /* Inter Regular */
        { "Medium",  1 },   /* Inter Medium  */
        { "Display", 2 },   /* Geist Medium  */
        { "Mono",    3 },   /* Geist Mono    */
        { "Icons",   4 },   /* Lucide        */
    };
    tui_register_enum("Tphp\\Tui\\FontRole", cases,
                      (int)(sizeof cases / sizeof cases[0]));
}

/* LayoutType（阶段 B）：Layout::begin() 的容器类型，backed int 0..4 */
static void tui_register_layout_type(void) {
    static const TuiEnumCase cases[] = {
        { "Column", 0 },
        { "Row",    1 },
        { "Box",    2 },
        { "Scroll", 3 },
        { "Split",  4 },
    };
    tui_register_enum("Tphp\\Tui\\LayoutType", cases,
                      (int)(sizeof cases / sizeof cases[0]));
}

static void tui_register_app_class(void) {
    TClass *cls = tui_cls_new("Tphp\\Tui\\App");
    tui_cls_add_prop(cls, "app");
    /* 阶段 B：5 个域子对象缓存槽（T_OBJ 引用，首次访问创建） */
    tui_cls_add_prop(cls, "dom0");   /* Layout  */
    tui_cls_add_prop(cls, "dom1");   /* Form    */
    tui_cls_add_prop(cls, "dom2");   /* Widgets */
    tui_cls_add_prop(cls, "dom3");   /* Charts  */
    tui_cls_add_prop(cls, "dom4");   /* Content */
    tui_cls_add_ctor(cls, (void *)nm_app_construct);
    tui_cls_add_native(cls, "frame", (void *)nm_app_frame, false);
    tui_cls_add_native(cls, "quit", (void *)nm_app_quit, false);
    tui_cls_add_native(cls, "setTitle", (void *)nm_app_set_title, false);
    tui_cls_add_native(cls, "setSize", (void *)nm_app_set_size, false);
    tui_cls_add_native(cls, "destroy", (void *)nm_app_destroy, false);
    tui_cls_add_native(cls, "spacer", (void *)nm_spacer, false);
    tui_cls_add_native(cls, "separator", (void *)nm_separator, false);
    tui_cls_add_native(cls, "heading", (void *)nm_heading, false);
    tui_cls_add_native(cls, "secondary", (void *)nm_secondary, false);
    tui_cls_add_native(cls, "text", (void *)nm_text, false);
    tui_cls_add_native(cls, "label", (void *)nm_label, false);
    /* 阶段 B：域访问器 */
    tui_cls_add_native(cls, "layout",  (void *)nm_app_layout,  false);
    tui_cls_add_native(cls, "form",    (void *)nm_app_form,    false);
    tui_cls_add_native(cls, "widgets", (void *)nm_app_widgets, false);
    tui_cls_add_native(cls, "charts",  (void *)nm_app_charts,  false);
    tui_cls_add_native(cls, "content", (void *)nm_app_content, false);
    cls->native_dtor = app_native_dtor;
    /* 仅 App 层（主题/字体/截图/测试面）留 App；其余域方法只在域类上，
     * 不再挂 App —— 真正精简 App（B.8）。 */
    tui_register_app_layer(cls);
    tphp_class_register_compile(cls);
}

/* tui.c 里的 5 个"桥接"方法（容器 3 + 控件 2）：impl 在 tui.c，
 * 但按语义归属域类。导出注册函数供域类构建时挂到对应 cls。 */
void tui_register_core_bridge_layout(TClass *cls) {
    tui_cls_add_native(cls, "columnBegin", (void *)nm_column_begin, false);
    tui_cls_add_native(cls, "rowBegin",    (void *)nm_row_begin, false);
    tui_cls_add_native(cls, "boxEnd",      (void *)nm_box_end, false);
}
void tui_register_core_bridge_widgets(TClass *cls) {
    tui_cls_add_native(cls, "button", (void *)nm_button, false);
    tui_cls_add_native(cls, "toggle", (void *)nm_toggle, false);
}

/* ---- 域类构建（阶段 B）----
 * 每个域类：CK_CLASS + 私有 app 槽 + 该域的 native 方法 + 编译注册。
 * 同一批 tui_register_* 也被 App 调用过（挂到 App），此处再挂到域类，
 * impl 完全共用——它们只经 self.props["app"] 取窗口，子对象带同样槽即可。 */
typedef void (*DomainRegFn)(TClass *);

extern void tui_register_core_bridge_layout(TClass *cls);
extern void tui_register_core_bridge_widgets(TClass *cls);
extern void tui_layout_state_free(void *p);
extern void tui_content_state_free(void *p);

/* native_dtor 签名是 void(TObj*)，经 tui_state_free 适配到 void(void*) 的 state_free */
static void layout_dtor(TObj *o) { tui_state_free(o, "state", tui_layout_state_free); }
static void content_dtor(TObj *o) { tui_state_free(o, "state", tui_content_state_free); }

static TClass *tui_build_domain_class(const char *fqcn, DomainRegFn reg,
                                      DomainRegFn bridge, bool with_depth,
                                      void (*dtor)(TObj *)) {
    TClass *cls = tui_cls_new(fqcn);
    tui_cls_add_prop(cls, "app");
    if (with_depth) tui_cls_add_prop(cls, "depth");   /* Layout 容器配平计数 */
    if (dtor) {
        tui_cls_add_prop(cls, "state");              /* per-App 状态 struct 指针 */
        cls->native_dtor = dtor;
    }
    reg(cls);
    if (bridge) bridge(cls);   /* tui.c 里按语义归属本域的桥接方法 */
    tphp_class_register_compile(cls);
    return cls;
}

static void tui_register_domain_classes(void) {
    g_layout_cls  = tui_build_domain_class("Tphp\\Tui\\Layout",  tui_register_layout,  tui_register_core_bridge_layout, true,  layout_dtor);
    g_form_cls    = tui_build_domain_class("Tphp\\Tui\\Form",    tui_register_form,    NULL, false, NULL);
    g_widgets_cls = tui_build_domain_class("Tphp\\Tui\\Widgets", tui_register_widgets, tui_register_core_bridge_widgets, false, NULL);
    g_charts_cls  = tui_build_domain_class("Tphp\\Tui\\Charts",  tui_register_charts,  NULL, false, NULL);
    g_content_cls = tui_build_domain_class("Tphp\\Tui\\Content", tui_register_content, NULL, false, content_dtor);
}

/* 域访问器用的类模板查表；which: 0=Layout 1=Form 2=Widgets 3=Charts 4=Content */
TClass *tui_domain_cls(int which) {
    switch (which) {
        case 0: return g_layout_cls;
        case 1: return g_form_cls;
        case 2: return g_widgets_cls;
        case 3: return g_charts_cls;
        case 4: return g_content_cls;
        default: return NULL;
    }
}

/* ---- init：被 ext registry 调用，首脚本编译前一次 ----
 * 注册符号约定（PLAN/ext-packages.md §2.3）：tphp/<pkg> → tphp_ext_tphp_tui_init */

void tphp_ext_tphp_tui_init(void) {
    tphp_ext_reg_const_str("Tphp\\Tui\\VERSION", "0.1.0");
    /* 枚举必须先于 App 注册（button/fontSetRole/fontAddFallback 的实参校验） */
    tui_register_button_variant();
    tui_register_font_role();
    tui_register_layout_type();
    tui_register_app_class();
    tui_register_domain_classes();   /* 阶段 B：5 个域类（访问器返回的子对象模板） */
}
