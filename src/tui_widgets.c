/*
 * tphp/tui - src/tui_widgets.c - Task 2 反馈与显示小件域
 *
 * 覆盖：文本扩展（muted/code/kbd/link/icon/tooltip/gap）、卡片、徽章头像、
 * 进度反馈、按钮族、文本特效、reactions/ticker/counter 等显示件。
 * opts 一律走 _ex + NULL 默认（CUI 内部默认值）；返回值照 C 侧语义：
 * 点击型 bool、索引型 int、其余 void。关联数组（reactions/ticker）的
 * PHP 形态见各转换助手注释。
 */
#include "tui_util.h"

#include <string.h>

#define RAISE(vm, cls, ...) tphp_vm_raise(vm, cls, __VA_ARGS__)

/* ---- 关联数组行读取（PHP assoc row → C 结构，仅本域使用） ---- */

static TVal *row_get(TArr *row, const char *k) {
    TVal key = TV_STR(tstr_new(k, strlen(k)));
    TVal *p = tarr_get(row, key);
    tval_release(&key);
    return p;
}

/* 校验行必须是 array，否则 TypeError（位置 1 起） */
static TArr *row_arr(VM *vm, TVal e, const char *who, const char *pname, int pos) {
    if (e.t != T_ARR)
        RAISE(vm, "TypeError",
              "%s(): array element %d of argument %s must be of type array, %s given",
              who, pos, pname, tphp_val_type_name(e));
    return (TArr *)e.u.p;
}

/* 0xRRGGBBAA → cui_color；v < 0 表示未给 → 全零（CUI 取主题前景色） */
static cui_color col_of(int64_t v) {
    cui_color c = { 0, 0, 0, 0 };
    if (v < 0) return c;
    uint32_t u = (uint32_t)v;
    c.r = (uint8_t)((u >> 24) & 0xFF);
    c.g = (uint8_t)((u >> 16) & 0xFF);
    c.b = (uint8_t)((u >> 8) & 0xFF);
    c.a = (uint8_t)(u & 0xFF);
    if (c.a == 0) c.a = 255;   /* 只给 0xRRGGBB 时补不透明 */
    return c;
}

/* ---- 文本扩展 ---- */

static TVal nm_muted(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::muted()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_muted(tui_need_ui(vm, (TObj *)self, who), s);
    return TV_NULL;
}

static TVal nm_code(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::code()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_code(tui_need_ui(vm, (TObj *)self, who), s);
    return TV_NULL;
}

static TVal nm_kbd(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::kbd()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_kbd(tui_need_ui(vm, (TObj *)self, who), s);
    return TV_NULL;
}

static TVal nm_link(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::link()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    bool clicked = cui_link(tui_need_ui(vm, (TObj *)self, who), s);
    return clicked ? TV_TRUE : TV_FALSE;
}

static TVal nm_icon(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::icon()";
    const char *name = tui_arg_str(vm, args, n, 0, who);
    double size = tui_opt_float(vm, args, n, 1, 0, who, "size");      /* 0 → CUI 默认 16 */
    int64_t color = tui_opt_int(vm, args, n, 2, -1, who, "color");    /* -1 → 主题前景色 */
    cui_icon(tui_need_ui(vm, (TObj *)self, who), name, (float)size, col_of(color));
    return TV_NULL;
}

static TVal nm_tooltip(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::tooltip()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_tooltip(tui_need_ui(vm, (TObj *)self, who), s);
    return TV_NULL;
}

static TVal nm_gap(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::gap()";
    double size = n >= 1 ? tui_opt_float(vm, args, n, 0, 8, who, "size") : 8;
    cui_gap(tui_need_ui(vm, (TObj *)self, who), (float)size);
    return TV_NULL;
}

/* ---- 卡片（begin/end scope） ---- */

static TVal nm_card_begin(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::cardBegin()";
    bool clicked = cui_card_begin(tui_need_ui(vm, (TObj *)self, who), NULL);
    return clicked ? TV_TRUE : TV_FALSE;
}

static TVal nm_card_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::cardEnd()";
    cui_card_end(tui_need_ui(vm, (TObj *)self, who));
    return TV_NULL;
}

/* ---- 徽章头像 ---- */

static TVal nm_badge(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::badge()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_badge_ex(tui_need_ui(vm, (TObj *)self, who), s, NULL);
    return TV_NULL;
}

static TVal nm_avatar(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::avatar()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_avatar_ex(tui_need_ui(vm, (TObj *)self, who), s, NULL);
    return TV_NULL;
}

static TVal nm_avatar_group(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::avatarGroup()";
    int cnt = 0;
    char **names = tui_strs(vm, &args[0], who, "names", &cnt);
    int64_t max_vis = tui_opt_int(vm, args, n, 1, 3, who, "maxVisible");
    double size = tui_opt_float(vm, args, n, 2, 32, who, "size");
    cui_avatar_group(tui_need_ui(vm, (TObj *)self, who),
                     (const char *const *)names, cnt, (int)max_vis, (float)size);
    tui_strs_free(names, cnt);
    return TV_NULL;
}

/* ---- 进度反馈 ---- */

static TVal nm_progress(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::progress()";
    double v = n >= 1 ? tui_opt_float(vm, args, n, 0, 0, who, "value") : 0;
    cui_progress_ex(tui_need_ui(vm, (TObj *)self, who), (float)v, NULL);
    return TV_NULL;
}

static TVal nm_progress_ring(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::progressRing()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    double v = n >= 2 ? tui_opt_float(vm, args, n, 1, 0, who, "value") : 0;
    double size = tui_opt_float(vm, args, n, 2, 48, who, "size");
    cui_progress_ring(tui_need_ui(vm, (TObj *)self, who), label,
                      (float)v, (float)size);
    return TV_NULL;
}

static TVal nm_spinner(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::spinner()";
    double size = tui_opt_float(vm, args, n, 0, 24, who, "size");
    cui_spinner(tui_need_ui(vm, (TObj *)self, who), (float)size);
    return TV_NULL;
}

static TVal nm_skeleton(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::skeleton()";
    double height = tui_opt_float(vm, args, n, 0, 20, who, "height");
    /* 宽度拉满（CUI_FILL），高度为内容占位行高 */
    cui_skeleton(tui_need_ui(vm, (TObj *)self, who), CUI_FILL, (float)height);
    return TV_NULL;
}

static TVal nm_loader(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\App::loader()";
    cui_loader_ex(tui_need_ui(vm, (TObj *)self, who), NULL);
    return TV_NULL;
}

/* ---- 状态/信息 ---- */

static TVal nm_empty_state(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::emptyState()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    bool action = cui_empty_state_ex(tui_need_ui(vm, (TObj *)self, who), title, NULL);
    return action ? TV_TRUE : TV_FALSE;
}

static TVal nm_metric(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::metric()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    const char *value = tui_arg_str(vm, args, n, 1, who);
    cui_metric_ex(tui_need_ui(vm, (TObj *)self, who), label, value, NULL);
    return TV_NULL;
}

static TVal nm_meter(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::meter()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    double used = tui_arg_float(vm, args, n, 1, who, "used");
    double total = tui_arg_float(vm, args, n, 2, who, "total");
    const char *unit = tui_opt_str(vm, args, n, 3, "", who, "unit");
    cui_meter(tui_need_ui(vm, (TObj *)self, who), label,
              (float)used, (float)total, unit[0] ? unit : NULL);
    return TV_NULL;
}

/* ---- 按钮族 ---- */

static TVal nm_icon_button(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::iconButton()";
    const char *icon = tui_arg_str(vm, args, n, 0, who);
    bool clicked = cui_icon_button_ex(tui_need_ui(vm, (TObj *)self, who), icon, NULL);
    return clicked ? TV_TRUE : TV_FALSE;
}

static TVal nm_copy_button(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::copyButton()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *text = tui_arg_str(vm, args, n, 1, who);
    bool clicked = cui_copy_button(tui_need_ui(vm, (TObj *)self, who), id, text);
    return clicked ? TV_TRUE : TV_FALSE;
}

static TVal nm_hold_button(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::holdButton()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    bool done = cui_hold_button_ex(tui_need_ui(vm, (TObj *)self, who), label, NULL);
    return done ? TV_TRUE : TV_FALSE;
}

static TVal nm_confirm_button(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::confirmButton()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    bool ok = cui_confirm_button_ex(tui_need_ui(vm, (TObj *)self, who), label, NULL);
    return ok ? TV_TRUE : TV_FALSE;
}

static TVal nm_button_group_begin(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_button_group_begin(tui_need_ui(vm, (TObj *)self,
                                       "Tphp\\Tui\\App::buttonGroupBegin()"));
    return TV_NULL;
}

static TVal nm_button_group_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_button_group_end(tui_need_ui(vm, (TObj *)self,
                                     "Tphp\\Tui\\App::buttonGroupEnd()"));
    return TV_NULL;
}

static TVal nm_split_button(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::splitButton()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    int cnt = 0;
    char **items = tui_strs(vm, &args[1], who, "items", &cnt);
    int picked = cui_split_button_ex(tui_need_ui(vm, (TObj *)self, who),
                                     label, (const char *const *)items, cnt, NULL);
    tui_strs_free(items, cnt);
    return TV_INT(picked);
}

static TVal nm_status_button(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::statusButton()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    int64_t st = tui_opt_int(vm, args, n, 1, 0, who, "status");
    if (st < 0 || st > 3)   /* cui_status: idle/busy/success/error */
        RAISE(vm, "ValueError", "%s(): status must be in 0..3", who);
    bool clicked = cui_status_button_ex(tui_need_ui(vm, (TObj *)self, who),
                                        label, (cui_status)st, NULL);
    return clicked ? TV_TRUE : TV_FALSE;
}

/* reactions：items = [['icon'=>'👍','count'=>3,'active'=>false], ...]
 * icon 必填 string；count/active 可选（缺省 0/false） */
static TVal nm_reactions(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::reactions()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    if (n <= 1 || args[1].t != T_ARR)
        RAISE(vm, "TypeError", "%s(): argument items must be of type array, %s given",
              who, n <= 1 ? "null" : tphp_val_type_name(args[1]));
    TArr *rows = (TArr *)args[1].u.p;
    /* 第一趟：全量校验（raise 前不分配） */
    int cnt = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "items", cnt + 1);
        TVal *ic = row_get(row, "icon");
        if (!ic || ic->t != T_STR)
            RAISE(vm, "TypeError", "%s(): items[%d][\"icon\"] must be a string", who, cnt);
        TVal *ct = row_get(row, "count");
        if (ct && ct->t != T_INT)
            RAISE(vm, "TypeError", "%s(): items[%d][\"count\"] must be an int", who, cnt);
        TVal *ac = row_get(row, "active");
        if (ac && ac->t != T_TRUE && ac->t != T_FALSE)
            RAISE(vm, "TypeError", "%s(): items[%d][\"active\"] must be a bool", who, cnt);
        cnt++;
    }
    /* 第二趟：填充（无 raise 点） */
    cui_reaction *out = cnt ? tm_malloc(sizeof(cui_reaction) * (size_t)cnt) : NULL;
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        TVal *ic = row_get(row, "icon");
        TVal *ct = row_get(row, "count");
        TVal *ac = row_get(row, "active");
        out[k].icon = tstr_val((TStr *)ic->u.p);
        out[k].count = ct ? (int)ct->u.i : 0;
        out[k].active = ac ? ac->t == T_TRUE : false;
        k++;
    }
    int picked = cui_reactions(tui_need_ui(vm, (TObj *)self, who), id, out, cnt);
    tm_free(out);
    return TV_INT(picked);
}

/* ---- 文本特效 ---- */

static TVal nm_shimmer_text(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::shimmerText()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_shimmer_text_ex(tui_need_ui(vm, (TObj *)self, who), s, NULL);
    return TV_NULL;
}

static TVal nm_typewriter(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::typewriter()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *s = tui_arg_str(vm, args, n, 1, who);
    bool done = cui_typewriter_ex(tui_need_ui(vm, (TObj *)self, who), id, s, NULL);
    return done ? TV_TRUE : TV_FALSE;
}

static TVal nm_text_reveal(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::textReveal()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *s = tui_arg_str(vm, args, n, 1, who);
    cui_text_reveal_ex(tui_need_ui(vm, (TObj *)self, who), id, s, NULL);
    return TV_NULL;
}

static TVal nm_word_rotate(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::wordRotate()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int cnt = 0;
    char **words = tui_strs(vm, &args[1], who, "words", &cnt);
    double interval = tui_opt_float(vm, args, n, 2, 2.0, who, "interval");
    cui_word_rotate_ex(tui_need_ui(vm, (TObj *)self, who), id,
                       (const char *const *)words, cnt, (float)interval, NULL);
    tui_strs_free(words, cnt);
    return TV_NULL;
}

static TVal nm_number_roll(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::numberRoll()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *s = tui_arg_str(vm, args, n, 1, who);
    cui_number_roll_ex(tui_need_ui(vm, (TObj *)self, who), id, s, NULL);
    return TV_NULL;
}

static TVal nm_gradient_text(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::gradientText()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_gradient_text_ex(tui_need_ui(vm, (TObj *)self, who), s, NULL);
    return TV_NULL;
}

static TVal nm_counter(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::counter()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    double v = n >= 2 ? tui_opt_float(vm, args, n, 1, 0, who, "value") : 0;
    cui_counter_ex(tui_need_ui(vm, (TObj *)self, who), id, v, NULL);
    return TV_NULL;
}

/* ticker：items = [['label'=>'BTC','value'=>'67,240','delta'=>1.2,'lower_is_better'=>false], ...]
 * label 必填；value 可空串→NULL；delta/lower_is_better 可选 */
static TVal nm_ticker(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::ticker()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    if (n <= 1 || args[1].t != T_ARR)
        RAISE(vm, "TypeError", "%s(): argument items must be of type array, %s given",
              who, n <= 1 ? "null" : tphp_val_type_name(args[1]));
    TArr *rows = (TArr *)args[1].u.p;
    /* 第一趟：校验 */
    int cnt = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "items", cnt + 1);
        TVal *lb = row_get(row, "label");
        if (!lb || lb->t != T_STR)
            RAISE(vm, "TypeError", "%s(): items[%d][\"label\"] must be a string", who, cnt);
        TVal *vl = row_get(row, "value");
        if (vl && vl->t != T_STR)
            RAISE(vm, "TypeError", "%s(): items[%d][\"value\"] must be a string", who, cnt);
        TVal *dl = row_get(row, "delta");
        if (dl && dl->t != T_INT && dl->t != T_FLOAT)
            RAISE(vm, "TypeError", "%s(): items[%d][\"delta\"] must be numeric", who, cnt);
        TVal *lb2 = row_get(row, "lower_is_better");
        if (lb2 && lb2->t != T_TRUE && lb2->t != T_FALSE)
            RAISE(vm, "TypeError", "%s(): items[%d][\"lower_is_better\"] must be a bool",
                  who, cnt);
        cnt++;
    }
    /* 第二趟：填充。字符串指针借用 TStr 内部缓冲——仅本次调用内有效，
     * CUI 立即模式也只在调用期间使用，调用后无需拷贝。 */
    cui_ticker_item *out = cnt ? tm_malloc(sizeof(cui_ticker_item) * (size_t)cnt) : NULL;
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        TVal *lb = row_get(row, "label");
        TVal *vl = row_get(row, "value");
        TVal *dl = row_get(row, "delta");
        TVal *lb2 = row_get(row, "lower_is_better");
        out[k].label = tstr_val((TStr *)lb->u.p);
        out[k].value = (vl && vl->t == T_STR && ((TStr *)vl->u.p)->len)
            ? tstr_val((TStr *)vl->u.p) : NULL;
        out[k].delta = dl ? tval_to_float(*dl) : 0.0f;
        out[k].lower_is_better = lb2 ? lb2->t == T_TRUE : false;
        k++;
    }
    cui_ticker_ex(tui_need_ui(vm, (TObj *)self, who), id, out, cnt, NULL);
    tm_free(out);
    return TV_NULL;
}

/* ---- 注册 ---- */

void tui_register_widgets(TClass *cls) {
    tui_cls_add_native(cls, "muted", (void *)nm_muted, false);
    tui_cls_add_native(cls, "code", (void *)nm_code, false);
    tui_cls_add_native(cls, "kbd", (void *)nm_kbd, false);
    tui_cls_add_native(cls, "link", (void *)nm_link, false);
    tui_cls_add_native(cls, "icon", (void *)nm_icon, false);
    tui_cls_add_native(cls, "tooltip", (void *)nm_tooltip, false);
    tui_cls_add_native(cls, "gap", (void *)nm_gap, false);
    tui_cls_add_native(cls, "cardBegin", (void *)nm_card_begin, false);
    tui_cls_add_native(cls, "cardEnd", (void *)nm_card_end, false);
    tui_cls_add_native(cls, "badge", (void *)nm_badge, false);
    tui_cls_add_native(cls, "avatar", (void *)nm_avatar, false);
    tui_cls_add_native(cls, "avatarGroup", (void *)nm_avatar_group, false);
    tui_cls_add_native(cls, "progress", (void *)nm_progress, false);
    tui_cls_add_native(cls, "progressRing", (void *)nm_progress_ring, false);
    tui_cls_add_native(cls, "spinner", (void *)nm_spinner, false);
    tui_cls_add_native(cls, "skeleton", (void *)nm_skeleton, false);
    tui_cls_add_native(cls, "loader", (void *)nm_loader, false);
    tui_cls_add_native(cls, "emptyState", (void *)nm_empty_state, false);
    tui_cls_add_native(cls, "metric", (void *)nm_metric, false);
    tui_cls_add_native(cls, "meter", (void *)nm_meter, false);
    tui_cls_add_native(cls, "iconButton", (void *)nm_icon_button, false);
    tui_cls_add_native(cls, "copyButton", (void *)nm_copy_button, false);
    tui_cls_add_native(cls, "holdButton", (void *)nm_hold_button, false);
    tui_cls_add_native(cls, "confirmButton", (void *)nm_confirm_button, false);
    tui_cls_add_native(cls, "buttonGroupBegin", (void *)nm_button_group_begin, false);
    tui_cls_add_native(cls, "buttonGroupEnd", (void *)nm_button_group_end, false);
    tui_cls_add_native(cls, "splitButton", (void *)nm_split_button, false);
    tui_cls_add_native(cls, "statusButton", (void *)nm_status_button, false);
    tui_cls_add_native(cls, "reactions", (void *)nm_reactions, false);
    tui_cls_add_native(cls, "shimmerText", (void *)nm_shimmer_text, false);
    tui_cls_add_native(cls, "typewriter", (void *)nm_typewriter, false);
    tui_cls_add_native(cls, "textReveal", (void *)nm_text_reveal, false);
    tui_cls_add_native(cls, "wordRotate", (void *)nm_word_rotate, false);
    tui_cls_add_native(cls, "numberRoll", (void *)nm_number_roll, false);
    tui_cls_add_native(cls, "gradientText", (void *)nm_gradient_text, false);
    tui_cls_add_native(cls, "counter", (void *)nm_counter, false);
    tui_cls_add_native(cls, "ticker", (void *)nm_ticker, false);
}
