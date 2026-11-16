/*
 * tphp/tui - src/tui_content.c - Task 6 富内容组件域（核心部分）
 *
 * 单例状态（PHP 侧无需持有，init 时重建）：terminal/log 缓冲、image 查看
 * 视图、hex 选择、裁剪矩形——各进程一份，文档记档 coverage.md。
 * buffer 往返型（fileBrowser/filePicker/requestBar/codeEditor）值由 PHP 持有。
 * 结构体数组照两趟模式（先校验后分配，raise 不泄漏）。
 */
#include "tui_util.h"

#include <stdio.h>
#include <string.h>

#define RAISE(vm, cls, ...) tphp_vm_raise(vm, cls, __VA_ARGS__)

/* ---- 行读取小工具 ---- */

static TVal *row_get(TArr *row, const char *k) {
    TVal key = TV_STR(tstr_new(k, strlen(k)));
    TVal *p = tarr_get(row, key);
    tval_release(&key);
    return p;
}

static char *dup_s(const char *s) {
    size_t n = strlen(s);
    char *p = tm_malloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

static TArr *rows_of(VM *vm, TVal *v, const char *who, const char *pname) {
    if (v->t != T_ARR)
        RAISE(vm, "TypeError", "%s: argument %s must be of type array, %s given",
              who, pname, tphp_val_type_name(*v));
    return (TArr *)v->u.p;
}

static TArr *row_arr(VM *vm, TVal e, const char *who, const char *pname, int pos) {
    if (e.t != T_ARR)
        RAISE(vm, "TypeError",
              "%s: array element %d of argument %s must be of type array, %s given",
              who, pos, pname, tphp_val_type_name(e));
    return (TArr *)e.u.p;
}

static const char *row_req_str(VM *vm, TArr *row, const char *k,
                               const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t != T_STR)
        RAISE(vm, "TypeError", "%s: %s[%d][\"%s\"] must be a string",
              who, pname, pos - 1, k);
    return tstr_val((TStr *)v->u.p);
}

static const char *row_opt_str(VM *vm, TArr *row, const char *k, const char *who,
                               const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return NULL;
    if (v->t != T_STR)
        RAISE(vm, "TypeError", "%s: %s[%d][\"%s\"] must be a string",
              who, pname, pos - 1, k);
    return tstr_val((TStr *)v->u.p);
}

static double row_opt_num(VM *vm, TArr *row, const char *k, double def,
                          const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return def;
    if (v->t != T_INT && v->t != T_FLOAT)
        RAISE(vm, "TypeError", "%s: %s[%d][\"%s\"] must be numeric",
              who, pname, pos - 1, k);
    return tval_to_float(*v);
}

static int64_t row_opt_int(VM *vm, TArr *row, const char *k, int64_t def,
                           const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return def;
    if (v->t != T_INT)
        RAISE(vm, "TypeError", "%s: %s[%d][\"%s\"] must be an int",
              who, pname, pos - 1, k);
    return v->u.i;
}

static bool row_opt_bool(VM *vm, TArr *row, const char *k, bool def,
                         const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return def;
    if (v->t != T_TRUE && v->t != T_FALSE)
        RAISE(vm, "TypeError", "%s: %s[%d][\"%s\"] must be a bool",
              who, pname, pos - 1, k);
    return v->t == T_TRUE;
}

static int count_rows(TArr *rows) {
    int cnt = 0;
    for (uint32_t i = 0; i < rows->dused; i++)
        if (rows->data[i].t != T_UNDEF) cnt++;
    return cnt;
}

/* 行内 [y,m,d] → cui_date（start 键用） */
static bool row_opt_date(VM *vm, TArr *row, const char *k, cui_date *out,
                         const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return false;
    if (v->t != T_ARR)
        RAISE(vm, "TypeError", "%s: %s[%d][\"%s\"] must be a [y,m,d] array",
              who, pname, pos - 1, k);
    TArr *a = (TArr *)v->u.p;
    int64_t vals[3] = { 0, 0, 0 };
    int n = 0;
    for (uint32_t i = 0; i < a->dused && n < 3; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        if (e.t != T_INT)
            RAISE(vm, "TypeError", "%s: %s[%d][\"%s\"][%d] must be an int",
                  who, pname, pos - 1, k, n);
        vals[n++] = e.u.i;
    }
    if (n != 3)
        RAISE(vm, "ValueError", "%s: %s[%d][\"%s\"] must have 3 elements",
              who, pname, pos - 1, k);
    out->year = (int)vals[0];
    out->month = (int)vals[1];
    out->day = (int)vals[2];
    return true;
}

/* 简单关联结果构造 */
static TArr *res_new(void) { return tarr_new(); }

static void res_int(TArr *a, const char *k, int64_t v) {
    tarr_set(a, TV_STR(tstr_new(k, strlen(k))), TV_INT(v));
}
static void res_float(TArr *a, const char *k, double v) {
    tarr_set(a, TV_STR(tstr_new(k, strlen(k))), TV_FLOAT(v));
}
static void res_bool(TArr *a, const char *k, bool v) {
    tarr_set(a, TV_STR(tstr_new(k, strlen(k))), v ? TV_TRUE : TV_FALSE);
}
static void res_str(TArr *a, const char *k, const char *v) {
    tarr_set(a, TV_STR(tstr_new(k, strlen(k))),
             v ? TV_STR(tstr_new(v, strlen(v))) : TV_NULL);
}
static void res_arr(TArr *a, const char *k, TVal v) {
    tarr_set(a, TV_STR(tstr_new(k, strlen(k))), v);
}

/* ---- 代码 / 文本 ---- */

static TVal nm_code_block(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::codeBlock()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *code = tui_arg_str(vm, args, n, 1, who);
    const char *lang = tui_opt_str(vm, args, n, 2, "", who, "lang");
    const char *title = tui_opt_str(vm, args, n, 3, "", who, "title");
    cui_code_opts o;
    memset(&o, 0, sizeof o);
    o.lang = lang[0] ? lang : NULL;
    o.title = title[0] ? title : NULL;
    cui_code_block_ex(tui_need_ui(vm, (TObj *)self, who), id, code, &o);
    return TV_NULL;
}

static TVal nm_json_view(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::jsonView()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *json = tui_arg_str(vm, args, n, 1, who);
    bool action = cui_json_view_ex(tui_need_ui(vm, (TObj *)self, who), id, json, NULL);
    return action ? TV_TRUE : TV_FALSE;
}

/* markdown：返回 ['link'=>, 'task_offset'=>, 'task_line'=>, 'task_checked'=>, 'height'=>] */
static TVal nm_markdown(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::markdown()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *text = tui_arg_str(vm, args, n, 1, who);
    (void)n;   /* v1 不透传 opts：markdown_opts 走 CUI 默认（fit_content/compact 等后续增量） */
    cui_markdown_result r = cui_markdown_ex(tui_need_ui(vm, (TObj *)self, who),
                                            id, text, NULL);
    TArr *a = res_new();
    res_str(a, "link", r.link ? r.link : NULL);   /* 本帧内存：转 PHP 串拷贝 */
    res_int(a, "task_offset", r.task_offset);
    res_int(a, "task_line", r.task_line);
    res_bool(a, "task_checked", r.task_checked);
    res_float(a, "height", r.height);
    return TV_ARR(a);
}

/* codeEditor：buffer 往返 + 位置/编辑结果 →
 * ['text'=>, 'changed'=>, 'line'=>, 'column'=>, 'search'=>, 'dirty'=>] */
static TVal nm_code_editor(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::codeEditor()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *code = tui_arg_str(vm, args, n, 1, who);
    const char *lang = tui_opt_str(vm, args, n, 2, "", who, "lang");
    size_t cap = strlen(code) + 1024;
    char *buf = tm_malloc(cap);
    memcpy(buf, code, strlen(code) + 1);
    cui_code_editor_opts o;
    memset(&o, 0, sizeof o);
    o.lang = lang[0] ? lang : NULL;
    cui_code_editor_result r = cui_code_editor_ex(tui_need_ui(vm, (TObj *)self, who),
                                                  id, buf, cap, &o);
    TArr *a = res_new();
    res_str(a, "text", buf);
    res_bool(a, "changed", r.changed);
    res_bool(a, "refused", r.refused);
    res_bool(a, "focused", r.focused);
    res_int(a, "cursor", r.cursor);
    res_int(a, "anchor", r.anchor);
    res_int(a, "line", r.line);
    res_int(a, "column", r.column);
    res_int(a, "line_count", r.line_count);
    res_int(a, "length", r.length);
    res_int(a, "find_matches", r.find_matches);
    res_int(a, "find_index", r.find_index);
    tm_free(buf);
    return TV_ARR(a);
}

static TVal nm_code_editor_goto(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::codeEditorGoto()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int64_t line = tui_arg_int(vm, args, n, 1, who, "line");
    int64_t col = tui_opt_int(vm, args, n, 2, 1, who, "column");
    cui_code_editor_goto(tui_need_ui(vm, (TObj *)self, who), id, (int)line, (int)col);
    return TV_NULL;
}

static TVal nm_code_editor_select(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::codeEditorSelect()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int64_t anchor = tui_arg_int(vm, args, n, 1, who, "anchor");
    int64_t cursor = tui_arg_int(vm, args, n, 2, who, "cursor");
    cui_code_editor_select(tui_need_ui(vm, (TObj *)self, who), id,
                           (int)anchor, (int)cursor);
    return TV_NULL;
}

static TVal nm_code_editor_find(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::codeEditorFind()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *q = tui_arg_str(vm, args, n, 1, who);
    cui_code_editor_find(tui_need_ui(vm, (TObj *)self, who), id, q);
    return TV_NULL;
}

/* diffView → ['added'=>,'removed'=>,'changes'=>,'current'=>] */
static TVal nm_diff_view(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::diffView()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *old_s = tui_arg_str(vm, args, n, 1, who);
    const char *new_s = tui_arg_str(vm, args, n, 2, who);
    cui_diff_result r = cui_diff_view_ex(tui_need_ui(vm, (TObj *)self, who),
                                         id, old_s, new_s, NULL);
    TArr *a = res_new();
    res_int(a, "added", r.added);
    res_int(a, "removed", r.removed);
    res_int(a, "changes", r.changes);
    res_int(a, "current", r.current);
    return TV_ARR(a);
}

/* hexView：bytes 二进制串往返；选择静态暂存（单实例）→
 * ['start'=>,'length'=>,'cursor'=>,'hovered'=>,'copied'=>,'selected'=>] */
static TVal nm_hex_view(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::hexView()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    if (n <= 1 || args[1].t != T_STR)
        RAISE(vm, "TypeError", "%s: argument bytes must be a string, %s given",
              who, n <= 1 ? "null" : tphp_val_type_name(args[1]));
    TStr *bs = (TStr *)args[1].u.p;
    static cui_hex_selection sel;   /* 单实例：选择跨帧保持（zeroed = 无选择） */
    cui_hex_result r = cui_hex_view_ex(tui_need_ui(vm, (TObj *)self, who), id,
                                       (const uint8_t *)bs->c, bs->len, &sel, NULL);
    TArr *a = res_new();
    res_int(a, "start", (int64_t)sel.start);
    res_int(a, "length", (int64_t)sel.length);
    res_int(a, "cursor", (int64_t)sel.cursor);
    res_bool(a, "selection_changed", r.selection_changed);
    res_bool(a, "edited", r.edited);
    res_int(a, "edit_offset", (int64_t)r.edit_offset);
    res_int(a, "edit_length", (int64_t)r.edit_length);
    res_int(a, "hovered", r.hovered);
    res_bool(a, "copied", r.copied);
    return TV_ARR(a);
}

static TVal nm_hex_view_goto(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::hexViewGoto()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int64_t off = tui_arg_int(vm, args, n, 1, who, "offset");
    if (off < 0)
        RAISE(vm, "ValueError", "%s: offset must be >= 0", who);
    cui_hex_view_goto(tui_need_ui(vm, (TObj *)self, who), id, (size_t)off);
    return TV_NULL;
}

/* ---- 终端 / 日志 / 节点编辑器（per-App 状态，存于 Content 子对象 state 槽）----
 * 所有方法均有 self 参数（域对象），宏 GSTATE 从 self 取 state。 */
#define GSTATE ((TuiContentState *)tui_state_get((TObj *)self, "state", sizeof(TuiContentState)))

/* Content state 析构：释放 terminal/log 缓冲（Init 时 malloc） */
void tui_content_state_free(void *p) {
    TuiContentState *S = (TuiContentState *)p;
    if (!S) return;
    tm_free(S->term_text); tm_free(S->term_lines);
    tm_free(S->log_text);  tm_free(S->log_lines);
}

static TVal nm_terminal_init(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::terminalInit()";
    int64_t text_cap = tui_opt_int(vm, args, n, 0, 65536, who, "textCap");
    int64_t line_cap = tui_opt_int(vm, args, n, 1, 4096, who, "lineCap");
    if (text_cap < 256 || line_cap < 16)
        RAISE(vm, "ValueError", "%s: textCap >= 256 and lineCap >= 16 required", who);
    (void)self;
    tm_free(GSTATE->term_text);
    tm_free(GSTATE->term_lines);
    GSTATE->term_text = tm_malloc((size_t)text_cap);
    GSTATE->term_lines = tm_malloc(sizeof(cui_terminal_line) * (size_t)line_cap);
    memset(&GSTATE->term_t, 0, sizeof GSTATE->term_t);
    cui_terminal_init(&GSTATE->term_t, GSTATE->term_text, (size_t)text_cap,
                      GSTATE->term_lines, (int)line_cap);
    GSTATE->term_ready = true;
    return TV_NULL;
}

static TVal nm_terminal_write(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::terminalWrite()";
    (void)self;
    if (!GSTATE->term_ready)
        RAISE(vm, "LogicException", "%s: call terminalInit() first", who);
    const char *s = tui_arg_str(vm, args, n, 0, who);
    cui_terminal_write(&GSTATE->term_t, s, -1);   /* len<0 → strlen */
    return TV_NULL;
}

static TVal nm_terminal_clear(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::terminalClear()";
    (void)self;
    if (!GSTATE->term_ready)
        RAISE(vm, "LogicException", "%s: call terminalInit() first", who);
    cui_terminal_clear(&GSTATE->term_t);
    return TV_NULL;
}

/* terminal：返回 ['command'=>string|null, 'interrupt'=>bool] */
static TVal nm_terminal(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::terminal()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    if (!GSTATE->term_ready)
        RAISE(vm, "LogicException", "%s: call terminalInit() first", who);
    cui_terminal_result r = cui_terminal_ex(tui_need_ui(vm, (TObj *)self, who),
                                            id, &GSTATE->term_t, NULL);
    TArr *a = res_new();
    res_str(a, "command", r.command ? r.command : NULL);
    res_bool(a, "interrupt", r.interrupt);
    return TV_ARR(a);
}

static TVal nm_log_init(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::logInit()";
    int64_t text_cap = tui_opt_int(vm, args, n, 0, 65536, who, "textCap");
    int64_t line_cap = tui_opt_int(vm, args, n, 1, 2048, who, "lineCap");
    if (text_cap < 256 || line_cap < 16)
        RAISE(vm, "ValueError", "%s: textCap >= 256 and lineCap >= 16 required", who);
    (void)self;
    tm_free(GSTATE->log_text);
    tm_free(GSTATE->log_lines);
    GSTATE->log_text = tm_malloc((size_t)text_cap);
    GSTATE->log_lines = tm_malloc(sizeof(cui_log_line) * (size_t)line_cap);
    memset(&GSTATE->log_t, 0, sizeof GSTATE->log_t);
    cui_log_init(&GSTATE->log_t, GSTATE->log_lines, (int)line_cap,
                 GSTATE->log_text, (size_t)text_cap);
    GSTATE->log_ready = true;
    return TV_NULL;
}

static TVal nm_log_append(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::logAppend()";
    (void)self;
    if (!GSTATE->log_ready)
        RAISE(vm, "LogicException", "%s: call logInit() first", who);
    int64_t level = tui_arg_int(vm, args, n, 0, who, "level");
    const char *text = tui_arg_str(vm, args, n, 1, who);
    if (level < 0 || level >= CUI_LOG_LEVEL_COUNT)
        RAISE(vm, "ValueError", "%s: level must be in 0..%d", who,
              (int)CUI_LOG_LEVEL_COUNT - 1);
    cui_log_append(&GSTATE->log_t, (cui_log_level)level, "%s", text);
    return TV_NULL;
}

static TVal nm_log_clear(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::logClear()";
    (void)self;
    if (!GSTATE->log_ready)
        RAISE(vm, "LogicException", "%s: call logInit() first", who);
    cui_log_clear(&GSTATE->log_t);
    return TV_NULL;
}

/* logView：筛选/跟随由组件内部工具条管理（CUI 自持），此处只渲染 → null */
static TVal nm_log_view(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::logView()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    if (!GSTATE->log_ready)
        RAISE(vm, "LogicException", "%s: call logInit() first", who);
    bool timestamps = tui_opt_bool(vm, args, n, 1, false, who, "timestamps");
    cui_log_view_opts o;
    memset(&o, 0, sizeof o);
    o.timestamps = timestamps;
    cui_log_view_ex(tui_need_ui(vm, (TObj *)self, who), id, &GSTATE->log_t, &o);
    return TV_NULL;
}

/* ---- 命令面板 / 向导 / 甘特 ---- */

/* commands=[['title'=>,'icon'=>?,'group'=>?,'shortcut'=>?]] */
static TVal nm_command_palette(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::commandPalette()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    bool open = tui_arg_bool(vm, args, n, 1, who, "open");
    TArr *rows = rows_of(vm, &args[2], who, "commands");
    int cnt = count_rows(rows);
    cui_command *cmds = cnt ? tm_malloc(sizeof(cui_command) * (size_t)cnt) : NULL;
    char **strs = NULL;
    int nstr = 0;
    /* 第一趟：校验 */
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "commands", pos + 1);
        row_req_str(vm, row, "title", who, "commands", pos + 1);
        row_opt_str(vm, row, "icon", who, "commands", pos + 1);
        row_opt_str(vm, row, "group", who, "commands", pos + 1);
        row_opt_str(vm, row, "shortcut", who, "commands", pos + 1);
        pos++;
    }
    /* 第二趟：填充（全部拷贝进 strs 池，调用后统一释放） */
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        const char *keys[] = { "title", "icon", "group", "shortcut" };
        char *got[4];
        for (int z = 0; z < 4; z++) {
            const char *s = z == 0
                ? row_req_str(vm, row, keys[z], who, "commands", k + 1)
                : row_opt_str(vm, row, keys[z], who, "commands", k + 1);
            got[z] = dup_s(s ? s : "");
            strs = tm_realloc(strs, sizeof(char *) * (size_t)(nstr + 1));
            strs[nstr++] = got[z];
        }
        cmds[k].title = got[0];
        cmds[k].icon = got[1][0] ? got[1] : NULL;
        cmds[k].group = got[2][0] ? got[2] : NULL;
        cmds[k].shortcut = got[3][0] ? got[3] : NULL;
        k++;
    }
    int picked = cui_command_palette(tui_need_ui(vm, (TObj *)self, who),
                                     &open, cmds, cnt);
    for (int z = 0; z < nstr; z++) tm_free(strs[z]);
    tm_free(strs);
    tm_free(cmds);
    TArr *a = res_new();
    res_int(a, "picked", picked);
    res_bool(a, "open", open);
    return TV_ARR(a);
}

/* tour steps=[['title'=>,'body'=>,'placement'=>0,'target'=>0]]；
 * 返回 ['event'=>,'step'=>]（step 进出） */
static TVal nm_tour(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::tour()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "steps");
    int step = (int)tui_arg_int(vm, args, n, 2, who, "step");
    int cnt = count_rows(rows);
    cui_tour_step *steps = cnt ? tm_malloc(sizeof(cui_tour_step) * (size_t)cnt) : NULL;
    char **strs = NULL;
    int nstr = 0;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "steps", pos + 1);
        row_req_str(vm, row, "title", who, "steps", pos + 1);
        row_req_str(vm, row, "body", who, "steps", pos + 1);
        row_opt_int(vm, row, "placement", 0, who, "steps", pos + 1);
        row_opt_int(vm, row, "target", 0, who, "steps", pos + 1);
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        const char *keys[] = { "title", "body" };
        char *got[2];
        for (int z = 0; z < 2; z++) {
            got[z] = dup_s(row_req_str(vm, row, keys[z], who, "steps", k + 1));
            strs = tm_realloc(strs, sizeof(char *) * (size_t)(nstr + 1));
            strs[nstr++] = got[z];
        }
        steps[k].title = got[0];
        steps[k].body = got[1];
        steps[k].placement = (int)row_opt_int(vm, row, "placement", 0, who, "steps", k + 1);
        steps[k].target = (cui_id)row_opt_int(vm, row, "target", 0, who, "steps", k + 1);
        k++;
    }
    int event = cui_tour_ex(tui_need_ui(vm, (TObj *)self, who), id, steps, cnt,
                            &step, NULL);
    for (int z = 0; z < nstr; z++) tm_free(strs[z]);
    tm_free(strs);
    tm_free(steps);
    TArr *a = res_new();
    res_int(a, "event", event);
    res_int(a, "step", step);
    return TV_ARR(a);
}

/* gantt：tasks=[['name'=>,'owner'=>?,'start'=>0,'end'=>0,'progress'=>0,'tone'=>0,
 *                'depends_on'=>0,'milestone'=>false]]；origin=[y,m,d] */
static TVal nm_gantt(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::gantt()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "tasks");
    cui_date origin;
    if (args[2].t != T_ARR)
        RAISE(vm, "TypeError", "%s: argument origin must be a [y,m,d] array", who);
    {
        TArr *oa = (TArr *)args[2].u.p;
        int64_t ov[3] = { 0, 0, 0 };
        int on = 0;
        for (uint32_t i = 0; i < oa->dused && on < 3; i++) {
            TVal e = oa->data[i];
            if (e.t == T_UNDEF) continue;
            if (e.t != T_INT)
                RAISE(vm, "TypeError", "%s: origin[%d] must be an int", who, on);
            ov[on++] = e.u.i;
        }
        if (on != 3)
            RAISE(vm, "ValueError", "%s: origin must have 3 elements [y,m,d]", who);
        origin.year = (int)ov[0];
        origin.month = (int)ov[1];
        origin.day = (int)ov[2];
    }
    int64_t days = tui_arg_int(vm, args, n, 3, who, "days");
    if (days < 1 || days > 400)
        RAISE(vm, "ValueError", "%s: days must be in 1..400", who);
    int cnt = count_rows(rows);
    cui_gantt_task *tasks = cnt ? tm_malloc(sizeof(cui_gantt_task) * (size_t)cnt) : NULL;
    char **strs = NULL;
    int nstr = 0;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "tasks", pos + 1);
        row_req_str(vm, row, "name", who, "tasks", pos + 1);
        row_opt_str(vm, row, "owner", who, "tasks", pos + 1);
        row_opt_int(vm, row, "start", 0, who, "tasks", pos + 1);
        row_opt_int(vm, row, "end", 0, who, "tasks", pos + 1);
        row_opt_num(vm, row, "progress", 0, who, "tasks", pos + 1);
        row_opt_int(vm, row, "tone", 0, who, "tasks", pos + 1);
        row_opt_int(vm, row, "depends_on", 0, who, "tasks", pos + 1);
        row_opt_bool(vm, row, "milestone", false, who, "tasks", pos + 1);
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        const char *nm = row_req_str(vm, row, "name", who, "tasks", k + 1);
        const char *ow = row_opt_str(vm, row, "owner", who, "tasks", k + 1);
        char *cn = dup_s(nm);
        strs = tm_realloc(strs, sizeof(char *) * (size_t)(nstr + 1));
        strs[nstr++] = cn;
        char *co = dup_s(ow ? ow : "");
        strs = tm_realloc(strs, sizeof(char *) * (size_t)(nstr + 1));
        strs[nstr++] = co;
        tasks[k].name = cn;
        tasks[k].owner = co[0] ? co : NULL;
        tasks[k].start = (int)row_opt_int(vm, row, "start", 0, who, "tasks", k + 1);
        tasks[k].end = (int)row_opt_int(vm, row, "end", 0, who, "tasks", k + 1);
        tasks[k].progress = (float)row_opt_num(vm, row, "progress", 0, who, "tasks", k + 1);
        tasks[k].tone = (int)row_opt_int(vm, row, "tone", 0, who, "tasks", k + 1);
        tasks[k].depends_on = (int)row_opt_int(vm, row, "depends_on", 0, who, "tasks", k + 1);
        tasks[k].milestone = row_opt_bool(vm, row, "milestone", false, who, "tasks", k + 1);
        k++;
    }
    int selected = cui_gantt_ex(tui_need_ui(vm, (TObj *)self, who), id, tasks, cnt,
                                origin, (int)days, NULL);
    for (int z = 0; z < nstr; z++) tm_free(strs[z]);
    tm_free(strs);
    tm_free(tasks);
    return TV_INT(selected);
}

/* ---- 文件 ---- */

static TVal nm_file_browser(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::fileBrowser()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *path_in = tui_arg_str(vm, args, n, 1, who);
    char path[1024];
    snprintf(path, sizeof path, "%s", path_in);
    cui_file_browser_result r = cui_file_browser_ex(tui_need_ui(vm, (TObj *)self, who),
                                                    id, path, sizeof path, NULL);
    TArr *a = res_new();
    res_str(a, "path", path);
    res_str(a, "picked", r.picked ? r.picked : NULL);
    res_str(a, "selected", r.selected ? r.selected : NULL);
    res_bool(a, "navigated", r.navigated);
    res_bool(a, "error", r.error);
    return TV_ARR(a);
}

/* filePicker → ['open'=>, 'path'=>]（open 进出、path 进出） */
static TVal nm_file_picker(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::filePicker()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    bool open = tui_arg_bool(vm, args, n, 1, who, "open");
    const char *path_in = tui_opt_str(vm, args, n, 2, "", who, "path");
    char path[1024];
    snprintf(path, sizeof path, "%s", path_in);
    cui_file_picker_ex(tui_need_ui(vm, (TObj *)self, who), id, &open, path,
                       sizeof path, NULL);
    TArr *a = res_new();
    res_bool(a, "open", open);
    res_str(a, "path", path);
    return TV_ARR(a);
}

/* ---- 图片纹理（句柄为 int） ---- */

static TVal nm_image_load(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::imageLoad()";
    const char *path = tui_arg_str(vm, args, n, 0, who);
    int w = 0, h = 0;
    uint32_t tex = cui_image_load(tui_need_ui(vm, (TObj *)self, who), path, &w, &h);
    TArr *a = res_new();
    res_int(a, "tex", (int64_t)tex);
    res_int(a, "w", w);
    res_int(a, "h", h);
    return TV_ARR(a);
}

static TVal nm_image_create(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::imageCreate()";
    int64_t w = tui_arg_int(vm, args, n, 0, who, "width");
    int64_t h = tui_arg_int(vm, args, n, 1, who, "height");
    if (n <= 2 || args[2].t != T_STR)
        RAISE(vm, "TypeError", "%s: argument rgba must be a binary string", who);
    TStr *rgba = (TStr *)args[2].u.p;
    if (w < 1 || h < 1 || (int64_t)rgba->len < w * h * 4)
        RAISE(vm, "ValueError", "%s: rgba must be width*height*4 bytes", who);
    uint32_t tex = cui_image_create(tui_need_ui(vm, (TObj *)self, who),
                                    (int)w, (int)h, (const uint8_t *)rgba->c);
    return TV_INT((int64_t)tex);
}

static TVal nm_image_destroy(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::imageDestroy()";
    int64_t tex = tui_arg_int(vm, args, n, 0, who, "tex");
    cui_image_destroy(tui_need_ui(vm, (TObj *)self, who), (uint32_t)tex);
    return TV_NULL;
}

static TVal nm_image(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::image()";
    int64_t tex = tui_arg_int(vm, args, n, 0, who, "tex");
    cui_image_ex(tui_need_ui(vm, (TObj *)self, who), (uint32_t)tex, NULL);
    return TV_NULL;
}

/* imageView：视图状态静态单例（zoom/cx/cy 跨帧）→ ['zoom'=>,'cx'=>,'cy'=>] */
static TVal nm_image_viewer(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::imageViewer()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int64_t tex = tui_arg_int(vm, args, n, 1, who, "tex");
    int64_t w = tui_arg_int(vm, args, n, 2, who, "width");
    int64_t h = tui_arg_int(vm, args, n, 3, who, "height");
    static cui_image_view view;   /* zeroed = fit */
    cui_image_viewer_ex(tui_need_ui(vm, (TObj *)self, who), id, (uint32_t)tex,
                        (int)w, (int)h, &view, NULL);
    TArr *a = res_new();
    res_float(a, "zoom", view.zoom);
    res_float(a, "cx", view.cx);
    res_float(a, "cy", view.cy);
    return TV_ARR(a);
}

/* cropper → ['x'=>,'y'=>,'w'=>,'h'=>]（裁剪矩形跨帧静态） */
static TVal nm_cropper(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::cropper()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int64_t tex = tui_arg_int(vm, args, n, 1, who, "tex");
    int64_t w = tui_arg_int(vm, args, n, 2, who, "width");
    int64_t h = tui_arg_int(vm, args, n, 3, who, "height");
    static cui_rect crop;   /* zeroed = 整图 */
    cui_cropper_ex(tui_need_ui(vm, (TObj *)self, who), id, (uint32_t)tex,
                   (int)w, (int)h, &crop, NULL);
    TArr *a = res_new();
    res_float(a, "x", crop.x);
    res_float(a, "y", crop.y);
    res_float(a, "w", crop.w);
    res_float(a, "h", crop.h);
    return TV_ARR(a);
}

/* ---- emoji ---- */

static TVal nm_emoji(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::emoji()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    double size = tui_opt_float(vm, args, n, 1, 0, who, "size");
    cui_emoji(tui_need_ui(vm, (TObj *)self, who), s, (float)size);
    return TV_NULL;
}

static TVal nm_emoji_picker(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::emojiPicker()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *picked = cui_emoji_picker_ex(tui_need_ui(vm, (TObj *)self, who),
                                             id, NULL);
    TArr *a = res_new();
    res_str(a, "picked", picked ? picked : NULL);
    return TV_ARR(a);
}

static TVal nm_emoji_supported(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::emojiSupported()";
    const char *s = tui_arg_str(vm, args, n, 0, who);
    return cui_emoji_supported(tui_need_ui(vm, (TObj *)self, who), s, (int)strlen(s))
        ? TV_TRUE : TV_FALSE;
}

static TVal nm_emoji_font_load_file(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::emojiFontLoadFile()";
    const char *path = tui_arg_str(vm, args, n, 0, who);
    return cui_emoji_font_load_file(tui_need_ui(vm, (TObj *)self, who), path)
        ? TV_TRUE : TV_FALSE;
}

static TVal nm_emoji_font_load_system(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\Content::emojiFontLoadSystem()";
    return cui_emoji_font_load_system(tui_need_ui(vm, (TObj *)self, who))
        ? TV_TRUE : TV_FALSE;
}

/* ---- 社交 / 评论 ---- */

static TVal nm_chat_message(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::chatMessage()";
    const char *text = tui_arg_str(vm, args, n, 0, who);
    bool outgoing = tui_opt_bool(vm, args, n, 1, false, who, "outgoing");
    const char *author = tui_opt_str(vm, args, n, 2, "", who, "author");
    bool markdown = tui_opt_bool(vm, args, n, 3, false, who, "markdown");
    cui_chat_opts o;
    memset(&o, 0, sizeof o);
    o.mine = outgoing;
    o.author = author[0] ? author : NULL;
    o.markdown = markdown;
    cui_chat_message_ex(tui_need_ui(vm, (TObj *)self, who), text, &o);
    return TV_NULL;
}

static TVal nm_chat_divider(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::chatDivider()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    cui_chat_divider(tui_need_ui(vm, (TObj *)self, who), label);
    return TV_NULL;
}

static TVal nm_typing_indicator(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::typingIndicator()";
    const char *name = tui_arg_str(vm, args, n, 0, who);
    cui_typing_indicator(tui_need_ui(vm, (TObj *)self, who), name);
    return TV_NULL;
}

static TVal nm_comment(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::comment()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *author = tui_arg_str(vm, args, n, 1, who);
    const char *text = tui_arg_str(vm, args, n, 2, who);
    return TV_INT(cui_comment_ex(tui_need_ui(vm, (TObj *)self, who),
                                 id, author, text, NULL));
}

/* presence：users=[['name'=>,'color'=>0xRRGGBBAA?,'idle'=>false,'tex'=>0]] */
static TVal nm_presence(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::presence()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "users");
    int64_t max_vis = tui_opt_int(vm, args, n, 2, 3, who, "maxVisible");
    double size = tui_opt_float(vm, args, n, 3, 32, who, "size");
    int cnt = count_rows(rows);
    cui_presence_user *users = cnt ? tm_malloc(sizeof(cui_presence_user) * (size_t)cnt) : NULL;
    char **strs = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt) : NULL;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "users", pos + 1);
        row_req_str(vm, row, "name", who, "users", pos + 1);
        row_opt_bool(vm, row, "idle", false, who, "users", pos + 1);
        row_opt_int(vm, row, "tex", 0, who, "users", pos + 1);
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        strs[k] = dup_s(row_req_str(vm, row, "name", who, "users", k + 1));
        users[k].name = strs[k];
        users[k].avatar = (cui_texture)(uint32_t)
            row_opt_int(vm, row, "tex", 0, who, "users", k + 1);
        users[k].idle = row_opt_bool(vm, row, "idle", false, who, "users", k + 1);
        users[k].color = (cui_color){ 0, 0, 0, 0 };   /* 0 → CUI 自动配色 */
        k++;
    }
    cui_presence(tui_need_ui(vm, (TObj *)self, who), id, users, cnt,
                 (int)max_vis, (float)size);
    for (int i = 0; i < cnt; i++) tm_free(strs[i]);
    tm_free(strs);
    tm_free(users);
    return TV_NULL;
}

/* ---- 签名板 ---- */

static TVal nm_signature_pad(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::signaturePad()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    return cui_signature_pad_ex(tui_need_ui(vm, (TObj *)self, who), id, NULL)
        ? TV_TRUE : TV_FALSE;
}

/* signaturePoints → ['points'=>[[x,y],...], 'strokes'=>[n,...]]（调用内拷出） */
static TVal nm_signature_points(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::signaturePoints()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const cui_vec2 *pts = NULL;
    const int *ends = NULL;
    int strokes = 0;
    int cnt = cui_signature_points(tui_need_ui(vm, (TObj *)self, who), id,
                                   &pts, &ends, &strokes);
    TArr *pa = tarr_new();
    for (int i = 0; i < cnt; i++) {
        TArr *p = tarr_new();
        tarr_append(p, TV_FLOAT(pts[i].x));
        tarr_append(p, TV_FLOAT(pts[i].y));
        tarr_append(pa, TV_ARR(p));
    }
    TArr *sa = tarr_new();
    for (int i = 0; i < strokes; i++) tarr_append(sa, TV_INT(ends[i]));
    TArr *a = res_new();
    res_arr(a, "points", TV_ARR(pa));
    res_arr(a, "strokes", TV_ARR(sa));
    return TV_ARR(a);
}

static TVal nm_signature_clear(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::signatureClear()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_signature_clear(tui_need_ui(vm, (TObj *)self, who), id);
    return TV_NULL;
}

/* ---- 节点编辑器 ---- */

/* nodeEditorBegin → false（未开）或 ['x'=>,'y'=>,'zoom'=>]（view 跨帧）
 * begin 返回 false 时 CUI 未开编辑器，End 必须跳过（SCOPE_IF 语义）。
 * nodeed_on 存于 Content state（per-App）。 */

/* SCOPE_IF 失配检测（阶段 A）：nodeEditor 是本域唯一的 SCOPE_IF 对 */
const char *tui_content_scopeif_unclosed(TObj *content) {
    TuiContentState *S = (TuiContentState *)tui_state_get(content, "state", sizeof(TuiContentState));
    return (S && S->nodeed_on) ? "nodeEditorBegin/nodeEditorEnd" : NULL;
}

void tui_content_scopeif_clear(TObj *content) {
    TuiContentState *S = (TuiContentState *)tui_state_get(content, "state", sizeof(TuiContentState));
    if (S) S->nodeed_on = false;
}

static TVal nm_node_editor_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::nodeEditorBegin()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    static cui_node_view view;   /* zeroed = origin/1x */
    GSTATE->nodeed_on = cui_node_editor_begin_ex(tui_need_ui(vm, (TObj *)self, who),
                                           id, &view, NULL);
    if (!GSTATE->nodeed_on) return TV_FALSE;
    TArr *a = res_new();
    res_float(a, "x", view.x);
    res_float(a, "y", view.y);
    res_float(a, "zoom", view.zoom);
    return TV_ARR(a);
}

static TVal nm_node_editor_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    if (GSTATE->nodeed_on) {
        cui_node_editor_end(tui_need_ui(vm, (TObj *)self,
                                        "Tphp\\Tui\\Content::nodeEditorEnd()"));
        GSTATE->nodeed_on = false;
    }
    return TV_NULL;
}

/* nodeBegin → false 或 ['x'=>,'y'=>,'selected'=>]（x/y 进出） */
static TVal nm_node_begin(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::nodeBegin()";
    int64_t node_id = tui_arg_int(vm, args, n, 0, who, "nodeId");
    float x = (float)tui_arg_float(vm, args, n, 1, who, "x");
    float y = (float)tui_arg_float(vm, args, n, 2, who, "y");
    const char *title = tui_arg_str(vm, args, n, 3, who);
    bool sel = cui_node_begin_ex(tui_need_ui(vm, (TObj *)self, who),
                                 (int)node_id, &x, &y, title, NULL);
    TArr *a = res_new();
    res_float(a, "x", x);
    res_float(a, "y", y);
    res_bool(a, "selected", sel);
    return TV_ARR(a);
}

static TVal nm_node_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_node_end(tui_need_ui(vm, (TObj *)self, "Tphp\\Tui\\Content::nodeEnd()"));
    return TV_NULL;
}

static TVal nm_node_input(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::nodeInput()";
    int64_t pin = tui_arg_int(vm, args, n, 0, who, "pinId");
    const char *label = tui_arg_str(vm, args, n, 1, who);
    int64_t type = tui_opt_int(vm, args, n, 2, 0, who, "type");
    cui_pin_opts o;
    memset(&o, 0, sizeof o);
    o.type = (int)type;
    bool linked = cui_node_input_ex(tui_need_ui(vm, (TObj *)self, who),
                                    (int)pin, label, &o);
    return linked ? TV_TRUE : TV_FALSE;
}

static TVal nm_node_output(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::nodeOutput()";
    int64_t pin = tui_arg_int(vm, args, n, 0, who, "pinId");
    const char *label = tui_arg_str(vm, args, n, 1, who);
    int64_t type = tui_opt_int(vm, args, n, 2, 0, who, "type");
    cui_pin_opts o;
    memset(&o, 0, sizeof o);
    o.type = (int)type;
    bool linked = cui_node_output_ex(tui_need_ui(vm, (TObj *)self, who),
                                     (int)pin, label, &o);
    return linked ? TV_TRUE : TV_FALSE;
}

static TVal nm_node_content_begin(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_node_content_begin(tui_need_ui(vm, (TObj *)self,
                                       "Tphp\\Tui\\Content::nodeContentBegin()"));
    return TV_NULL;
}

static TVal nm_node_content_end(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    cui_node_content_end(tui_need_ui(vm, (TObj *)self,
                                     "Tphp\\Tui\\Content::nodeContentEnd()"));
    return TV_NULL;
}

static TVal nm_node_link(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::nodeLink()";
    int64_t link = tui_arg_int(vm, args, n, 0, who, "linkId");
    int64_t from = tui_arg_int(vm, args, n, 1, who, "fromPin");
    int64_t to = tui_arg_int(vm, args, n, 2, who, "toPin");
    return cui_node_link(tui_need_ui(vm, (TObj *)self, who),
                         (int)link, (int)from, (int)to) ? TV_TRUE : TV_FALSE;
}

/* nodeLinkCreated → null 或 ['from'=>,'to'=>]（循环取到空为止） */
static TVal nm_node_link_created(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\Content::nodeLinkCreated()";
    int from = 0, to = 0;
    if (!cui_node_link_created(tui_need_ui(vm, (TObj *)self, who), &from, &to))
        return TV_NULL;
    TArr *a = res_new();
    res_int(a, "from", from);
    res_int(a, "to", to);
    return TV_ARR(a);
}

static TVal nm_node_link_removed(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\Content::nodeLinkRemoved()";
    int id = 0;
    if (!cui_node_link_removed(tui_need_ui(vm, (TObj *)self, who), &id))
        return TV_NULL;
    return TV_INT(id);
}

static TVal nm_node_deleted(VM *vm, void *self, TVal *args, int n) {
    (void)args;
    (void)n;
    const char *who = "Tphp\\Tui\\Content::nodeDeleted()";
    int id = 0;
    if (!cui_node_deleted(tui_need_ui(vm, (TObj *)self, who), &id))
        return TV_NULL;
    return TV_INT(id);
}

/* ---- devtools / 仪表族（Task 6 后半段） ---- */

/* testExplorer → ['selected'=>,'activated'=>,'run'=>,'run_all'=>,'passed'=>,
 *                 'failed'=>,'skipped'=>,'total'=>]
 * items=[['name'=>,'depth'=>0,'status'=>0,'duration'=>0.0,'message'=>?]] */
static TVal nm_test_explorer(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::testExplorer()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "items");
    int cnt = count_rows(rows);
    cui_test_item *items = cnt ? tm_malloc(sizeof(cui_test_item) * (size_t)cnt) : NULL;
    char **strs = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt * 2) : NULL;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "items", pos + 1);
        row_req_str(vm, row, "name", who, "items", pos + 1);
        row_opt_int(vm, row, "depth", 0, who, "items", pos + 1);
        row_opt_int(vm, row, "status", 0, who, "items", pos + 1);
        row_opt_num(vm, row, "duration", 0, who, "items", pos + 1);
        row_opt_str(vm, row, "message", who, "items", pos + 1);
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        const char *nm = row_req_str(vm, row, "name", who, "items", k + 1);
        const char *msg = row_opt_str(vm, row, "message", who, "items", k + 1);
        strs[k * 2] = dup_s(nm);
        strs[k * 2 + 1] = msg ? dup_s(msg) : NULL;
        int64_t st = row_opt_int(vm, row, "status", 0, who, "items", k + 1);
        if (st < 0 || st > 5) st = 0;
        items[k].name = strs[k * 2];
        items[k].depth = (int)row_opt_int(vm, row, "depth", 0, who, "items", k + 1);
        items[k].status = (cui_job_status)st;
        items[k].duration =
            (float)row_opt_num(vm, row, "duration", 0, who, "items", k + 1);
        items[k].message = strs[k * 2 + 1];
        k++;
    }
    cui_test_explorer_result r =
        cui_test_explorer_ex(tui_need_ui(vm, (TObj *)self, who), id, items, cnt, NULL);
    for (int i = 0; i < cnt; i++) {
        tm_free(strs[i * 2]);
        tm_free(strs[i * 2 + 1]);
    }
    tm_free(strs);
    tm_free(items);
    TArr *a = res_new();
    res_int(a, "selected", r.selected);
    res_int(a, "activated", r.activated);
    res_int(a, "run", r.run);
    res_bool(a, "run_all", r.run_all);
    res_int(a, "passed", r.passed);
    res_int(a, "failed", r.failed);
    res_int(a, "skipped", r.skipped);
    res_int(a, "total", r.total);
    return TV_ARR(a);
}

/* stackTrace → ['clicked'=>]；frames=[['function'=>,'file'=>,'line'=>0,
 *   'column'=>0,'external'=>false,'source'=>[...]?,'source_first'=>1]] */
static TVal nm_stack_trace(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::stackTrace()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *title = tui_arg_str(vm, args, n, 1, who);
    const char *message = tui_arg_str(vm, args, n, 2, who);
    TArr *rows = rows_of(vm, &args[3], who, "frames");
    const char *lang = tui_opt_str(vm, args, n, 4, "", who, "lang");
    int cnt = count_rows(rows);
    cui_stack_frame *frames = cnt
        ? tm_malloc(sizeof(cui_stack_frame) * (size_t)cnt) : NULL;
    char **strs = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt * 3) : NULL;
    char ***srcs = cnt ? tm_malloc(sizeof(char **) * (size_t)cnt) : NULL;
    int *srcn = cnt ? tm_malloc(sizeof(int) * (size_t)cnt) : NULL;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "frames", pos + 1);
        row_opt_str(vm, row, "function", who, "frames", pos + 1);
        row_opt_str(vm, row, "file", who, "frames", pos + 1);
        row_opt_int(vm, row, "line", 0, who, "frames", pos + 1);
        row_opt_int(vm, row, "column", 0, who, "frames", pos + 1);
        row_opt_bool(vm, row, "external", false, who, "frames", pos + 1);
        row_opt_int(vm, row, "source_first", 1, who, "frames", pos + 1);
        TVal *sv = row_get(row, "source");
        if (sv) {
            int sc = 0;
            char **ss = tui_strs(vm, sv, who, "frames.source", &sc);
            tui_strs_free(ss, sc);
        }
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        const char *fn = row_opt_str(vm, row, "function", who, "frames", k + 1);
        const char *file = row_opt_str(vm, row, "file", who, "frames", k + 1);
        strs[k * 3] = fn ? dup_s(fn) : NULL;
        strs[k * 3 + 1] = file ? dup_s(file) : NULL;
        strs[k * 3 + 2] = NULL;
        srcs[k] = NULL;
        srcn[k] = 0;
        TVal *sv = row_get(row, "source");
        if (sv && sv->t == T_ARR) {
            srcs[k] = tui_strs(vm, sv, who, "frames.source", &srcn[k]);
            strs[k * 3 + 2] = NULL;   /* tui_strs 已拷贝，独立释放 */
        }
        frames[k].function = strs[k * 3];
        frames[k].file = strs[k * 3 + 1];
        frames[k].line = (int)row_opt_int(vm, row, "line", 0, who, "frames", k + 1);
        frames[k].column = (int)row_opt_int(vm, row, "column", 0, who, "frames", k + 1);
        frames[k].external = row_opt_bool(vm, row, "external", false, who, "frames", k + 1);
        frames[k].source = (const char *const *)srcs[k];
        frames[k].source_count = srcn[k];
        frames[k].source_first =
            (int)row_opt_int(vm, row, "source_first", 1, who, "frames", k + 1);
        k++;
    }
    cui_stack_trace_opts o;
    memset(&o, 0, sizeof o);
    o.title = title;
    o.message = message;
    o.lang = lang[0] ? lang : NULL;
    cui_stack_trace_result r =
        cui_stack_trace_ex(tui_need_ui(vm, (TObj *)self, who), id, frames, cnt, &o);
    for (int i = 0; i < cnt; i++) {
        tm_free(strs[i * 3]);
        tm_free(strs[i * 3 + 1]);
        if (srcs[i]) tui_strs_free(srcs[i], srcn[i]);
    }
    tm_free(strs);
    tm_free(srcs);
    tm_free(srcn);
    tm_free(frames);
    TArr *a = res_new();
    res_int(a, "clicked", r.clicked);
    return TV_ARR(a);
}

/* requestBar → ['send'=>,'cancel'=>,'changed'=>,'method'=>,'url'=>]
 * url 为 buffer 往返（值由 PHP 持有） */
static TVal nm_request_bar(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::requestBar()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *url_in = tui_arg_str(vm, args, n, 1, who);
    int method = (int)tui_opt_int(vm, args, n, 2, 0, who, "method");
    bool loading = tui_opt_bool(vm, args, n, 3, false, who, "loading");
    const char *ph = tui_opt_str(vm, args, n, 4, "", who, "placeholder");
    if (method < 0 || method > 6)
        RAISE(vm, "ValueError", "%s: method must be in 0..6", who);
    if (strlen(url_in) + 1 > 2048)
        RAISE(vm, "ValueError", "%s: url is longer than 2047 bytes", who);
    char url[2048];
    snprintf(url, sizeof url, "%s", url_in);
    cui_request_bar_opts o;
    memset(&o, 0, sizeof o);
    o.loading = loading;
    o.placeholder = ph[0] ? ph : NULL;
    cui_request_bar_result r =
        cui_request_bar_ex(tui_need_ui(vm, (TObj *)self, who), id, &method,
                           url, sizeof url, &o);
    TArr *a = res_new();
    res_bool(a, "send", r.send);
    res_bool(a, "cancel", r.cancel);
    res_bool(a, "changed", r.changed);
    res_int(a, "method", method);
    res_str(a, "url", url);
    return TV_ARR(a);
}

static TVal nm_response_status(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::responseStatus()";
    int64_t status = tui_arg_int(vm, args, n, 0, who, "status");
    double seconds = tui_arg_float(vm, args, n, 1, who, "seconds");
    double bytes = tui_arg_float(vm, args, n, 2, who, "bytes");
    cui_response_status(tui_need_ui(vm, (TObj *)self, who), (int)status,
                        seconds, bytes);
    return TV_NULL;
}

/* uptime → ['service'=>,'day'=>]；services=[['name'=>,'days'=>float[]]] */
static TVal nm_uptime(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::uptime()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "services");
    int cnt = count_rows(rows);
    cui_uptime_service *svcs = cnt
        ? tm_malloc(sizeof(cui_uptime_service) * (size_t)cnt) : NULL;
    char **names = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt) : NULL;
    float **days = cnt ? tm_malloc(sizeof(float *) * (size_t)cnt) : NULL;
    int *dayn = cnt ? tm_malloc(sizeof(int) * (size_t)cnt) : NULL;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "services", pos + 1);
        row_req_str(vm, row, "name", who, "services", pos + 1);
        TVal *dv = row_get(row, "days");
        if (!dv || dv->t != T_ARR)
            RAISE(vm, "TypeError", "%s: services[%d][\"days\"] must be an array",
                  who, pos);
        int dn = 0;
        double *d = tui_floats(vm, dv, who, "services.days", &dn);
        tui_arr_free(d);
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        names[k] = dup_s(row_req_str(vm, row, "name", who, "services", k + 1));
        double *d = NULL;
        int dn = 0;
        TVal *dv = row_get(row, "days");
        d = tui_floats(vm, dv, who, "services.days", &dn);
        days[k] = tm_malloc(sizeof(float) * (size_t)(dn ? dn : 1));
        for (int z = 0; z < dn; z++) days[k][z] = (float)d[z];
        dayn[k] = dn;
        tui_arr_free(d);
        svcs[k].name = names[k];
        svcs[k].days = days[k];
        svcs[k].day_count = dn;
        k++;
    }
    cui_uptime_result r = cui_uptime_ex(tui_need_ui(vm, (TObj *)self, who),
                                        id, svcs, cnt, NULL);
    for (int i = 0; i < cnt; i++) { tm_free(names[i]); tm_free(days[i]); }
    tm_free(names);
    tm_free(days);
    tm_free(dayn);
    tm_free(svcs);
    TArr *a = res_new();
    res_int(a, "service", r.service);
    res_int(a, "day", r.day);
    return TV_ARR(a);
}

/* timeHistogram → ['selection_changed'=>,'hovered'=>,'sel_start'=>,'sel_end'=>]
 * series=[['name'=>,'counts'=>float[],'tone'=>0]]；selStart/selEnd 进出 */
static TVal nm_time_histogram(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::timeHistogram()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    double start = tui_arg_float(vm, args, n, 1, who, "start");
    double bucket_s = tui_arg_float(vm, args, n, 2, who, "bucketSeconds");
    int64_t buckets = tui_arg_int(vm, args, n, 3, who, "buckets");
    TArr *rows = rows_of(vm, &args[4], who, "series");
    double sel_start = tui_opt_float(vm, args, n, 5, 0, who, "selStart");
    double sel_end = tui_opt_float(vm, args, n, 6, 0, who, "selEnd");
    if (buckets < 1 || buckets > 100000)
        RAISE(vm, "ValueError", "%s: buckets must be in 1..100000", who);
    int cnt = count_rows(rows);
    cui_histogram_series *ss = cnt
        ? tm_malloc(sizeof(cui_histogram_series) * (size_t)cnt) : NULL;
    char **names = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt) : NULL;
    float **counts = cnt ? tm_malloc(sizeof(float *) * (size_t)cnt) : NULL;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "series", pos + 1);
        row_req_str(vm, row, "name", who, "series", pos + 1);
        TVal *cv = row_get(row, "counts");
        if (!cv)
            RAISE(vm, "TypeError", "%s: series[%d][\"counts\"] is required", who, pos);
        int cn = 0;
        double *d = tui_floats(vm, cv, who, "series.counts", &cn);
        tui_arr_free(d);
        row_opt_int(vm, row, "tone", 0, who, "series", pos + 1);
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        names[k] = dup_s(row_req_str(vm, row, "name", who, "series", k + 1));
        int cn = 0;
        double *d = tui_floats(vm, row_get(row, "counts"), who, "series.counts", &cn);
        counts[k] = tm_malloc(sizeof(float) * (size_t)(cn ? cn : 1));
        for (int z = 0; z < cn; z++) counts[k][z] = (float)d[z];
        tui_arr_free(d);
        if (cn != (int)buckets) {
            for (int z = 0; z <= k; z++) { tm_free(names[z]); tm_free(counts[z]); }
            tm_free(names); tm_free(counts); tm_free(ss);
            RAISE(vm, "ValueError",
                  "%s: series[%d] counts length (%d) must equal buckets (%lld)",
                  who, k, cn, (long long)buckets);
        }
        ss[k].name = names[k];
        ss[k].counts = counts[k];
        ss[k].tone = (cui_tone)row_opt_int(vm, row, "tone", 0, who, "series", k + 1);
        k++;
    }
    cui_time_histogram_result r =
        cui_time_histogram_ex(tui_need_ui(vm, (TObj *)self, who), id, start,
                              bucket_s, (int)buckets, ss, cnt,
                              &sel_start, &sel_end, NULL);
    for (int i = 0; i < cnt; i++) { tm_free(names[i]); tm_free(counts[i]); }
    tm_free(names);
    tm_free(counts);
    tm_free(ss);
    TArr *a = res_new();
    res_bool(a, "selection_changed", r.selection_changed);
    res_int(a, "hovered", r.hovered);
    res_float(a, "sel_start", sel_start);
    res_float(a, "sel_end", sel_end);
    return TV_ARR(a);
}

/* coverage → ['activated'=>,'lines'=>]；
 * files=[['path'=>,'lines'=>,'lines_hit'=>,'branches'=>0,'branches_hit'=>0,
 *         'functions'=>0,'functions_hit'=>0]] */
static TVal nm_coverage(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::coverage()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "files");
    int cnt = count_rows(rows);
    cui_coverage_file *files = cnt
        ? tm_malloc(sizeof(cui_coverage_file) * (size_t)cnt) : NULL;
    char **paths = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt) : NULL;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "files", pos + 1);
        row_req_str(vm, row, "path", who, "files", pos + 1);
        row_opt_int(vm, row, "lines", 0, who, "files", pos + 1);
        row_opt_int(vm, row, "lines_hit", 0, who, "files", pos + 1);
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        paths[k] = dup_s(row_req_str(vm, row, "path", who, "files", k + 1));
        files[k].path = paths[k];
        files[k].lines = (int)row_opt_int(vm, row, "lines", 0, who, "files", k + 1);
        files[k].lines_hit =
            (int)row_opt_int(vm, row, "lines_hit", 0, who, "files", k + 1);
        files[k].branches =
            (int)row_opt_int(vm, row, "branches", 0, who, "files", k + 1);
        files[k].branches_hit =
            (int)row_opt_int(vm, row, "branches_hit", 0, who, "files", k + 1);
        files[k].functions =
            (int)row_opt_int(vm, row, "functions", 0, who, "files", k + 1);
        files[k].functions_hit =
            (int)row_opt_int(vm, row, "functions_hit", 0, who, "files", k + 1);
        k++;
    }
    cui_coverage_result r = cui_coverage_ex(tui_need_ui(vm, (TObj *)self, who),
                                            id, files, cnt, NULL);
    for (int i = 0; i < cnt; i++) tm_free(paths[i]);
    tm_free(paths);
    tm_free(files);
    TArr *a = res_new();
    res_int(a, "activated", r.activated);
    res_float(a, "lines", r.lines);
    return TV_ARR(a);
}

/* blame → ['clicked'=>,'hovered'=>]；
 * lines=[str...]、lineCommit=[int...]、commits=[['hash'=>,'author'=>,
 *        'summary'=>,'time'=>0]] */
static TVal nm_blame(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::blame()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int lc = 0;
    char **lines = tui_strs(vm, &args[1], who, "lines", &lc);
    int cc = 0;
    int64_t *line_commit = tui_ints(vm, &args[2], who, "lineCommit", &cc);
    if (cc != lc) {
        tui_strs_free(lines, lc);
        tui_arr_free(line_commit);
        RAISE(vm, "ValueError", "%s: lineCommit length (%d) must match lines (%d)",
              who, cc, lc);
    }
    TArr *rows = rows_of(vm, &args[3], who, "commits");
    int cnt = count_rows(rows);
    cui_blame_commit *commits = cnt
        ? tm_malloc(sizeof(cui_blame_commit) * (size_t)cnt) : NULL;
    char **strs = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt * 3) : NULL;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "commits", pos + 1);
        row_req_str(vm, row, "hash", who, "commits", pos + 1);
        row_req_str(vm, row, "author", who, "commits", pos + 1);
        row_req_str(vm, row, "summary", who, "commits", pos + 1);
        row_opt_int(vm, row, "time", 0, who, "commits", pos + 1);
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        const char *ck[] = { "hash", "author", "summary" };
        char *got[3];
        for (int z = 0; z < 3; z++) {
            got[z] = dup_s(row_req_str(vm, row, ck[z], who, "commits", k + 1));
            strs = tm_realloc(strs, sizeof(char *) * (size_t)(k * 3 + z + 1));
            strs[k * 3 + z] = got[z];
        }
        commits[k].hash = got[0];
        commits[k].author = got[1];
        commits[k].summary = got[2];
        commits[k].time = row_opt_int(vm, row, "time", 0, who, "commits", k + 1);
        k++;
    }
    int *ic_arr = tm_malloc(sizeof(int) * (size_t)(lc ? lc : 1));
    for (int i = 0; i < lc; i++) ic_arr[i] = (int)line_commit[i];
    cui_blame_result r = cui_blame_ex(tui_need_ui(vm, (TObj *)self, who), id,
                                      (const char *const *)lines, ic_arr, lc,
                                      commits, cnt, NULL);
    tui_strs_free(lines, lc);
    tui_arr_free(line_commit);
    tm_free(ic_arr);
    for (int i = 0; i < cnt * 3; i++) tm_free(strs[i]);
    tm_free(strs);
    tm_free(commits);
    TArr *a = res_new();
    res_int(a, "clicked", r.clicked);
    res_int(a, "hovered", r.hovered);
    return TV_ARR(a);
}

/* featureFlags → ['flag'=>,'env'=>,'flags'=>[['on'=>bool[≤8],'rollout'=>int[≤8]]...]]
 * flags 原地被 CUI 修改，回读完整状态交还 PHP */
static TVal nm_feature_flags(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::featureFlags()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "flags");
    int ec = 0;
    char **envs = tui_strs(vm, &args[2], who, "envs", &ec);
    if (ec < 1 || ec > CUI_FLAG_ENVS) {
        tui_strs_free(envs, ec);
        RAISE(vm, "ValueError", "%s: envs length must be in 1..%d", who,
              CUI_FLAG_ENVS);
    }
    int cnt = count_rows(rows);
    cui_feature_flag *flags = cnt
        ? tm_malloc(sizeof(cui_feature_flag) * (size_t)cnt) : NULL;
    char **strs = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt * 2) : NULL;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "flags", pos + 1);
        row_req_str(vm, row, "key", who, "flags", pos + 1);
        row_req_str(vm, row, "description", who, "flags", pos + 1);
        TVal *ov = row_get(row, "on");
        if (ov) {
            int on = 0;
            bool *b = tui_bools(vm, ov, who, "flags.on", &on);
            tui_arr_free(b);
        }
        TVal *rv = row_get(row, "rollout");
        if (rv) {
            int rn = 0;
            int64_t *r = tui_ints(vm, rv, who, "flags.rollout", &rn);
            tui_arr_free(r);
        }
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        strs[k * 2] = dup_s(row_req_str(vm, row, "key", who, "flags", k + 1));
        strs[k * 2 + 1] = dup_s(row_req_str(vm, row, "description", who, "flags", k + 1));
        for (int z = 0; z < CUI_FLAG_ENVS; z++) {
            flags[k].on[z] = false;
            flags[k].rollout[z] = 100;
        }
        TVal *ov = row_get(row, "on");
        if (ov && ov->t == T_ARR) {
            int on = 0;
            bool *b = tui_bools(vm, ov, who, "flags.on", &on);
            for (int z = 0; z < on && z < CUI_FLAG_ENVS; z++) flags[k].on[z] = b[z];
            tui_arr_free(b);
        }
        TVal *rv = row_get(row, "rollout");
        if (rv && rv->t == T_ARR) {
            int rn = 0;
            int64_t *r = tui_ints(vm, rv, who, "flags.rollout", &rn);
            for (int z = 0; z < rn && z < CUI_FLAG_ENVS; z++)
                flags[k].rollout[z] = (int)r[z];
            tui_arr_free(r);
        }
        flags[k].key = strs[k * 2];
        flags[k].description = strs[k * 2 + 1];
        k++;
    }
    cui_feature_flags_result r =
        cui_feature_flags_ex(tui_need_ui(vm, (TObj *)self, who), id, flags, cnt,
                             (const char *const *)envs, ec, NULL);
    /* 回读（CUI 原地改） */
    TArr *out = tarr_new();
    for (int i = 0; i < cnt; i++) {
        TArr *f = tarr_new();
        TArr *on_a = tarr_new();
        TArr *ro_a = tarr_new();
        for (int z = 0; z < ec; z++) {
            tarr_append(on_a, flags[i].on[z] ? TV_TRUE : TV_FALSE);
            tarr_append(ro_a, TV_INT(flags[i].rollout[z]));
        }
        tarr_set(f, TV_STR(tstr_new("on", 2)), TV_ARR(on_a));
        tarr_set(f, TV_STR(tstr_new("rollout", 7)), TV_ARR(ro_a));
        tarr_append(out, TV_ARR(f));
    }
    for (int i = 0; i < cnt * 2; i++) tm_free(strs[i]);
    tm_free(strs);
    tm_free(flags);
    tui_strs_free(envs, ec);
    TArr *a = res_new();
    res_int(a, "flag", r.flag);
    res_int(a, "env", r.env);
    res_arr(a, "flags", TV_ARR(out));
    return TV_ARR(a);
}

/* apiExplorer → ['selected'=>,'try_it'=>]；
 * endpoints=[['method'=>,'path'=>,'summary'=>,'tag'=>?,'deprecated'=>false,
 *             'params'=>[['name'=>,'in'=>,'type'=>,'required'=>false,
 *                         'description'=>?]]?]] */
static TVal nm_api_explorer(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::apiExplorer()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "endpoints");
    int cnt = count_rows(rows);
    cui_api_endpoint *eps = cnt
        ? tm_malloc(sizeof(cui_api_endpoint) * (size_t)cnt) : NULL;
    char **strs = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt * 4) : NULL;
    cui_api_param **param_lists = cnt
        ? tm_malloc(sizeof(cui_api_param *) * (size_t)cnt) : NULL;
    int *param_ns = cnt ? tm_malloc(sizeof(int) * (size_t)cnt) : NULL;
    char **param_strs = NULL;
    int nparam_str = 0;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "endpoints", pos + 1);
        row_req_str(vm, row, "method", who, "endpoints", pos + 1);
        row_req_str(vm, row, "path", who, "endpoints", pos + 1);
        row_req_str(vm, row, "summary", who, "endpoints", pos + 1);
        row_opt_str(vm, row, "tag", who, "endpoints", pos + 1);
        row_opt_bool(vm, row, "deprecated", false, who, "endpoints", pos + 1);
        TVal *pv = row_get(row, "params");
        if (pv) {
            if (pv->t != T_ARR)
                RAISE(vm, "TypeError", "%s: endpoints[%d][\"params\"] must be "
                      "an array", who, pos);
            TArr *pa = (TArr *)pv->u.p;
            for (uint32_t j = 0; j < pa->dused; j++) {
                TVal pe = pa->data[j];
                if (pe.t == T_UNDEF) continue;
                if (pe.t != T_ARR)
                    RAISE(vm, "TypeError", "%s: endpoints[%d].params entries "
                          "must be arrays", who, pos);
                TArr *pr = (TArr *)pe.u.p;
                row_req_str(vm, pr, "name", who, "endpoints.params", (int)j + 1);
                row_req_str(vm, pr, "in", who, "endpoints.params", (int)j + 1);
                row_req_str(vm, pr, "type", who, "endpoints.params", (int)j + 1);
            }
        }
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        const char *ek[] = { "method", "path", "summary", "tag" };
        char *got[4];
        for (int z = 0; z < 4; z++) {
            const char *s = z < 3
                ? row_req_str(vm, row, ek[z], who, "endpoints", k + 1)
                : row_opt_str(vm, row, ek[z], who, "endpoints", k + 1);
            got[z] = dup_s(s ? s : "");
            strs = tm_realloc(strs, sizeof(char *) * (size_t)(k * 4 + z + 1));
            strs[k * 4 + z] = got[z];
        }
        eps[k].method = got[0];
        eps[k].path = got[1];
        eps[k].summary = got[2];
        eps[k].tag = got[3][0] ? got[3] : NULL;
        eps[k].deprecated =
            row_opt_bool(vm, row, "deprecated", false, who, "endpoints", k + 1);
        /* params 二级结构 */
        eps[k].params = NULL;
        eps[k].param_count = 0;
        TVal *pv = row_get(row, "params");
        if (pv && pv->t == T_ARR) {
            TArr *pa = (TArr *)pv->u.p;
            int pc = 0;
            for (uint32_t j = 0; j < pa->dused; j++)
                if (pa->data[j].t != T_UNDEF) pc++;
            cui_api_param *pl = pc
                ? tm_malloc(sizeof(cui_api_param) * (size_t)pc) : NULL;
            int pk = 0;
            for (uint32_t j = 0; j < pa->dused; j++) {
                TVal pe = pa->data[j];
                if (pe.t == T_UNDEF) continue;
                TArr *pr = (TArr *)pe.u.p;
                const char *pk_s[] = { "name", "in", "type" };
                char *pg[4];
                for (int z = 0; z < 3; z++) {
                    pg[z] = dup_s(row_req_str(vm, pr, pk_s[z], who,
                                              "endpoints.params", (int)j + 1));
                    param_strs = tm_realloc(param_strs,
                                            sizeof(char *) * (size_t)(nparam_str + 1));
                    param_strs[nparam_str++] = pg[z];
                }
                const char *desc = row_opt_str(vm, pr, "description", who,
                                               "endpoints.params", (int)j + 1);
                pg[3] = desc ? dup_s(desc) : NULL;
                if (pg[3]) {
                    param_strs = tm_realloc(param_strs,
                                            sizeof(char *) * (size_t)(nparam_str + 1));
                    param_strs[nparam_str++] = pg[3];
                }
                pl[pk].name = pg[0];
                pl[pk].in = pg[1];
                pl[pk].type = pg[2];
                pl[pk].required =
                    row_opt_bool(vm, pr, "required", false, who,
                                 "endpoints.params", (int)j + 1);
                pl[pk].description = pg[3];
                pk++;
            }
            param_lists[k] = pl;
            param_ns[k] = pc;
            eps[k].params = pl;
            eps[k].param_count = pc;
        } else {
            param_lists[k] = NULL;
            param_ns[k] = 0;
        }
        k++;
    }
    cui_api_explorer_result r =
        cui_api_explorer_ex(tui_need_ui(vm, (TObj *)self, who), id, eps, cnt, NULL);
    for (int i = 0; i < cnt; i++) {
        for (int z = 0; z < 4; z++) tm_free(strs[i * 4 + z]);
        tm_free(param_lists[i]);
    }
    for (int i = 0; i < nparam_str; i++) tm_free(param_strs[i]);
    tm_free(param_strs);
    tm_free(param_lists);
    tm_free(param_ns);
    tm_free(strs);
    tm_free(eps);
    TArr *a = res_new();
    res_int(a, "selected", r.selected);
    res_int(a, "try_it", r.try_it);
    return TV_ARR(a);
}

static TVal nm_readout(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::readout()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    const char *value = tui_arg_str(vm, args, n, 1, who);
    cui_readout_ex(tui_need_ui(vm, (TObj *)self, who), label, value, NULL);
    return TV_NULL;
}

static TVal nm_dot_display(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\Content::dotDisplay()";
    const char *text = tui_arg_str(vm, args, n, 0, who);
    cui_dot_display_ex(tui_need_ui(vm, (TObj *)self, who), text, NULL);
    return TV_NULL;
}

/* ---- 注册（核心部分；devtools 见 tui_register_content 后半段） ---- */

void tui_register_content(TClass *cls) {
    tui_cls_add_native(cls, "codeBlock", (void *)nm_code_block, false);
    tui_cls_add_native(cls, "jsonView", (void *)nm_json_view, false);
    tui_cls_add_native(cls, "markdown", (void *)nm_markdown, false);
    tui_cls_add_native(cls, "codeEditor", (void *)nm_code_editor, false);
    tui_cls_add_native(cls, "codeEditorGoto", (void *)nm_code_editor_goto, false);
    tui_cls_add_native(cls, "codeEditorSelect", (void *)nm_code_editor_select, false);
    tui_cls_add_native(cls, "codeEditorFind", (void *)nm_code_editor_find, false);
    tui_cls_add_native(cls, "diffView", (void *)nm_diff_view, false);
    tui_cls_add_native(cls, "hexView", (void *)nm_hex_view, false);
    tui_cls_add_native(cls, "hexViewGoto", (void *)nm_hex_view_goto, false);
    tui_cls_add_native(cls, "terminalInit", (void *)nm_terminal_init, false);
    tui_cls_add_native(cls, "terminalWrite", (void *)nm_terminal_write, false);
    tui_cls_add_native(cls, "terminalClear", (void *)nm_terminal_clear, false);
    tui_cls_add_native(cls, "terminal", (void *)nm_terminal, false);
    tui_cls_add_native(cls, "logInit", (void *)nm_log_init, false);
    tui_cls_add_native(cls, "logAppend", (void *)nm_log_append, false);
    tui_cls_add_native(cls, "logClear", (void *)nm_log_clear, false);
    tui_cls_add_native(cls, "logView", (void *)nm_log_view, false);
    tui_cls_add_native(cls, "commandPalette", (void *)nm_command_palette, false);
    tui_cls_add_native(cls, "tour", (void *)nm_tour, false);
    tui_cls_add_native(cls, "gantt", (void *)nm_gantt, false);
    tui_cls_add_native(cls, "fileBrowser", (void *)nm_file_browser, false);
    tui_cls_add_native(cls, "filePicker", (void *)nm_file_picker, false);
    tui_cls_add_native(cls, "imageLoad", (void *)nm_image_load, false);
    tui_cls_add_native(cls, "imageCreate", (void *)nm_image_create, false);
    tui_cls_add_native(cls, "imageDestroy", (void *)nm_image_destroy, false);
    tui_cls_add_native(cls, "image", (void *)nm_image, false);
    tui_cls_add_native(cls, "imageViewer", (void *)nm_image_viewer, false);
    tui_cls_add_native(cls, "cropper", (void *)nm_cropper, false);
    tui_cls_add_native(cls, "emoji", (void *)nm_emoji, false);
    tui_cls_add_native(cls, "emojiPicker", (void *)nm_emoji_picker, false);
    tui_cls_add_native(cls, "emojiSupported", (void *)nm_emoji_supported, false);
    tui_cls_add_native(cls, "emojiFontLoadFile", (void *)nm_emoji_font_load_file, false);
    tui_cls_add_native(cls, "emojiFontLoadSystem", (void *)nm_emoji_font_load_system, false);
    tui_cls_add_native(cls, "chatMessage", (void *)nm_chat_message, false);
    tui_cls_add_native(cls, "chatDivider", (void *)nm_chat_divider, false);
    tui_cls_add_native(cls, "typingIndicator", (void *)nm_typing_indicator, false);
    tui_cls_add_native(cls, "comment", (void *)nm_comment, false);
    tui_cls_add_native(cls, "presence", (void *)nm_presence, false);
    tui_cls_add_native(cls, "signaturePad", (void *)nm_signature_pad, false);
    tui_cls_add_native(cls, "signaturePoints", (void *)nm_signature_points, false);
    tui_cls_add_native(cls, "signatureClear", (void *)nm_signature_clear, false);
    tui_cls_add_native(cls, "nodeEditorBegin", (void *)nm_node_editor_begin, false);
    tui_cls_add_native(cls, "nodeEditorEnd", (void *)nm_node_editor_end, false);
    tui_cls_add_native(cls, "nodeBegin", (void *)nm_node_begin, false);
    tui_cls_add_native(cls, "nodeEnd", (void *)nm_node_end, false);
    tui_cls_add_native(cls, "nodeInput", (void *)nm_node_input, false);
    tui_cls_add_native(cls, "nodeOutput", (void *)nm_node_output, false);
    tui_cls_add_native(cls, "nodeContentBegin", (void *)nm_node_content_begin, false);
    tui_cls_add_native(cls, "nodeContentEnd", (void *)nm_node_content_end, false);
    tui_cls_add_native(cls, "nodeLink", (void *)nm_node_link, false);
    tui_cls_add_native(cls, "nodeLinkCreated", (void *)nm_node_link_created, false);
    tui_cls_add_native(cls, "nodeLinkRemoved", (void *)nm_node_link_removed, false);
    tui_cls_add_native(cls, "nodeDeleted", (void *)nm_node_deleted, false);
    /* devtools / 仪表族 */
    tui_cls_add_native(cls, "testExplorer", (void *)nm_test_explorer, false);
    tui_cls_add_native(cls, "stackTrace", (void *)nm_stack_trace, false);
    tui_cls_add_native(cls, "requestBar", (void *)nm_request_bar, false);
    tui_cls_add_native(cls, "responseStatus", (void *)nm_response_status, false);
    tui_cls_add_native(cls, "uptime", (void *)nm_uptime, false);
    tui_cls_add_native(cls, "timeHistogram", (void *)nm_time_histogram, false);
    tui_cls_add_native(cls, "coverage", (void *)nm_coverage, false);
    tui_cls_add_native(cls, "blame", (void *)nm_blame, false);
    tui_cls_add_native(cls, "featureFlags", (void *)nm_feature_flags, false);
    tui_cls_add_native(cls, "apiExplorer", (void *)nm_api_explorer, false);
    tui_cls_add_native(cls, "readout", (void *)nm_readout, false);
    tui_cls_add_native(cls, "dotDisplay", (void *)nm_dot_display, false);
}
