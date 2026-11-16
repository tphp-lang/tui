/*
 * tphp/tui - src/tui_util.h - 跨文件共享助手（Task 1 参数助手层）
 *
 * 所有导出符号带 tui_ 前缀（非 static），避免与其它 ext 包撞名；
 * 各域 .c（tui.c / tui_widgets.c / ...）通过 #include "tui_util.h" 取用。
 *
 * 约定：
 *   - 参数类型不符一律 tphp_vm_raise 抛异常（noreturn），不静默降级；
 *   - 数组助手返回 tm_malloc 的 C 数组，调用方负责释放：
 *     字符串数组 tui_strs_free，其余 tui_arr_free；
 *   - tui_strs 每项是独立拷贝——tstr_val 返回 TStr 内部缓冲，
 *     不可跨帧持有，必须拷贝后再用。
 */
#ifndef TUI_UTIL_H
#define TUI_UTIL_H

#include "class.h"        /* TClass / tphp_class_find_prop / tphp_val_type_name */
#include "value.h"        /* TVal / TV_* / TStr / TObj / tm_malloc / tstr_val */
#include "opcode.h"       /* VM / tphp_vm_raise / NativeMethodFn */

#include <cui/cui_app.h>  /* cui_app / cui_ctx（其内含 cui.h 全部组件入口） */

#include <stdbool.h>
#include <stdint.h>

/* ---- 参数读取（必需参数：缺失抛 ArgumentCountError，类型不符抛 TypeError） ----
 * who   = 完整方法名（错误消息前缀，如 "Tphp\\Tui\\App::button()"）
 * pname = 参数名（如 "variant"，用于类型错误消息） */
const char *tui_arg_str(VM *vm, TVal *args, int n, int i, const char *who);
int64_t tui_arg_int(VM *vm, TVal *args, int n, int i, const char *who,
                    const char *pname);
bool tui_arg_bool(VM *vm, TVal *args, int n, int i, const char *who,
                  const char *pname);
/* 必需 float：int 安全升格；其余类型抛 TypeError */
double tui_arg_float(VM *vm, TVal *args, int n, int i, const char *who,
                     const char *pname);

/* ---- tui 枚举类的小写 FQCN（各域文件共享；tui_arg_enum 按名查表） ---- */
#define TUI_LC_BUTTON_VARIANT "tphp\\tui\\buttonvariant"
#define TUI_LC_FONT_ROLE      "tphp\\tui\\fontrole"
#define TUI_LC_LAYOUT_TYPE    "tphp\\tui\\layouttype"

/* ---- 枚举 case 读取（Tphp\Tui\* 枚举类；兼容历史 int 传参） ----
 * enum_lc = 枚举类 FQCN 小写（如 "tphp\\tui\\buttonvariant"）。按名查表而
 * 非缓存 TClass*：worker 线程的类表是克隆的，静态指针会指到 master 副本。
 * 接受：T_OBJ 且属于该枚举的 case 单例 → 取 backing int；
 *       或 T_INT（旧 API 兼容，平滑迁移）；
 * 其余抛 TypeError，错误消息带枚举名（"must be of type Tphp\Tui\ButtonVariant"）。
 * unit enum（无 backing）抛 TypeError——tui 的枚举都是 backed int。 */
int64_t tui_arg_enum(VM *vm, TVal *args, int n, int i, const char *who,
                     const char *pname, const char *enum_lc);

/* 同上，可选参：缺省返回 def。 */
int64_t tui_opt_enum(VM *vm, TVal *args, int n, int i, int64_t def,
                     const char *who, const char *pname, const char *enum_lc);

/* ---- 可选参数：缺省（n <= i）返回给定默认值；给了但类型不对抛 TypeError ---- */
int64_t tui_opt_int(VM *vm, TVal *args, int n, int i, int64_t def,
                    const char *who, const char *pname);
bool tui_opt_bool(VM *vm, TVal *args, int n, int i, bool def,
                  const char *who, const char *pname);
const char *tui_opt_str(VM *vm, TVal *args, int n, int i, const char *def,
                        const char *who, const char *pname);
/* float 可选参：int 安全升格为 float；其余类型抛 TypeError */
double tui_opt_float(VM *vm, TVal *args, int n, int i, double def,
                     const char *who, const char *pname);

/* ---- App 句柄（props["app"] = TV_INT 存窗口地址；destroy 后再用抛 Error） ---- */
cui_app *tui_app_of(TObj *o);                    /* 取句柄；已销毁返回 NULL */
cui_app *tui_need_app(VM *vm, TObj *o, const char *who);  /* NULL 则抛 Error */
cui_ctx *tui_need_ui(VM *vm, TObj *o, const char *who);   /* 同上，取绘制上下文 */

/* ---- SCOPE_IF 失配检测（阶段 A；per-App 化后接收域对象）----
 * 各域从自己的 state 读未闭合标记，返回 PHP begin 名，全闭合返回 NULL。
 * tui_scopeif_unclosed(app) 从 App 的 dom0/dom4 槽取 Layout/Content 子对象再汇总；
 * frame() 入口调用，有残留抛 Error。若域对象尚未创建则视为无残留。 */
const char *tui_layout_scopeif_unclosed(TObj *layout);
const char *tui_content_scopeif_unclosed(TObj *content);
void tui_layout_scopeif_clear(TObj *layout);
void tui_content_scopeif_clear(TObj *content);
const char *tui_scopeif_unclosed(TObj *app);
void tui_scopeif_clear(TObj *app);   /* destroy 时清残留 */

/* ---- 类构建 / 注册（teb 同款模式；各域共用） ---- */
TClass *tui_cls_new(const char *fqcn);            /* CK_CLASS，全 -1 魔法方法槽 */
void tui_cls_add_ctor(TClass *cls, void *fn);
void tui_cls_add_prop(TClass *cls, const char *name);  /* private int/null 槽 */
void tui_cls_add_native(TClass *cls, const char *name, void *fn, bool is_static);

/* ---- 子对象（阶段 B）----
 * 建一个域子对象：tphp_obj_alloc 新建，把父的 props["app"] 复制到子同名槽。
 * 引用计数（§7 纪律）：alloc 的 rc=1 转移给返回值；复制的 app 值是 TV_INT
 * 标量无需 addref。子对象由父的私有槽持有（调用方另 addref），destroy 时 release。 */
TObj *tui_make_child(VM *vm, TObj *parent, TClass *cls);

/* 按名读写 int 私有槽（不存在返回 -1 时写操作 no-op、读返回 0）。
 * 用于 Layout.depth 配平计数——不硬编码槽号。 */
int  tui_prop_int(TObj *o, const char *name);
void tui_prop_set_int(TObj *o, const char *name, int v);

/* Layout 容器 depth 配平：begin 自增、end 自减并校验（阶段 B.4） */
void tui_depth_inc(TObj *o);
void tui_depth_dec(VM *vm, TObj *o, const char *who);

/* ---- 数组转换：PHP array → C 数组（tm_malloc 分配，插入序） ----
 * 空数组合法：*n = 0 且返回 NULL；v 不是数组抛 TypeError。
 * 元素类型不符抛 TypeError 并指明参数名与元素位置（1 起）：
 *   tui_strs   仅 string（每项拷贝，可跨帧持有）
 *   tui_floats 允许 int/float，其余抛
 *   tui_ints   仅 int（float 不做静默截断）
 *   tui_bools  仅 bool
 * 释放：tui_strs_free(a, n) / tui_arr_free(p)（float/int/bool 同用）。 */
char **tui_strs(VM *vm, TVal *v, const char *who, const char *pname, int *n);
double *tui_floats(VM *vm, TVal *v, const char *who, const char *pname, int *n);
int64_t *tui_ints(VM *vm, TVal *v, const char *who, const char *pname, int *n);
bool *tui_bools(VM *vm, TVal *v, const char *who, const char *pname, int *n);
void tui_strs_free(char **a, int n);   /* 逐项 + 数组本身；a 可为 NULL */
void tui_arr_free(void *p);            /* float/int/bool 数组统一释放；p 可为 NULL */

/* ---- per-App 域状态（替代文件级静态，消除同线程多窗口交错串扰）----
 * 每个域对象（Layout/Content，由 App 缓存）持一个 state 私有槽（TV_INT 存指针），
 * 首次使用时懒分配，域对象析构（native_dtor）时统一释放其内部数组与 struct。
 * 这样两个 App 交错画同类型容器时状态互不相干。 */
typedef struct {
    /* SCOPE_IF begin 标记（13 对） */
    bool popover_on, menu_on, ctxmenu_on, dialog_on, sheet_on,
         accordion_on, table_on, tree_on, hover_on, expand_on,
         usermenu_on, dockpanel_on, datagrid_on;
    float split_ratio;
    /* begin→end 借阅的临时数组（begin 分配 / end 回收） */
    int *reorder; int reorder_n;
    cui_swipe_action *swipe_l; int swipe_ln;
    cui_swipe_action *swipe_r; int swipe_rn;
    char **swipe_strs; int swipe_nstr;
    cui_table_column *tcols; int tcoln; char **tcol_strs; int tcol_nstr;
    cui_grid_column *gcols; int gcoln; char **gcol_strs; int gcol_nstr;
    cui_tree_node *tnodes; int tnoden;
} TuiLayoutState;

typedef struct {
    cui_terminal_buffer term_t; char *term_text; cui_terminal_line *term_lines; bool term_ready;
    cui_log_buffer log_t;       char *log_text;       cui_log_line *log_lines;       bool log_ready;
    bool nodeed_on;
} TuiContentState;

/* 取/懒分配域对象的 state 槽；slot 名由各域传入（如 "state"）。
 * 返回 void*，调用方 cast 为对应 struct*；无槽返回 NULL。 */
void *tui_state_get(TObj *o, const char *slot, size_t struct_size);
/* 域对象析构时调用：free 由 fn 指定的内部资源（fn 为 NULL 仅 free struct）。
 * struct 指针从 slot 读取，释放后置 NULL 防二次释放。 */
void tui_state_free(TObj *o, const char *slot, void (*fn)(void *state));

#endif /* TUI_UTIL_H */
