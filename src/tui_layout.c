/*
 * tphp/tui - src/tui_layout.c - Task 4 容器与导航域
 *
 * 跨 begin/end 被 CUI 借阅的数据（table/dataGrid/treeTable 的 columns、
 * reorder 的 order、swipe 的 actions、split 的 ratio）在本文件静态暂存：
 * 下一次 begin 或对应 end 时释放/覆盖——CUI 同类 scope 每帧至多一层，
 * 异常中断路径由下一次 begin 兜底回收（记档于 doc/coverage.md）。
 * 单次调用内消费的组件（toc/speedDial/filterBar 等）用完即释放。
 *
 * 返回值约定：点击型 bool、索引/页码 int、open 类传入当前值返回新值、
 * 结构化结果（pathBar/filterBar/treeTableEnd/kanbanEnd）返回关联数组。
 */
#include "tui_util.h"

#include <stdio.h>
#include <string.h>

#define RAISE(vm, cls, ...) tphp_vm_raise(vm, cls, __VA_ARGS__)

/* ---- 局部小工具 ---- */

static TVal *row_get(TArr *row, const char *k) {
    TVal key = TV_STR(tstr_new(k, strlen(k)));
    TVal *p = tarr_get(row, key);
    tval_release(&key);
    return p;
}

/* 堆拷贝（跨帧/跨调用持有的字符串一律拷贝，不借 TStr 内部缓冲） */
static char *dup_s(const char *s) {
    size_t n = strlen(s);
    char *p = tm_malloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

static TArr *rows_of(VM *vm, TVal *v, const char *who, const char *pname) {
    if (v->t != T_ARR)
        RAISE(vm, "TypeError", "%s(): argument %s must be of type array, %s given",
              who, pname, tphp_val_type_name(*v));
    return (TArr *)v->u.p;
}

/* 校验行是 array（位置 1 起） */
static TArr *row_arr(VM *vm, TVal e, const char *who, const char *pname, int pos) {
    if (e.t != T_ARR)
        RAISE(vm, "TypeError",
              "%s(): array element %d of argument %s must be of type array, %s given",
              who, pos, pname, tphp_val_type_name(e));
    return (TArr *)e.u.p;
}

/* 行内字符串键（必填） */
static const char *row_req_str(VM *vm, TArr *row, const char *k,
                               const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t != T_STR)
        RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be a string",
              who, pname, pos - 1, k);
    return tstr_val((TStr *)v->u.p);
}

/* 行内可选键 */
static const char *row_opt_str(VM *vm, TArr *row, const char *k, const char *who,
                               const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return NULL;
    if (v->t != T_STR)
        RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be a string",
              who, pname, pos - 1, k);
    return tstr_val((TStr *)v->u.p);
}

static int64_t row_opt_int(VM *vm, TArr *row, const char *k, int64_t def,
                           const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return def;
    if (v->t != T_INT)
        RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be an int",
              who, pname, pos - 1, k);
    return v->u.i;
}

static bool row_opt_bool(VM *vm, TArr *row, const char *k, bool def,
                         const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return def;
    if (v->t != T_TRUE && v->t != T_FALSE)
        RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be a bool",
              who, pname, pos - 1, k);
    return v->t == T_TRUE;
}

static double row_opt_float(VM *vm, TArr *row, const char *k, double def,
                            const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v) return def;
    if (v->t == T_FLOAT) return v->u.d;
    if (v->t == T_INT) return (double)v->u.i;
    RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be numeric",
          who, pname, pos - 1, k);
}

/* open 类参数：bool in/out */
static bool open_in(VM *vm, TVal *args, int n, int i, const char *who,
                    const char *pname) {
    return tui_arg_bool(vm, args, n, i, who, pname);
}

static TVal open_out(bool v) { return v ? TV_TRUE : TV_FALSE; }

/* ---- per-App 域状态（替代文件级静态，消除同线程多窗口交错串扰）----
 * 所有 begin→end 借阅缓冲与 SCOPE_IF 标记存于 Layout 子对象的 state 槽，
 * 每个 App 的 Layout 子对象独立 → 状态天然 per-App。
 * 注意：下列方法均有 self 参数（域对象），宏 GSTATE 从 self 取 state。 */
#define GSTATE ((TuiLayoutState *)tui_state_get((TObj *)self, "state", sizeof(TuiLayoutState)))

/* SCOPE_IF 失配检测（阶段 A）：返回本域第一个未闭合的 PHP begin 名。
 * 接收 Layout 域对象（per-App）；顺序与 coverage.md §3 的 14 对一致。 */
const char *tui_layout_scopeif_unclosed(TObj *layout) {
    TuiLayoutState *S = (TuiLayoutState *)tui_state_get(layout, "state", sizeof(TuiLayoutState));
    if (!S) return NULL;
    if (S->popover_on)   return "popoverBegin/popoverEnd";
    if (S->menu_on)      return "menuBegin/menuEnd";
    if (S->ctxmenu_on)   return "contextMenuBegin/contextMenuEnd";
    if (S->dialog_on)    return "dialogBegin/dialogEnd";
    if (S->sheet_on)     return "sheetBegin/sheetEnd";
    if (S->hover_on)     return "hoverCardBegin/hoverCardEnd";
    if (S->usermenu_on)  return "userMenuBegin/userMenuEnd";
    if (S->accordion_on) return "accordionBegin/accordionEnd";
    if (S->tree_on)      return "treeBegin/treeEnd";
    if (S->table_on)     return "tableBegin/tableEnd";
    if (S->datagrid_on)  return "dataGridBegin/dataGridEnd";
    if (S->dockpanel_on) return "dockPanelBegin/dockPanelEnd";
    if (S->expand_on)    return "expandableCardBegin/expandableCardEnd";
    return NULL;
}

void tui_layout_scopeif_clear(TObj *layout) {
    TuiLayoutState *S = (TuiLayoutState *)tui_state_get(layout, "state", sizeof(TuiLayoutState));
    if (!S) return;
    S->popover_on = S->menu_on = S->ctxmenu_on = S->dialog_on = S->sheet_on = false;
    S->accordion_on = S->table_on = S->tree_on = S->hover_on = S->expand_on = false;
    S->usermenu_on = S->dockpanel_on = S->datagrid_on = false;
}

static void free_strs(char **a, int n) {
    for (int i = 0; i < n; i++) tm_free(a[i]);
    tm_free(a);
}

/* Layout state 析构：释放尚存的借阅数组（正常配对下 End 已回收，
 * 此处兜底异常中断或漏 End 的场景，防泄漏）。 */
void tui_layout_state_free(void *p) {
    TuiLayoutState *S = (TuiLayoutState *)p;
    if (!S) return;
    tm_free(S->reorder);
    tm_free(S->swipe_l); tm_free(S->swipe_r);
    free_strs(S->swipe_strs, S->swipe_nstr);
    tm_free(S->tcols); free_strs(S->tcol_strs, S->tcol_nstr);
    tm_free(S->gcols); free_strs(S->gcol_strs, S->gcol_nstr);
    tm_free(S->tnodes);
}

/* ---- 容器 ---- */

static TVal nm_box_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::boxBegin()";
    int64_t pad = n >= 1 ? tui_opt_int(vm, args, n, 0, 0, who, "pad") : 0;
    int64_t gap = n >= 2 ? tui_opt_int(vm, args, n, 1, 0, who, "gap") : 0;
    cui_box b;
    memset(&b, 0, sizeof b);
    b.pad = (float)pad;
    b.gap = (float)gap;
    b.width.kind = CUI_SIZE_GROW;
    b.width.value = 1.0f;
    cui_box_begin(tui_need_ui(vm, (TObj *)self, who), &b);
    tui_depth_inc((TObj *)self);
    return TV_NULL;
}

static TVal nm_scroll_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::scrollBegin()";
    const char *id = tui_opt_str(vm, args, n, 0, "", who, "id");
    bool horiz = tui_opt_bool(vm, args, n, 1, false, who, "horizontal");
    cui_scroll_opts o;
    memset(&o, 0, sizeof o);
    o.id = id[0] ? id : NULL;
    o.horizontal = horiz;
    cui_scroll_begin(tui_need_ui(vm, (TObj *)self, who),
                     (id[0] || horiz) ? &o : NULL);
    tui_depth_inc((TObj *)self);
    return TV_NULL;
}

static TVal nm_scroll_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\Layout::scrollEnd()";
    tui_depth_dec(vm, (TObj *)self, who);
    cui_scroll_end(tui_need_ui(vm, (TObj *)self, who));
    return TV_NULL;
}

static TVal nm_scroll_to(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::scrollTo()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    double y = tui_arg_float(vm, args, n, 1, who, "y");
    cui_scroll_to(tui_need_ui(vm, (TObj *)self, who), id, (float)y);
    return TV_NULL;
}

/* split：ratio 借阅到 splitEnd —— 静态暂存（每帧一层，同类 scope 至多一个） */
static TVal nm_split_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::splitBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    double ratio = tui_arg_float(vm, args, n, 1, who, "ratio");
    GSTATE->split_ratio = (float)ratio;
    cui_split_begin(tui_need_ui(vm, (TObj *)self, who), id, &GSTATE->split_ratio, NULL);
    tui_depth_inc((TObj *)self);
    return TV_NULL;
}

static TVal nm_split_next(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_split_next(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::splitNext()"));
    return TV_NULL;
}

static TVal nm_split_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    tui_depth_dec(vm, (TObj *)self, "Tphp\\Tui\\Layout::splitEnd()");
    cui_split_end(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\Layout::splitEnd()"));
    return TV_FLOAT(GSTATE->split_ratio);   /* 拖拽后的新比例（PHP 持状态） */
}

/* ---- 导航 ---- */

static TVal nm_tabs(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::tabs()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int ic = 0;
    char **items = tui_strs(vm, &args[1], who, "items", &ic);
    int idx = (int)tui_arg_int(vm, args, n, 2, who, "index");
    cui_tabs_ex(tui_need_ui(vm, (TObj *)self, who), id,
                (const char *const *)items, ic, &idx, NULL);
    tui_strs_free(items, ic);
    return TV_INT(idx);
}

static TVal nm_nav_item(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::navItem()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    bool clicked = cui_nav_item_ex(tui_need_ui(vm, (TObj *)self, who), label, NULL);
    return open_out(clicked);
}

static TVal nm_nav_section(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::navSection()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    cui_nav_section(tui_need_ui(vm, (TObj *)self, who), title);
    return TV_NULL;
}

static TVal nm_breadcrumb(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::breadcrumb()";
    int ic = 0;
    char **items = tui_strs(vm, &args[0], who, "items", &ic);
    int picked = cui_breadcrumb(tui_need_ui(vm, (TObj *)self, who),
                                (const char *const *)items, ic);
    tui_strs_free(items, ic);
    return TV_INT(picked);   /* 点击索引，-1 = 未点 */
}

static TVal nm_pagination(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::pagination()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int page = (int)tui_arg_int(vm, args, n, 1, who, "page");
    int64_t cnt = tui_arg_int(vm, args, n, 2, who, "pageCount");
    if (cnt < 1)
        RAISE(vm, "ValueError", "%s(): pageCount must be >= 1", who);
    cui_pagination(tui_need_ui(vm, (TObj *)self, who), id, &page, (int)cnt);
    return TV_INT(page);   /* 0 起新页码 */
}

static TVal nm_stepper(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::stepper()";
    int sc = 0;
    char **steps = tui_strs(vm, &args[0], who, "steps", &sc);
    int64_t cur = tui_arg_int(vm, args, n, 1, who, "current");
    cui_stepper(tui_need_ui(vm, (TObj *)self, who),
                (const char *const *)steps, sc, (int)cur);
    tui_strs_free(steps, sc);
    return TV_NULL;
}

/* pathBar：返回 ['clicked'=>int,'submitted'=>string|null,'editing'=>bool] */
static TVal nm_path_bar(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::pathBar()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int sc = 0;
    char **segs = tui_strs(vm, &args[1], who, "segments", &sc);
    cui_path_bar_result r = cui_path_bar_ex(tui_need_ui(vm, (TObj *)self, who),
                                            id, (const char *const *)segs, sc, NULL);
    tui_strs_free(segs, sc);
    TArr *a = tarr_new();
    tarr_set(a, TV_STR(tstr_new("clicked", 7)), TV_INT(r.clicked));
    tarr_set(a, TV_STR(tstr_new("submitted", 9)),
             r.submitted ? TV_STR(tstr_new(r.submitted, strlen(r.submitted)))
                         : TV_NULL);
    tarr_set(a, TV_STR(tstr_new("editing", 7)), r.editing ? TV_TRUE : TV_FALSE);
    return TV_ARR(a);
}

/* pathBarMenu：返回段索引（-1 = 未打开） */
static TVal nm_path_bar_menu(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::pathBarMenu()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int seg = -1;
    cui_path_bar_menu_begin(tui_need_ui(vm, (TObj *)self, who), id, &seg);
    return TV_INT(seg);
}

static TVal nm_path_bar_menu_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_path_bar_menu_end(tui_need_ui(vm, (TObj *)self,
                                      "Tphp\\Tui\\App::pathBarMenuEnd()"));
    return TV_NULL;
}

/* toc：items=[['title'=>,'anchor'=>,'level'=>0..4]] 单次调用内消费 */
static TVal nm_toc(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::toc()";
    const char *scroll_id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "items");
    int cnt = 0;
    for (uint32_t i = 0; i < rows->dused; i++)
        if (rows->data[i].t != T_UNDEF) cnt++;
    cui_toc_item *items = cnt ? tm_malloc(sizeof(cui_toc_item) * (size_t)cnt) : NULL;
    char **strs = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt * 2) : NULL;
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "items", k + 1);
        strs[k * 2] = dup_s(row_req_str(vm, row, "title", who, "items", k + 1));
        strs[k * 2 + 1] = dup_s(row_opt_str(vm, row, "anchor", who, "items", k + 1)
                                ? row_opt_str(vm, row, "anchor", who, "items", k + 1)
                                : "");
        items[k].title = strs[k * 2];
        items[k].anchor = strs[k * 2 + 1][0] ? strs[k * 2 + 1] : NULL;
        items[k].level = (int)row_opt_int(vm, row, "level", 0, who, "items", k + 1);
        k++;
    }
    int active = cui_toc(tui_need_ui(vm, (TObj *)self, who), scroll_id, items, cnt);
    free_strs(strs, cnt * 2);
    tm_free(items);
    return TV_INT(active);
}

static TVal nm_toc_anchor(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::tocAnchor()";
    const char *anchor = tui_arg_str(vm, args, n, 0, who);
    cui_toc_anchor(tui_need_ui(vm, (TObj *)self, who), anchor);
    return TV_NULL;
}

/* ---- 覆盖层 ---- */

static TVal nm_popover_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::popoverBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    GSTATE->popover_on = cui_popover_begin(tui_need_ui(vm, (TObj *)self, who), id, NULL);
    return open_out(GSTATE->popover_on);
}

static TVal nm_popover_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->popover_on) {
        cui_popover_end(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::popoverEnd()"));
        GSTATE->popover_on = false;
    }
    return TV_NULL;
}

#define POPOVER_OP(name, fn, php)                                        \
static TVal name(VM *vm, void *self, TVal *args, int n) {                 \
    const char *who = "Tphp\\Tui\\App::" php "()";                        \
    const char *id = tui_arg_str(vm, args, n, 0, who);                    \
    cui_##fn(tui_need_ui(vm, (TObj *)self, who), id);                     \
    return TV_NULL;                                                       \
}
POPOVER_OP(nm_popover_open, popover_open, "popoverOpen")
POPOVER_OP(nm_popover_close, popover_close, "popoverClose")
POPOVER_OP(nm_popover_toggle, popover_toggle, "popoverToggle")
#undef POPOVER_OP

static TVal nm_popover_is_open(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::popoverIsOpen()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    return open_out(cui_popover_is_open(tui_need_ui(vm, (TObj *)self, who), id));
}

static TVal nm_menu_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::menuBegin()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    GSTATE->menu_on = cui_menu_begin(tui_need_ui(vm, (TObj *)self, who), label, NULL);
    return open_out(GSTATE->menu_on);
}

static TVal nm_menu_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->menu_on) {
        cui_menu_end(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::menuEnd()"));
        GSTATE->menu_on = false;
    }
    return TV_NULL;
}

static TVal nm_context_menu_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::contextMenuBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    GSTATE->ctxmenu_on = cui_context_menu_begin(tui_need_ui(vm, (TObj *)self, who), id);
    return open_out(GSTATE->ctxmenu_on);
}

static TVal nm_context_menu_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->ctxmenu_on) {
        cui_context_menu_end(tui_need_ui(vm, (TObj *)self,
                                         "Tphp\\Tui\\App::contextMenuEnd()"));
        GSTATE->ctxmenu_on = false;
    }
    return TV_NULL;
}

static TVal nm_menu_item(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::menuItem()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    return open_out(cui_menu_item_ex(tui_need_ui(vm, (TObj *)self, who), label, NULL));
}

static TVal nm_menu_separator(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_menu_separator(tui_need_ui(vm, (TObj *)self,
                                   "Tphp\\Tui\\App::menuSeparator()"));
    return TV_NULL;
}

static TVal nm_menu_label(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::menuLabel()";
    const char *text = tui_arg_str(vm, args, n, 0, who);
    cui_menu_label(tui_need_ui(vm, (TObj *)self, who), text);
    return TV_NULL;
}

/* dialog/sheet：返回 C 的渲染判定（SCOPE_IF 条件），End 按该标记守卫 */
static TVal nm_dialog_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dialogBegin()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    bool open = open_in(vm, args, n, 1, who, "open");
    GSTATE->dialog_on = cui_dialog_begin(tui_need_ui(vm, (TObj *)self, who),
                                   title, &open, NULL);
    return open_out(GSTATE->dialog_on);
}

static TVal nm_dialog_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->dialog_on) {
        cui_dialog_end(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::dialogEnd()"));
        GSTATE->dialog_on = false;
    }
    return TV_NULL;
}

static TVal nm_dialog_footer_begin(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_dialog_footer_begin(tui_need_ui(vm, (TObj *)self,
                                        "Tphp\\Tui\\App::dialogFooterBegin()"));
    return TV_NULL;
}

static TVal nm_dialog_footer_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_dialog_footer_end(tui_need_ui(vm, (TObj *)self,
                                      "Tphp\\Tui\\App::dialogFooterEnd()"));
    return TV_NULL;
}

static TVal nm_sheet_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::sheetBegin()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    bool open = open_in(vm, args, n, 1, who, "open");
    GSTATE->sheet_on = cui_sheet_begin(tui_need_ui(vm, (TObj *)self, who),
                                 title, &open, NULL);
    return open_out(GSTATE->sheet_on);
}

static TVal nm_sheet_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->sheet_on) {
        cui_sheet_end(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::sheetEnd()"));
        GSTATE->sheet_on = false;
    }
    return TV_NULL;
}

static TVal nm_toast(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::toast()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    int h = cui_toast_ex(tui_need_ui(vm, (TObj *)self, who), title, NULL);
    return TV_INT(h);
}

static TVal nm_toast_action_clicked(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::toastActionClicked()";
    int64_t h = tui_arg_int(vm, args, n, 0, who, "handle");
    return open_out(cui_toast_action_clicked(tui_need_ui(vm, (TObj *)self, who),
                                             (int)h));
}

static TVal nm_toast_dismiss(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::toastDismiss()";
    int64_t h = tui_arg_int(vm, args, n, 0, who, "handle");
    cui_toast_dismiss(tui_need_ui(vm, (TObj *)self, who), (int)h);
    return TV_NULL;
}

static TVal nm_alert(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::alert()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    return open_out(cui_alert_ex(tui_need_ui(vm, (TObj *)self, who), title, NULL));
}

static TVal nm_banner(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::banner()";
    const char *text = tui_arg_str(vm, args, n, 0, who);
    bool open = open_in(vm, args, n, 1, who, "open");
    cui_banner_ex(tui_need_ui(vm, (TObj *)self, who), text, &open, NULL);
    return open_out(open);
}

static TVal nm_hover_card_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::hoverCardBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    GSTATE->hover_on = cui_hover_card_begin(tui_need_ui(vm, (TObj *)self, who), id, NULL);
    return open_out(GSTATE->hover_on);
}

static TVal nm_hover_card_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->hover_on) {
        cui_hover_card_end(tui_need_ui(vm, (TObj *)self,
                                       "Tphp\\Tui\\App::hoverCardEnd()"));
        GSTATE->hover_on = false;
    }
    return TV_NULL;
}

static TVal nm_user_menu_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::userMenuBegin()";
    const char *name = tui_arg_str(vm, args, n, 0, who);
    GSTATE->usermenu_on = cui_user_menu_begin(tui_need_ui(vm, (TObj *)self, who), name, NULL);
    return open_out(GSTATE->usermenu_on);
}

static TVal nm_user_menu_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->usermenu_on) {
        cui_user_menu_end(tui_need_ui(vm, (TObj *)self,
                                      "Tphp\\Tui\\App::userMenuEnd()"));
        GSTATE->usermenu_on = false;
    }
    return TV_NULL;
}

/* speedDial：actions=[['icon'=>,'label'=>]] 单次消费，返回选中索引（-1 无） */
static TVal nm_speed_dial(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::speedDial()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "actions");
    int cnt = 0;
    for (uint32_t i = 0; i < rows->dused; i++)
        if (rows->data[i].t != T_UNDEF) cnt++;
    cui_speed_action *acts = cnt ? tm_malloc(sizeof(cui_speed_action) * (size_t)cnt) : NULL;
    char **strs = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt * 2) : NULL;
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "actions", k + 1);
        strs[k * 2] = dup_s(row_opt_str(vm, row, "icon", who, "actions", k + 1)
                            ? row_opt_str(vm, row, "icon", who, "actions", k + 1) : "");
        strs[k * 2 + 1] = dup_s(row_req_str(vm, row, "label", who, "actions", k + 1));
        acts[k].icon = strs[k * 2][0] ? strs[k * 2] : NULL;
        acts[k].label = strs[k * 2 + 1];
        k++;
    }
    int picked = cui_speed_dial_ex(tui_need_ui(vm, (TObj *)self, who), id,
                                   acts, cnt, NULL);
    free_strs(strs, cnt * 2);
    tm_free(acts);
    return TV_INT(picked);
}

static TVal nm_spotlight_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::spotlightBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_spotlight_begin(tui_need_ui(vm, (TObj *)self, who), id, NULL);
    return TV_NULL;
}

static TVal nm_spotlight_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_spotlight_end(tui_need_ui(vm, (TObj *)self,
                                  "Tphp\\Tui\\App::spotlightEnd()"));
    return TV_NULL;
}

/* ---- 列表/树 ---- */

static TVal nm_accordion_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::accordionBegin()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    bool open = open_in(vm, args, n, 1, who, "open");
    GSTATE->accordion_on = cui_accordion_begin(tui_need_ui(vm, (TObj *)self, who),
                                         title, &open);
    return open_out(GSTATE->accordion_on);
}

static TVal nm_accordion_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->accordion_on) {
        cui_accordion_end(tui_need_ui(vm, (TObj *)self,
                                      "Tphp\\Tui\\App::accordionEnd()"));
        GSTATE->accordion_on = false;
    }
    return TV_NULL;
}

static TVal nm_tree_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::treeBegin()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    bool open = open_in(vm, args, n, 1, who, "open");
    GSTATE->tree_on = cui_tree_begin(tui_need_ui(vm, (TObj *)self, who),
                               label, &open, NULL);
    return open_out(GSTATE->tree_on);
}

static TVal nm_tree_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->tree_on) {
        cui_tree_end(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::treeEnd()"));
        GSTATE->tree_on = false;
    }
    return TV_NULL;
}

static TVal nm_tree_clicked(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    return open_out(cui_tree_clicked(tui_need_ui(vm, (TObj *)self,
                                                 "Tphp\\Tui\\App::treeClicked()")));
}

static TVal nm_list_item(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::listItem()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    return open_out(cui_list_item_ex(tui_need_ui(vm, (TObj *)self, who), title, NULL));
}

static TVal nm_virtual_list(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::virtualList()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int64_t cnt = tui_arg_int(vm, args, n, 1, who, "count");
    double row_h = tui_arg_float(vm, args, n, 2, who, "rowHeight");
    double height = tui_opt_float(vm, args, n, 3, 0, who, "height");   /* 0 = 拉满 */
    int first = 0, last = 0;
    cui_size h = height > 0
        ? (cui_size){ CUI_SIZE_FIXED, (float)height }
        : (cui_size){ CUI_SIZE_GROW, 1 };
    cui_virtual_list_begin(tui_need_ui(vm, (TObj *)self, who), id, (int)cnt,
                           (float)row_h, h, &first, &last);
    TArr *a = tarr_new();
    tarr_set(a, TV_STR(tstr_new("first", 5)), TV_INT(first));
    tarr_set(a, TV_STR(tstr_new("last", 4)), TV_INT(last));
    return TV_ARR(a);
}

static TVal nm_virtual_list_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_virtual_list_end(tui_need_ui(vm, (TObj *)self,
                                     "Tphp\\Tui\\App::virtualListEnd()"));
    return TV_NULL;
}

/* swipe：actions=[['icon'=>,'label'=>,'tone'=>0]]；begin 借阅到 end（静态暂存） */
static void swipe_fill(VM *vm, TArr *rows, const char *pname, const char *who,
                       cui_swipe_action **out_acts, int *out_n, TuiLayoutState *S) {
    int cnt = 0;
    for (uint32_t i = 0; i < rows->dused; i++)
        if (rows->data[i].t != T_UNDEF) cnt++;
    /* 第一趟：只校验（raise 前不分配，避免抛出路径泄漏） */
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, pname, pos + 1);
        row_req_str(vm, row, "label", who, pname, pos + 1);
        int64_t tone = row_opt_int(vm, row, "tone", 0, who, pname, pos + 1);
        if (tone < 0 || tone > 8)
            RAISE(vm, "ValueError", "%s(): %s[%d][\"tone\"] must be in 0..8",
                  who, pname, pos);
        pos++;
    }
    /* 第二趟：分配填充 */
    cui_swipe_action *acts = cnt ? tm_malloc(sizeof(cui_swipe_action) * (size_t)cnt) : NULL;
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, pname, k + 1);
        const char *ic = row_opt_str(vm, row, "icon", who, pname, k + 1);
        const char *lb = row_req_str(vm, row, "label", who, pname, k + 1);
        char *cs = dup_s(lb);
        S->swipe_strs = tm_realloc(S->swipe_strs,
                                  sizeof(char *) * (size_t)(S->swipe_nstr + 1));
        S->swipe_strs[S->swipe_nstr++] = cs;
        char *icon_dup = ic ? dup_s(ic) : NULL;
        if (icon_dup) {
            S->swipe_strs = tm_realloc(S->swipe_strs,
                                      sizeof(char *) * (size_t)(S->swipe_nstr + 1));
            S->swipe_strs[S->swipe_nstr++] = icon_dup;
        }
        acts[k].icon = icon_dup;
        acts[k].label = cs;
        acts[k].tone = (cui_tone)row_opt_int(vm, row, "tone", 0, who, pname, k + 1);
        k++;
    }
    *out_acts = acts;
    *out_n = cnt;
}

static TVal nm_swipe_item_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::swipeItemBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    /* 清上一轮借阅 */
    tm_free(GSTATE->swipe_l);
    tm_free(GSTATE->swipe_r);
    free_strs(GSTATE->swipe_strs, GSTATE->swipe_nstr);
    GSTATE->swipe_l = GSTATE->swipe_r = NULL;
    GSTATE->swipe_strs = NULL;
    GSTATE->swipe_ln = GSTATE->swipe_rn = GSTATE->swipe_nstr = 0;
    TArr *left = rows_of(vm, &args[1], who, "left");
    TArr *right = n >= 3 && args[2].t == T_ARR ? (TArr *)args[2].u.p : NULL;
    if (n >= 3 && args[2].t != T_ARR && args[2].t != T_NULL)
        RAISE(vm, "TypeError", "%s(): argument right must be of type array, %s given",
              who, tphp_val_type_name(args[2]));
    swipe_fill(vm, left, "left", who, &GSTATE->swipe_l, &GSTATE->swipe_ln, GSTATE);
    if (right) swipe_fill(vm, right, "right", who, &GSTATE->swipe_r, &GSTATE->swipe_rn, GSTATE);
    cui_swipe_item_begin(tui_need_ui(vm, (TObj *)self, who), id,
                         GSTATE->swipe_l, GSTATE->swipe_ln, GSTATE->swipe_r, GSTATE->swipe_rn);
    return TV_NULL;
}

static TVal nm_swipe_item_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::swipeItemEnd()";
    int picked = cui_swipe_item_end(tui_need_ui(vm, (TObj *)self, who));
    /* end 后 CUI 不再借阅：立即回收 */
    tm_free(GSTATE->swipe_l);
    tm_free(GSTATE->swipe_r);
    free_strs(GSTATE->swipe_strs, GSTATE->swipe_nstr);
    GSTATE->swipe_l = GSTATE->swipe_r = NULL;
    GSTATE->swipe_strs = NULL;
    GSTATE->swipe_ln = GSTATE->swipe_rn = GSTATE->swipe_nstr = 0;
    return TV_INT(picked);   /* 0..left-1 / 100+right / -1 */
}

/* reorder：order 借阅 begin→end（静态拷贝） */
static TVal nm_reorder_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::reorderBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int cnt = 0;
    int64_t *vals = tui_ints(vm, &args[1], who, "order", &cnt);
    tm_free(GSTATE->reorder);
    GSTATE->reorder = NULL;
    GSTATE->reorder_n = 0;
    if (cnt) {
        GSTATE->reorder = tm_malloc(sizeof(int) * (size_t)cnt);
        for (int i = 0; i < cnt; i++) GSTATE->reorder[i] = (int)vals[i];
        GSTATE->reorder_n = cnt;
    }
    tui_arr_free(vals);
    cui_reorder_begin(tui_need_ui(vm, (TObj *)self, who), id, GSTATE->reorder, GSTATE->reorder_n);
    return TV_NULL;
}

static TVal nm_reorder_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::reorderEnd()";
    cui_reorder_end(tui_need_ui(vm, (TObj *)self, who));
    TArr *a = tarr_new();
    for (int i = 0; i < GSTATE->reorder_n; i++) tarr_append(a, TV_INT(GSTATE->reorder[i]));
    tm_free(GSTATE->reorder);
    GSTATE->reorder = NULL;
    GSTATE->reorder_n = 0;
    return TV_ARR(a);   /* 新顺序（未变则与输入相同） */
}

/* ---- 表格 ---- */

/* columns=[['title'=>,'sortable'=>false,'align'=>0,'width'=>0.0]] → 静态拷贝 */
static void grid_cols_fill(VM *vm, TArr *rows, const char *pname, const char *who,
                           cui_grid_column **out, int *out_n, TuiLayoutState *S) {
    int cnt = 0;
    for (uint32_t i = 0; i < rows->dused; i++)
        if (rows->data[i].t != T_UNDEF) cnt++;
    cui_grid_column *cols = cnt ? tm_malloc(sizeof(cui_grid_column) * (size_t)cnt) : NULL;
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, pname, k + 1);
        char *title = dup_s(row_req_str(vm, row, "title", who, pname, k + 1));
        S->gcol_strs = tm_realloc(S->gcol_strs,
                                 sizeof(char *) * (size_t)(S->gcol_nstr + 1));
        S->gcol_strs[S->gcol_nstr++] = title;
        double w = row_opt_float(vm, row, "width", 150, who, pname, k + 1);
        int64_t align = row_opt_int(vm, row, "align", 0, who, pname, k + 1);
        if (align < 0 || align > 4)
            RAISE(vm, "ValueError", "%s(): %s[%d][\"align\"] must be in 0..4",
                  who, pname, k);
        cols[k].title = title;
        cols[k].width = (float)w;
        cols[k].align = (cui_text_align)align;
        cols[k].sortable = row_opt_bool(vm, row, "sortable", false, who, pname, k + 1);
        cols[k].fixed = row_opt_bool(vm, row, "fixed", false, who, pname, k + 1);
        cols[k].priority = (int)row_opt_int(vm, row, "priority", 0, who, pname, k + 1);
        k++;
    }
    *out = cols;
    *out_n = cnt;
}

static void grid_cols_free(TuiLayoutState *S) {
    tm_free(S->gcols);
    free_strs(S->gcol_strs, S->gcol_nstr);
    S->gcols = NULL;
    S->gcol_strs = NULL;
    S->gcoln = S->gcol_nstr = 0;
}

static TVal nm_table_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::tableBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "columns");
    /* 释放上一轮 */
    tm_free(GSTATE->tcols);
    free_strs(GSTATE->tcol_strs, GSTATE->tcol_nstr);
    GSTATE->tcols = NULL;
    GSTATE->tcol_strs = NULL;
    GSTATE->tcoln = GSTATE->tcol_nstr = 0;
    int cnt = 0;
    for (uint32_t i = 0; i < rows->dused; i++)
        if (rows->data[i].t != T_UNDEF) cnt++;
    GSTATE->tcols = cnt ? tm_malloc(sizeof(cui_table_column) * (size_t)cnt) : NULL;
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "columns", k + 1);
        char *title = dup_s(row_req_str(vm, row, "title", who, "columns", k + 1));
        GSTATE->tcol_strs = tm_realloc(GSTATE->tcol_strs,
                                 sizeof(char *) * (size_t)(GSTATE->tcol_nstr + 1));
        GSTATE->tcol_strs[GSTATE->tcol_nstr++] = title;
        GSTATE->tcols[k].title = title;
        GSTATE->tcols[k].width = (cui_size){ CUI_SIZE_GROW, 1 };   /* 默认 fill */
        GSTATE->tcols[k].align = CUI_TEXT_START;
        GSTATE->tcols[k].sortable = row_opt_bool(vm, row, "sortable", false,
                                           who, "columns", k + 1);
        k++;
    }
    GSTATE->tcoln = cnt;
    GSTATE->table_on = cui_table_begin_ex(tui_need_ui(vm, (TObj *)self, who), id,
                                    GSTATE->tcols, GSTATE->tcoln, NULL);
    return open_out(GSTATE->table_on);
}

static TVal nm_table_row(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::tableRow()";
    bool selected = tui_opt_bool(vm, args, n, 0, false, who, "selected");
    return open_out(cui_table_row(tui_need_ui(vm, (TObj *)self, who), selected));
}

static TVal nm_table_cell(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_table_cell(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::tableCell()"));
    return TV_NULL;
}

static TVal nm_table_cell_text(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::tableCellText()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_table_cell_text(tui_need_ui(vm, (TObj *)self, who), s);
    return TV_NULL;
}

static TVal nm_table_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->table_on) {
        const char *who = "Tphp\\Tui\\App::tableEnd()";
        cui_table_end(tui_need_ui(vm, (TObj *)self, who));
        GSTATE->table_on = false;
    }
    tm_free(GSTATE->tcols);
    free_strs(GSTATE->tcol_strs, GSTATE->tcol_nstr);
    GSTATE->tcols = NULL;
    GSTATE->tcol_strs = NULL;
    GSTATE->tcoln = GSTATE->tcol_nstr = 0;
    return TV_NULL;
}

/* dataGridBegin：返回 false（本帧不可建）或 ['first'=>,'last'=>] */
static TVal nm_data_grid_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dataGridBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "columns");
    int64_t row_count = tui_arg_int(vm, args, n, 2, who, "rowCount");
    grid_cols_free(GSTATE);
    grid_cols_fill(vm, rows, "columns", who, &GSTATE->gcols, &GSTATE->gcoln, GSTATE);
    int first = 0, last = 0;
    GSTATE->datagrid_on = cui_data_grid_begin_ex(tui_need_ui(vm, (TObj *)self, who), id,
                                           GSTATE->gcols, GSTATE->gcoln, (int)row_count,
                                           &first, &last, NULL);
    if (!GSTATE->datagrid_on) return TV_FALSE;   /* 列暂存至下一次 begin/end 再回收 */
    TArr *a = tarr_new();
    tarr_set(a, TV_STR(tstr_new("first", 5)), TV_INT(first));
    tarr_set(a, TV_STR(tstr_new("last", 4)), TV_INT(last));
    return TV_ARR(a);
}

static TVal nm_data_grid_row(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dataGridRow()";
    int64_t row = tui_arg_int(vm, args, n, 0, who, "row");
    return open_out(cui_data_grid_row(tui_need_ui(vm, (TObj *)self, who), (int)row));
}

static TVal nm_data_grid_cell(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_data_grid_cell(tui_need_ui(vm, (TObj *)self,
                                   "Tphp\\Tui\\App::dataGridCell()"));
    return TV_NULL;
}

static TVal nm_data_grid_cell_text(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dataGridCellText()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_data_grid_cell_text(tui_need_ui(vm, (TObj *)self, who), s);
    return TV_NULL;
}

/* 可编辑单元格：buffer 往返（值由 PHP 持有），返回编辑后的字符串 */
static TVal nm_data_grid_cell_edit(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dataGridCellEdit()";
    const char *val = tui_arg_str(vm, args, n, 0, who);
    int64_t cap = tui_opt_int(vm, args, n, 1, 256, who, "cap");
    if (cap < 2)
        RAISE(vm, "ValueError", "%s(): cap must be >= 2", who);
    if ((int64_t)strlen(val) + 1 > cap)
        RAISE(vm, "ValueError", "%s(): value is longer than cap (%d)", who, (int)cap);
    char *buf = tm_malloc((size_t)cap);
    memcpy(buf, val, strlen(val) + 1);
    cui_data_grid_cell_edit(tui_need_ui(vm, (TObj *)self, who), buf, (size_t)cap);
    TStr *s = tstr_new(buf, strlen(buf));
    tm_free(buf);
    return TV_STR(s);
}

static TVal nm_data_grid_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->datagrid_on) {
        const char *who = "Tphp\\Tui\\App::dataGridEnd()";
        cui_data_grid_end(tui_need_ui(vm, (TObj *)self, who));
        GSTATE->datagrid_on = false;
    }
    grid_cols_free(GSTATE);
    return TV_NULL;
}

/* treeCell：open 传入当前值，返回是否翻转后的新值 */
static TVal nm_tree_cell(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::treeCell()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    int64_t depth = tui_arg_int(vm, args, n, 1, who, "depth");
    bool has_children = tui_arg_bool(vm, args, n, 2, who, "hasChildren");
    bool open = open_in(vm, args, n, 3, who, "open");
    const char *icon = tui_opt_str(vm, args, n, 4, "", who, "icon");
    cui_tree_cell(tui_need_ui(vm, (TObj *)self, who), label, (int)depth,
                  has_children, &open, icon[0] ? icon : NULL);
    return open_out(open);
}

/* treeTableBegin：nodes=[['parent'=>,'first_child'=>,'next_sibling'=>,'flags'=>]] */
static TVal nm_tree_table_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::treeTableBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *cols_rows = rows_of(vm, &args[1], who, "columns");
    TArr *node_rows = rows_of(vm, &args[2], who, "nodes");
    grid_cols_free(GSTATE);
    grid_cols_fill(vm, cols_rows, "columns", who, &GSTATE->gcols, &GSTATE->gcoln, GSTATE);
    tm_free(GSTATE->tnodes);
    GSTATE->tnodes = NULL;
    GSTATE->tnoden = 0;
    int cnt = 0;
    for (uint32_t i = 0; i < node_rows->dused; i++)
        if (node_rows->data[i].t != T_UNDEF) cnt++;
    GSTATE->tnodes = cnt ? tm_malloc(sizeof(cui_tree_node) * (size_t)cnt) : NULL;
    int k = 0;
    for (uint32_t i = 0; i < node_rows->dused; i++) {
        TVal e = node_rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "nodes", k + 1);
        GSTATE->tnodes[k].parent = (int)row_opt_int(vm, row, "parent", -1, who, "nodes", k + 1);
        GSTATE->tnodes[k].first_child =
            (int)row_opt_int(vm, row, "first_child", -1, who, "nodes", k + 1);
        GSTATE->tnodes[k].next_sibling =
            (int)row_opt_int(vm, row, "next_sibling", -1, who, "nodes", k + 1);
        GSTATE->tnodes[k].flags = (uint32_t)row_opt_int(vm, row, "flags", 0, who, "nodes", k + 1);
        k++;
    }
    GSTATE->tnoden = cnt;
    cui_tree_table_begin_ex(tui_need_ui(vm, (TObj *)self, who), id,
                            GSTATE->gcols, GSTATE->gcoln, GSTATE->tnodes, GSTATE->tnoden, NULL);
    return TV_NULL;
}

static TVal nm_tree_table_row(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    int node = cui_tree_table_row(tui_need_ui(vm, (TObj *)self,
                                              "Tphp\\Tui\\App::treeTableRow()"));
    return TV_INT(node);   /* -1 = 结束 */
}

static TVal nm_tree_table_name(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::treeTableName()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    const char *icon = tui_opt_str(vm, args, n, 1, "", who, "icon");
    cui_tree_table_name(tui_need_ui(vm, (TObj *)self, who), label,
                        icon[0] ? icon : NULL);
    return TV_NULL;
}

static TVal nm_tree_table_depth(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    return TV_INT(cui_tree_table_depth(tui_need_ui(vm, (TObj *)self,
                                                   "Tphp\\Tui\\App::treeTableDepth()")));
}

/* treeTableEnd：返回事件关联数组 activated/expanded/collapsed/load/sort_changed */
static TVal nm_tree_table_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::treeTableEnd()";
    cui_tree_table_events ev = cui_tree_table_end(tui_need_ui(vm, (TObj *)self, who));
    TArr *a = tarr_new();
    tarr_set(a, TV_STR(tstr_new("activated", 9)), TV_INT(ev.activated));
    tarr_set(a, TV_STR(tstr_new("expanded", 8)), TV_INT(ev.expanded));
    tarr_set(a, TV_STR(tstr_new("collapsed", 9)), TV_INT(ev.collapsed));
    tarr_set(a, TV_STR(tstr_new("selection_changed", 18)),
             ev.selection_changed ? TV_TRUE : TV_FALSE);
    tarr_set(a, TV_STR(tstr_new("sort_changed", 12)),
             ev.sort_changed ? TV_TRUE : TV_FALSE);
    TArr *load = tarr_new();
    for (int i = 0; i < ev.load_count; i++) tarr_append(load, TV_INT(ev.load[i]));
    tarr_set(a, TV_STR(tstr_new("load", 4)), TV_ARR(load));
    grid_cols_free(GSTATE);
    tm_free(GSTATE->tnodes);
    GSTATE->tnodes = NULL;
    GSTATE->tnoden = 0;
    return TV_ARR(a);
}

/* filterBar：fields=[['name'=>,'icon'=>,'options'=>[...]]]，
 * selected=int[] 掩码往返、search 字符串往返 → ['changed'=>,'selected'=>,'search'=>] */
static TVal nm_filter_bar(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::filterBar()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "fields");
    int fcnt = 0;
    for (uint32_t i = 0; i < rows->dused; i++)
        if (rows->data[i].t != T_UNDEF) fcnt++;
    /* selected 掩码往返 */
    int mcnt = 0;
    int64_t *masks_in = tui_ints(vm, &args[2], who, "selected", &mcnt);
    if (mcnt != fcnt) {
        tui_arr_free(masks_in);
        RAISE(vm, "ValueError", "%s(): selected length (%d) must match fields (%d)",
              who, mcnt, fcnt);
    }
    uint32_t *masks = fcnt ? tm_malloc(sizeof(uint32_t) * (size_t)fcnt) : NULL;
    for (int i = 0; i < fcnt; i++) masks[i] = (uint32_t)masks_in[i];
    tui_arr_free(masks_in);
    const char *search_in = tui_opt_str(vm, args, n, 3, "", who, "search");
    char search[256];
    snprintf(search, sizeof search, "%s", search_in);
    /* fields 转换（调用期内消费，用完释放） */
    cui_filter_field *fields = fcnt ? tm_malloc(sizeof(cui_filter_field) * (size_t)fcnt) : NULL;
    char **strs = NULL; int nstr = 0;
    char ***optlists = fcnt ? tm_malloc(sizeof(char **) * (size_t)fcnt) : NULL;
    int *optns = fcnt ? tm_malloc(sizeof(int) * (size_t)fcnt) : NULL;
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "fields", k + 1);
        const char *nm = row_req_str(vm, row, "name", who, "fields", k + 1);
        const char *ic = row_opt_str(vm, row, "icon", who, "fields", k + 1);
        char *cs = dup_s(nm);
        strs = tm_realloc(strs, sizeof(char *) * (size_t)(nstr + 1));
        strs[nstr++] = cs;
        fields[k].name = cs;
        if (ic) {
            char *ci = dup_s(ic);
            strs = tm_realloc(strs, sizeof(char *) * (size_t)(nstr + 1));
            strs[nstr++] = ci;
            fields[k].icon = ci;
        } else fields[k].icon = NULL;
        /* options 字符串数组 */
        TVal *ov = row_get(row, "options");
        int onc = 0;
        char **onames = NULL;
        if (ov && ov->t == T_ARR) {
            TArr *oa = (TArr *)ov->u.p;
            for (uint32_t j = 0; j < oa->dused; j++) {
                TVal oe = oa->data[j];
                if (oe.t == T_UNDEF) continue;
                if (oe.t != T_STR) {
                    /* 提前释放已建资源再抛（抛点前的清理） */
                    for (int z = 0; z < nstr; z++) tm_free(strs[z]);
                    tm_free(strs);
                    tm_free(optlists); tm_free(optns);
                    tm_free(fields); tm_free(masks);
                    RAISE(vm, "TypeError",
                          "%s(): fields[%d][\"options\"] elements must be strings",
                          who, k);
                }
                char *os = dup_s(tstr_val((TStr *)oe.u.p));
                onames = tm_realloc(onames, sizeof(char *) * (size_t)(onc + 1));
                onames[onc++] = os;
                strs = tm_realloc(strs, sizeof(char *) * (size_t)(nstr + 1));
                strs[nstr++] = os;
            }
        }
        optlists[k] = onames;
        optns[k] = onc;
        fields[k].options = (const char *const *)onames;
        fields[k].option_count = onc;
        k++;
    }
    bool changed = cui_filter_bar(tui_need_ui(vm, (TObj *)self, who), id,
                                  fields, fcnt, masks, search, sizeof search);
    for (int i = 0; i < fcnt; i++) tm_free(optlists[i]);
    tm_free(optlists);
    tm_free(optns);
    free_strs(strs, nstr);
    tm_free(fields);
    TArr *out_sel = tarr_new();
    for (int i = 0; i < fcnt; i++) tarr_append(out_sel, TV_INT((int64_t)masks[i]));
    tm_free(masks);
    TArr *a = tarr_new();
    tarr_set(a, TV_STR(tstr_new("changed", 7)), changed ? TV_TRUE : TV_FALSE);
    tarr_set(a, TV_STR(tstr_new("selected", 8)), TV_ARR(out_sel));
    tarr_set(a, TV_STR(tstr_new("search", 6)),
             TV_STR(tstr_new(search, strlen(search))));
    return TV_ARR(a);
}

/* ---- 工具条/状态栏/菜单栏 ---- */

static TVal nm_toolbar_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::toolbarBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_toolbar_begin(tui_need_ui(vm, (TObj *)self, who), id, NULL);
    return TV_NULL;
}

static TVal nm_toolbar_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_toolbar_end(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::toolbarEnd()"));
    return TV_NULL;
}

static TVal nm_toolbar_button(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::toolbarButton()";
    const char *icon = tui_arg_str(vm, args, n, 0, who);
    return open_out(cui_toolbar_button_ex(tui_need_ui(vm, (TObj *)self, who), icon, NULL));
}

static TVal nm_toolbar_separator(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_toolbar_separator(tui_need_ui(vm, (TObj *)self,
                                      "Tphp\\Tui\\App::toolbarSeparator()"));
    return TV_NULL;
}

static TVal nm_toolbar_spacer(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_toolbar_spacer(tui_need_ui(vm, (TObj *)self,
                                   "Tphp\\Tui\\App::toolbarSpacer()"));
    return TV_NULL;
}

static TVal nm_status_bar_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::statusBarBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_status_bar_begin(tui_need_ui(vm, (TObj *)self, who), id, NULL);
    return TV_NULL;
}

static TVal nm_status_bar_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_status_bar_end(tui_need_ui(vm, (TObj *)self,
                                   "Tphp\\Tui\\App::statusBarEnd()"));
    return TV_NULL;
}

static TVal nm_status_bar_spacer(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_status_bar_spacer(tui_need_ui(vm, (TObj *)self,
                                      "Tphp\\Tui\\App::statusBarSpacer()"));
    return TV_NULL;
}

static TVal nm_status_bar_text(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::statusBarText()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_status_bar_text_ex(tui_need_ui(vm, (TObj *)self, who), s, NULL);
    return TV_NULL;
}

static TVal nm_status_bar_item(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::statusBarItem()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    return open_out(cui_status_bar_item_ex(tui_need_ui(vm, (TObj *)self, who),
                                           label, NULL));
}

static TVal nm_status_bar_progress(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::statusBarProgress()";
    double v = tui_arg_float(vm, args, n, 0, who, "value");
    cui_status_bar_progress_ex(tui_need_ui(vm, (TObj *)self, who), (float)v, NULL);
    return TV_NULL;
}

static TVal nm_menu_bar_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::menuBarBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_menu_bar_begin(tui_need_ui(vm, (TObj *)self, who), id, NULL);
    return TV_NULL;
}

static TVal nm_menu_bar_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_menu_bar_end(tui_need_ui(vm, (TObj *)self,
                                 "Tphp\\Tui\\App::menuBarEnd()"));
    return TV_NULL;
}

static TVal nm_menu_bar_menu_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::menuBarMenuBegin()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    return open_out(cui_menu_bar_menu_begin(tui_need_ui(vm, (TObj *)self, who),
                                            label, NULL));
}

static TVal nm_menu_bar_menu_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_menu_bar_menu_end(tui_need_ui(vm, (TObj *)self,
                                      "Tphp\\Tui\\App::menuBarMenuEnd()"));
    return TV_NULL;
}

/* ---- 停靠 ---- */

static TVal nm_dock_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dockBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_dock_begin(tui_need_ui(vm, (TObj *)self, who), id, NULL);
    return TV_NULL;
}

static TVal nm_dock_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_dock_end(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::dockEnd()"));
    return TV_NULL;
}

static TVal nm_dock_item(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dockItem()";
    const char *icon = tui_arg_str(vm, args, n, 0, who);
    const char *label = tui_arg_str(vm, args, n, 1, who);
    return open_out(cui_dock_item_ex(tui_need_ui(vm, (TObj *)self, who),
                                     icon, label, NULL));
}

static TVal nm_dock_separator(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_dock_separator(tui_need_ui(vm, (TObj *)self,
                                   "Tphp\\Tui\\App::dockSeparator()"));
    return TV_NULL;
}

static TVal nm_dockspace_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dockspaceBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_dockspace_begin_ex(tui_need_ui(vm, (TObj *)self, who), id, NULL);
    return TV_NULL;
}

static TVal nm_dockspace_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_dockspace_end(tui_need_ui(vm, (TObj *)self,
                                  "Tphp\\Tui\\App::dockspaceEnd()"));
    return TV_NULL;
}

static TVal nm_dock_panel_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dockPanelBegin()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    bool open = open_in(vm, args, n, 1, who, "open");
    GSTATE->dockpanel_on = cui_dock_panel_begin_ex(tui_need_ui(vm, (TObj *)self, who),
                                             title, &open, NULL);
    return open_out(GSTATE->dockpanel_on);
}

static TVal nm_dock_panel_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->dockpanel_on) {
        cui_dock_panel_end(tui_need_ui(vm, (TObj *)self,
                                       "Tphp\\Tui\\App::dockPanelEnd()"));
        GSTATE->dockpanel_on = false;
    }
    return TV_NULL;
}

/* dockSave：先探大小再取（堆缓冲，返回 JSON 字符串） */
static TVal nm_dock_save(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dockSave()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_ctx *ui = tui_need_ui(vm, (TObj *)self, who);
    size_t need = cui_dock_save(ui, id, NULL, 0);
    char *buf = tm_malloc(need + 1);
    size_t got = cui_dock_save(ui, id, buf, need + 1);
    if (got > need) got = need;   /* 并发帧变化的兜底截断（不会发生于单线程） */
    buf[got] = '\0';
    TStr *s = tstr_new(buf, got);
    tm_free(buf);
    return TV_STR(s);
}

static TVal nm_dock_load(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dockLoad()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *text = tui_arg_str(vm, args, n, 1, who);
    return open_out(cui_dock_load(tui_need_ui(vm, (TObj *)self, who), id, text));
}

/* ---- 轮播/看板/卡片栈 ---- */

/* carouselBegin：index 进出，返回新索引 */
static TVal nm_carousel_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::carouselBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int64_t cnt = tui_arg_int(vm, args, n, 1, who, "count");
    int idx = (int)tui_arg_int(vm, args, n, 2, who, "index");
    cui_carousel_begin(tui_need_ui(vm, (TObj *)self, who), id, (int)cnt, &idx, NULL);
    return TV_INT(idx);
}

static TVal nm_carousel_slide_begin(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_carousel_slide_begin(tui_need_ui(vm, (TObj *)self,
                                         "Tphp\\Tui\\App::carouselSlideBegin()"));
    return TV_NULL;
}

static TVal nm_carousel_slide_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_carousel_slide_end(tui_need_ui(vm, (TObj *)self,
                                       "Tphp\\Tui\\App::carouselSlideEnd()"));
    return TV_NULL;
}

static TVal nm_carousel_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_carousel_end(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\App::carouselEnd()"));
    return TV_NULL;
}

static TVal nm_kanban_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::kanbanBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_kanban_begin(tui_need_ui(vm, (TObj *)self, who), id);
    return TV_NULL;
}

static TVal nm_kanban_column_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::kanbanColumnBegin()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    int64_t cards = tui_arg_int(vm, args, n, 1, who, "cardCount");
    return open_out(cui_kanban_column_begin_ex(tui_need_ui(vm, (TObj *)self, who),
                                               title, (int)cards, NULL));
}

static TVal nm_kanban_card_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::kanbanCardBegin()";
    int64_t id = tui_arg_int(vm, args, n, 0, who, "cardId");
    return open_out(cui_kanban_card_begin(tui_need_ui(vm, (TObj *)self, who),
                                          (int)id));
}

static TVal nm_kanban_card_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_kanban_card_end(tui_need_ui(vm, (TObj *)self,
                                    "Tphp\\Tui\\App::kanbanCardEnd()"));
    return TV_NULL;
}

static TVal nm_kanban_column_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_kanban_column_end(tui_need_ui(vm, (TObj *)self,
                                      "Tphp\\Tui\\App::kanbanColumnEnd()"));
    return TV_NULL;
}

/* kanbanEnd：无移动返回 null，有移动返回 ['card'=>,'to_column'=>,'to_index'=>] */
static TVal nm_kanban_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::kanbanEnd()";
    cui_kanban_move m;
    memset(&m, 0, sizeof m);
    bool moved = cui_kanban_end(tui_need_ui(vm, (TObj *)self, who), &m);
    if (!moved) return TV_NULL;
    TArr *a = tarr_new();
    tarr_set(a, TV_STR(tstr_new("card", 4)), TV_INT(m.card));
    tarr_set(a, TV_STR(tstr_new("to_column", 9)), TV_INT(m.to_column));
    tarr_set(a, TV_STR(tstr_new("to_index", 8)), TV_INT(m.to_index));
    return TV_ARR(a);
}

static TVal nm_expandable_card_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::expandableCardBegin()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    bool open = open_in(vm, args, n, 1, who, "open");
    GSTATE->expand_on = cui_expandable_card_begin(tui_need_ui(vm, (TObj *)self, who),
                                            title, &open, NULL);
    return open_out(GSTATE->expand_on);
}

static TVal nm_expandable_card_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->expand_on) {
        cui_expandable_card_end(tui_need_ui(vm, (TObj *)self,
                                            "Tphp\\Tui\\App::expandableCardEnd()"));
        GSTATE->expand_on = false;
    }
    return TV_NULL;
}

static TVal nm_card_stack_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::cardStackBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int64_t cnt = tui_arg_int(vm, args, n, 1, who, "count");
    cui_card_stack_begin(tui_need_ui(vm, (TObj *)self, who), id, (int)cnt, NULL);
    return TV_NULL;
}

static TVal nm_card_stack_item_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::cardStackItemBegin()";
    int64_t depth = tui_arg_int(vm, args, n, 0, who, "depth");
    cui_card_stack_item_begin(tui_need_ui(vm, (TObj *)self, who), (int)depth);
    return TV_NULL;
}

static TVal nm_card_stack_item_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_card_stack_item_end(tui_need_ui(vm, (TObj *)self,
                                        "Tphp\\Tui\\App::cardStackItemEnd()"));
    return TV_NULL;
}

static TVal nm_card_stack_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    return TV_INT(cui_card_stack_end(tui_need_ui(vm, (TObj *)self,
                                                 "Tphp\\Tui\\App::cardStackEnd()")));
}

/* ---- 注册 ---- */

void tui_register_layout(TClass *cls) {
    /* 容器 */
    tui_cls_add_native(cls, "boxBegin", (void *)nm_box_begin, false);
    tui_cls_add_native(cls, "scrollBegin", (void *)nm_scroll_begin, false);
    tui_cls_add_native(cls, "scrollEnd", (void *)nm_scroll_end, false);
    tui_cls_add_native(cls, "scrollTo", (void *)nm_scroll_to, false);
    tui_cls_add_native(cls, "splitBegin", (void *)nm_split_begin, false);
    tui_cls_add_native(cls, "splitNext", (void *)nm_split_next, false);
    tui_cls_add_native(cls, "splitEnd", (void *)nm_split_end, false);
    /* 导航 */
    tui_cls_add_native(cls, "tabs", (void *)nm_tabs, false);
    tui_cls_add_native(cls, "navItem", (void *)nm_nav_item, false);
    tui_cls_add_native(cls, "navSection", (void *)nm_nav_section, false);
    tui_cls_add_native(cls, "breadcrumb", (void *)nm_breadcrumb, false);
    tui_cls_add_native(cls, "pagination", (void *)nm_pagination, false);
    tui_cls_add_native(cls, "stepper", (void *)nm_stepper, false);
    tui_cls_add_native(cls, "pathBar", (void *)nm_path_bar, false);
    tui_cls_add_native(cls, "pathBarMenu", (void *)nm_path_bar_menu, false);
    tui_cls_add_native(cls, "pathBarMenuEnd", (void *)nm_path_bar_menu_end, false);
    tui_cls_add_native(cls, "toc", (void *)nm_toc, false);
    tui_cls_add_native(cls, "tocAnchor", (void *)nm_toc_anchor, false);
    /* 覆盖层 */
    tui_cls_add_native(cls, "popoverBegin", (void *)nm_popover_begin, false);
    tui_cls_add_native(cls, "popoverEnd", (void *)nm_popover_end, false);
    tui_cls_add_native(cls, "popoverOpen", (void *)nm_popover_open, false);
    tui_cls_add_native(cls, "popoverClose", (void *)nm_popover_close, false);
    tui_cls_add_native(cls, "popoverToggle", (void *)nm_popover_toggle, false);
    tui_cls_add_native(cls, "popoverIsOpen", (void *)nm_popover_is_open, false);
    tui_cls_add_native(cls, "menuBegin", (void *)nm_menu_begin, false);
    tui_cls_add_native(cls, "menuEnd", (void *)nm_menu_end, false);
    tui_cls_add_native(cls, "contextMenuBegin", (void *)nm_context_menu_begin, false);
    tui_cls_add_native(cls, "contextMenuEnd", (void *)nm_context_menu_end, false);
    tui_cls_add_native(cls, "menuItem", (void *)nm_menu_item, false);
    tui_cls_add_native(cls, "menuSeparator", (void *)nm_menu_separator, false);
    tui_cls_add_native(cls, "menuLabel", (void *)nm_menu_label, false);
    tui_cls_add_native(cls, "dialogBegin", (void *)nm_dialog_begin, false);
    tui_cls_add_native(cls, "dialogEnd", (void *)nm_dialog_end, false);
    tui_cls_add_native(cls, "dialogFooterBegin", (void *)nm_dialog_footer_begin, false);
    tui_cls_add_native(cls, "dialogFooterEnd", (void *)nm_dialog_footer_end, false);
    tui_cls_add_native(cls, "sheetBegin", (void *)nm_sheet_begin, false);
    tui_cls_add_native(cls, "sheetEnd", (void *)nm_sheet_end, false);
    tui_cls_add_native(cls, "toast", (void *)nm_toast, false);
    tui_cls_add_native(cls, "toastActionClicked", (void *)nm_toast_action_clicked, false);
    tui_cls_add_native(cls, "toastDismiss", (void *)nm_toast_dismiss, false);
    tui_cls_add_native(cls, "alert", (void *)nm_alert, false);
    tui_cls_add_native(cls, "banner", (void *)nm_banner, false);
    tui_cls_add_native(cls, "hoverCardBegin", (void *)nm_hover_card_begin, false);
    tui_cls_add_native(cls, "hoverCardEnd", (void *)nm_hover_card_end, false);
    tui_cls_add_native(cls, "userMenuBegin", (void *)nm_user_menu_begin, false);
    tui_cls_add_native(cls, "userMenuEnd", (void *)nm_user_menu_end, false);
    tui_cls_add_native(cls, "speedDial", (void *)nm_speed_dial, false);
    tui_cls_add_native(cls, "spotlightBegin", (void *)nm_spotlight_begin, false);
    tui_cls_add_native(cls, "spotlightEnd", (void *)nm_spotlight_end, false);
    /* 列表/树 */
    tui_cls_add_native(cls, "accordionBegin", (void *)nm_accordion_begin, false);
    tui_cls_add_native(cls, "accordionEnd", (void *)nm_accordion_end, false);
    tui_cls_add_native(cls, "treeBegin", (void *)nm_tree_begin, false);
    tui_cls_add_native(cls, "treeEnd", (void *)nm_tree_end, false);
    tui_cls_add_native(cls, "treeClicked", (void *)nm_tree_clicked, false);
    tui_cls_add_native(cls, "listItem", (void *)nm_list_item, false);
    tui_cls_add_native(cls, "virtualList", (void *)nm_virtual_list, false);
    tui_cls_add_native(cls, "virtualListEnd", (void *)nm_virtual_list_end, false);
    tui_cls_add_native(cls, "swipeItemBegin", (void *)nm_swipe_item_begin, false);
    tui_cls_add_native(cls, "swipeItemEnd", (void *)nm_swipe_item_end, false);
    tui_cls_add_native(cls, "reorderBegin", (void *)nm_reorder_begin, false);
    tui_cls_add_native(cls, "reorderEnd", (void *)nm_reorder_end, false);
    /* 表格 */
    tui_cls_add_native(cls, "tableBegin", (void *)nm_table_begin, false);
    tui_cls_add_native(cls, "tableRow", (void *)nm_table_row, false);
    tui_cls_add_native(cls, "tableCell", (void *)nm_table_cell, false);
    tui_cls_add_native(cls, "tableCellText", (void *)nm_table_cell_text, false);
    tui_cls_add_native(cls, "tableEnd", (void *)nm_table_end, false);
    tui_cls_add_native(cls, "dataGridBegin", (void *)nm_data_grid_begin, false);
    tui_cls_add_native(cls, "dataGridRow", (void *)nm_data_grid_row, false);
    tui_cls_add_native(cls, "dataGridCell", (void *)nm_data_grid_cell, false);
    tui_cls_add_native(cls, "dataGridCellText", (void *)nm_data_grid_cell_text, false);
    tui_cls_add_native(cls, "dataGridCellEdit", (void *)nm_data_grid_cell_edit, false);
    tui_cls_add_native(cls, "dataGridEnd", (void *)nm_data_grid_end, false);
    tui_cls_add_native(cls, "treeCell", (void *)nm_tree_cell, false);
    tui_cls_add_native(cls, "treeTableBegin", (void *)nm_tree_table_begin, false);
    tui_cls_add_native(cls, "treeTableRow", (void *)nm_tree_table_row, false);
    tui_cls_add_native(cls, "treeTableName", (void *)nm_tree_table_name, false);
    tui_cls_add_native(cls, "treeTableDepth", (void *)nm_tree_table_depth, false);
    tui_cls_add_native(cls, "treeTableEnd", (void *)nm_tree_table_end, false);
    tui_cls_add_native(cls, "filterBar", (void *)nm_filter_bar, false);
    /* 工具条/状态栏/菜单栏 */
    tui_cls_add_native(cls, "toolbarBegin", (void *)nm_toolbar_begin, false);
    tui_cls_add_native(cls, "toolbarEnd", (void *)nm_toolbar_end, false);
    tui_cls_add_native(cls, "toolbarButton", (void *)nm_toolbar_button, false);
    tui_cls_add_native(cls, "toolbarSeparator", (void *)nm_toolbar_separator, false);
    tui_cls_add_native(cls, "toolbarSpacer", (void *)nm_toolbar_spacer, false);
    tui_cls_add_native(cls, "statusBarBegin", (void *)nm_status_bar_begin, false);
    tui_cls_add_native(cls, "statusBarEnd", (void *)nm_status_bar_end, false);
    tui_cls_add_native(cls, "statusBarSpacer", (void *)nm_status_bar_spacer, false);
    tui_cls_add_native(cls, "statusBarText", (void *)nm_status_bar_text, false);
    tui_cls_add_native(cls, "statusBarItem", (void *)nm_status_bar_item, false);
    tui_cls_add_native(cls, "statusBarProgress", (void *)nm_status_bar_progress, false);
    tui_cls_add_native(cls, "menuBarBegin", (void *)nm_menu_bar_begin, false);
    tui_cls_add_native(cls, "menuBarEnd", (void *)nm_menu_bar_end, false);
    tui_cls_add_native(cls, "menuBarMenuBegin", (void *)nm_menu_bar_menu_begin, false);
    tui_cls_add_native(cls, "menuBarMenuEnd", (void *)nm_menu_bar_menu_end, false);
    /* 停靠 */
    tui_cls_add_native(cls, "dockBegin", (void *)nm_dock_begin, false);
    tui_cls_add_native(cls, "dockEnd", (void *)nm_dock_end, false);
    tui_cls_add_native(cls, "dockItem", (void *)nm_dock_item, false);
    tui_cls_add_native(cls, "dockSeparator", (void *)nm_dock_separator, false);
    tui_cls_add_native(cls, "dockspaceBegin", (void *)nm_dockspace_begin, false);
    tui_cls_add_native(cls, "dockspaceEnd", (void *)nm_dockspace_end, false);
    tui_cls_add_native(cls, "dockPanelBegin", (void *)nm_dock_panel_begin, false);
    tui_cls_add_native(cls, "dockPanelEnd", (void *)nm_dock_panel_end, false);
    tui_cls_add_native(cls, "dockSave", (void *)nm_dock_save, false);
    tui_cls_add_native(cls, "dockLoad", (void *)nm_dock_load, false);
    /* 轮播/看板/卡片栈 */
    tui_cls_add_native(cls, "carouselBegin", (void *)nm_carousel_begin, false);
    tui_cls_add_native(cls, "carouselSlideBegin", (void *)nm_carousel_slide_begin, false);
    tui_cls_add_native(cls, "carouselSlideEnd", (void *)nm_carousel_slide_end, false);
    tui_cls_add_native(cls, "carouselEnd", (void *)nm_carousel_end, false);
    tui_cls_add_native(cls, "kanbanBegin", (void *)nm_kanban_begin, false);
    tui_cls_add_native(cls, "kanbanColumnBegin", (void *)nm_kanban_column_begin, false);
    tui_cls_add_native(cls, "kanbanCardBegin", (void *)nm_kanban_card_begin, false);
    tui_cls_add_native(cls, "kanbanCardEnd", (void *)nm_kanban_card_end, false);
    tui_cls_add_native(cls, "kanbanColumnEnd", (void *)nm_kanban_column_end, false);
    tui_cls_add_native(cls, "kanbanEnd", (void *)nm_kanban_end, false);
    tui_cls_add_native(cls, "expandableCardBegin", (void *)nm_expandable_card_begin, false);
    tui_cls_add_native(cls, "expandableCardEnd", (void *)nm_expandable_card_end, false);
    tui_cls_add_native(cls, "cardStackBegin", (void *)nm_card_stack_begin, false);
    tui_cls_add_native(cls, "cardStackItemBegin", (void *)nm_card_stack_item_begin, false);
    tui_cls_add_native(cls, "cardStackItemEnd", (void *)nm_card_stack_item_end, false);
    tui_cls_add_native(cls, "cardStackEnd", (void *)nm_card_stack_end, false);
}
