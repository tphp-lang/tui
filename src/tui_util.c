/*
 * tphp/tui - src/tui_util.c - Task 1 参数助手层实现（详见 tui_util.h）
 *
 * 数组助手一律两趟执行：第一趟全量类型校验 + 计数，第二趟才分配转换。
 * 原因：tphp_vm_raise 是 noreturn（长跳转展开），先分配再校验会在抛
 * 异常时泄漏已分配内存；两趟结构保证第二趟不可能再抛，天然无泄漏。
 */
#include "tui_util.h"
#include "builtin.h"    /* need_int（strict：仅 T_INT） */

#include <string.h>

/* ---- 参数读取（必需） ---- */

const char *tui_arg_str(VM *vm, TVal *args, int n, int i, const char *who) {
    if (n <= i || args[i].t != T_STR)
        tphp_vm_raise(vm, "TypeError", "%s() expects argument %d to be a string",
                      who, i + 1);
    return tstr_val((TStr *)args[i].u.p);
}

int64_t tui_arg_int(VM *vm, TVal *args, int n, int i, const char *who,
                    const char *pname) {
    if (n <= i)
        tphp_vm_raise(vm, "ArgumentCountError", "%s() expects at least %d argument(s)",
                      who, i + 1);
    return need_int(vm, &args[i], who, pname);
}

bool tui_arg_bool(VM *vm, TVal *args, int n, int i, const char *who,
                  const char *pname) {
    if (n <= i)
        tphp_vm_raise(vm, "ArgumentCountError", "%s() expects at least %d argument(s)",
                      who, i + 1);
    if (args[i].t != T_TRUE && args[i].t != T_FALSE)
        tphp_vm_raise(vm, "TypeError", "%s(): argument %s must be of type bool, %s given",
                      who, pname, tphp_val_type_name(args[i]));
    return args[i].t == T_TRUE;
}

double tui_arg_float(VM *vm, TVal *args, int n, int i, const char *who,
                     const char *pname) {
    if (n <= i)
        tphp_vm_raise(vm, "ArgumentCountError", "%s() expects at least %d argument(s)",
                      who, i + 1);
    if (args[i].t == T_FLOAT) return args[i].u.d;
    if (args[i].t == T_INT) return (double)args[i].u.i;
    tphp_vm_raise(vm, "TypeError", "%s(): argument %s must be of type float, %s given",
                  who, pname, tphp_val_type_name(args[i]));
}

/* ---- 枚举 case 读取 ----
 * 按 FQCN 查当前线程类表（worker 的类表是克隆，不能缓存 master 指针）。
 * 命中 CK_ENUM 的类后，用 tphp_enum_case_find 确认对象确为该类 case 单例，
 * 再取 backing int。非 case 对象（例如 new 出来的普通实例）一律 TypeError。 */

/* PHP 类名习惯是 UpperCamelCase，但 FQCN 由扩展作者给定。这里按「命名空间
 * 分隔符后首字母大写，其余原样」渲染，不做中间驼峰——那是猜，猜错比留小写
 * 更误导。报错文案要的是「用户能对上号」，不是完美还原声明。 */
static const char *enum_display(const char *enum_lc, char *buf, size_t cap) {
    size_t i = 0;
    bool up = true;
    for (; enum_lc[i] && i + 1 < cap; i++) {
        char c = enum_lc[i];
        if (c == '\\') { up = true; buf[i] = c; continue; }
        buf[i] = up ? (char)((c >= 'a' && c <= 'z') ? c - 32 : c) : c;
        up = false;
    }
    buf[i] = 0;
    return buf;
}

int64_t tui_opt_enum(VM *vm, TVal *args, int n, int i, int64_t def,
                     const char *who, const char *pname, const char *enum_lc) {
    if (n <= i) return def;
    TVal v = args[i];
    if (v.t == T_INT) return v.u.i;              /* 旧 API 兼容 */
    if (v.t == T_OBJ) {
        TObj *o = (TObj *)v.u.p;
        TClass *cls = o->cls;
        if (cls && cls->kind == CK_ENUM &&
            tstr_case_eq(tstr_val(cls->name), enum_lc)) {
            int ci = tphp_enum_case_find(cls, o);
            if (ci >= 0 && cls->cases[ci].backing.t == T_INT)
                return cls->cases[ci].backing.u.i;
        }
    }
    char dbuf[256];
    /* who 形如 "Tphp\\Tui\\App::button()"（自带括号），故不再补 */
    tphp_vm_raise(vm, "TypeError",
                  "%s: argument %s must be of type %s, %s given",
                  who, pname, enum_display(enum_lc, dbuf, sizeof dbuf),
                  tphp_val_type_name(v));
}

int64_t tui_arg_enum(VM *vm, TVal *args, int n, int i, const char *who,
                     const char *pname, const char *enum_lc) {
    if (n <= i)
        tphp_vm_raise(vm, "ArgumentCountError",
                      "%s() expects at least %d argument(s)", who, i + 1);
    return tui_opt_enum(vm, args, n, i, 0, who, pname, enum_lc);
}

/* ---- 参数读取（可选：缺省给默认值，给了但类型不对抛 TypeError） ---- */

int64_t tui_opt_int(VM *vm, TVal *args, int n, int i, int64_t def,
                    const char *who, const char *pname) {
    if (n <= i) return def;
    return need_int(vm, &args[i], who, pname);   /* strict：非 int 抛 TypeError */
}

bool tui_opt_bool(VM *vm, TVal *args, int n, int i, bool def,
                  const char *who, const char *pname) {
    if (n <= i) return def;
    if (args[i].t != T_TRUE && args[i].t != T_FALSE)
        tphp_vm_raise(vm, "TypeError", "%s(): argument %s must be of type bool, %s given",
                      who, pname, tphp_val_type_name(args[i]));
    return args[i].t == T_TRUE;
}

const char *tui_opt_str(VM *vm, TVal *args, int n, int i, const char *def,
                        const char *who, const char *pname) {
    if (n <= i) return def;
    if (args[i].t != T_STR)
        tphp_vm_raise(vm, "TypeError", "%s(): argument %s must be of type string, %s given",
                      who, pname, tphp_val_type_name(args[i]));
    return tstr_val((TStr *)args[i].u.p);
}

double tui_opt_float(VM *vm, TVal *args, int n, int i, double def,
                     const char *who, const char *pname) {
    if (n <= i) return def;
    if (args[i].t == T_FLOAT) return args[i].u.d;
    if (args[i].t == T_INT) return (double)args[i].u.i;
    tphp_vm_raise(vm, "TypeError", "%s(): argument %s must be of type float, %s given",
                  who, pname, tphp_val_type_name(args[i]));
}

/* ---- App 句柄（props["app"] = TV_INT 地址） ---- */

cui_app *tui_app_of(TObj *o) {
    int s = tphp_class_find_prop(o->cls, "app");
    if (s < 0) return NULL;
    TVal v = o->props[s];
    if (v.t != T_INT || v.u.i == 0) return NULL;
    return (cui_app *)(intptr_t)v.u.i;
}

cui_app *tui_need_app(VM *vm, TObj *o, const char *who) {
    cui_app *a = tui_app_of(o);
    if (!a)
        tphp_vm_raise(vm, "Error", "%s: window is already destroyed", who);
    return a;
}

cui_ctx *tui_need_ui(VM *vm, TObj *o, const char *who) {
    return cui_app_ui(tui_need_app(vm, o, who));
}

/* ---- SCOPE_IF 失配检测（阶段 A）----
 * 遍历各域的未闭合 begin 标记，返回第一个仍为 true 的 PHP begin 名；
 * 全闭合返回 NULL。frame() 在渲染上一段之前调用，残留即意味着上一段
 * 漏写 xxxEnd()，继续渲染会令 node 栈失衡而随机 AV（coverage.md §3）。 */
const char *tui_scopeif_unclosed(void) {
    const char *n = tui_layout_scopeif_unclosed();
    if (n) return n;
    return tui_content_scopeif_unclosed();
}

/* destroy() 时清残留：避免本实例的未闭合标记污染下一个 App 实例 */
void tui_scopeif_clear(void) {
    /* 逐域清理由各域导出；此处仅调用汇总清理由各域函数完成。
     * 当前实现见 tui_layout.c / tui_content.c 的同名 helper。 */
    tui_layout_scopeif_clear();
    tui_content_scopeif_clear();
}

/* ---- 类注册 ---- */

void tui_cls_add_native(TClass *cls, const char *name, void *fn,
                        bool is_static) {
    TMethod *ms = tm_realloc(cls->methods,
                             sizeof(TMethod) * (size_t)(cls->n_methods + 1));
    TMethod *m = &ms[cls->n_methods];
    memset(m, 0, sizeof *m);
    m->name = tstr_intern(name, strlen(name));
    m->visibility = 0;            /* CVIS_PUBLIC */
    m->native = fn;
    m->is_static = is_static;
    cls->methods = ms;
    cls->n_methods++;
}

/* ---- 数组转换 ----
 * 迭代口径（array.c）：data[0..dused) 为插入序，T_UNDEF = unset 空洞跳过。
 * 第一趟校验 + 计数，第二趟分配转换（第二趟不再有抛出点）。 */

static TArr *tui_arr_of(VM *vm, TVal *v, const char *who, const char *pname) {
    if (v->t != T_ARR)
        tphp_vm_raise(vm, "TypeError",
                      "%s(): argument %s must be of type array, %s given",
                      who, pname, tphp_val_type_name(*v));
    return (TArr *)v->u.p;
}

/* 元素类型错误：位置 1 起、指明参数名 */
static void tui_elem_type_err(VM *vm, const char *who, const char *pname,
                              int pos, const char *want, TVal e) {
    tphp_vm_raise(vm, "TypeError",
                  "%s(): array element %d of argument %s must be of type %s, %s given",
                  who, pos, pname, want, tphp_val_type_name(e));
}

char **tui_strs(VM *vm, TVal *v, const char *who, const char *pname, int *n) {
    TArr *a = tui_arr_of(vm, v, who, pname);
    /* 第一趟：全量类型校验 + 计数 */
    int cnt = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        if (e.t != T_STR) tui_elem_type_err(vm, who, pname, cnt + 1, "string", e);
        cnt++;
    }
    *n = cnt;
    if (!cnt) return NULL;
    /* 第二趟：逐项拷贝（tstr_val 是 TStr 内部缓冲，必须拷贝才能跨帧持有） */
    char **out = tm_malloc(sizeof(char *) * (size_t)cnt);
    int k = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        TStr *s = (TStr *)e.u.p;
        char *c = tm_malloc((size_t)s->len + 1);
        memcpy(c, s->c, s->len);
        c[s->len] = '\0';
        out[k++] = c;
    }
    return out;
}

double *tui_floats(VM *vm, TVal *v, const char *who, const char *pname, int *n) {
    TArr *a = tui_arr_of(vm, v, who, pname);
    /* 第一趟：校验（允许 int/float，其余 TypeError）+ 计数 */
    int cnt = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        if (e.t != T_INT && e.t != T_FLOAT)
            tui_elem_type_err(vm, who, pname, cnt + 1, "int|float", e);
        cnt++;
    }
    *n = cnt;
    if (!cnt) return NULL;
    /* 第二趟：转换（tval_to_float：int 安全升格，float 原样） */
    double *out = tm_malloc(sizeof(double) * (size_t)cnt);
    int k = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        out[k++] = tval_to_float(e);
    }
    return out;
}

int64_t *tui_ints(VM *vm, TVal *v, const char *who, const char *pname, int *n) {
    TArr *a = tui_arr_of(vm, v, who, pname);
    /* 第一趟：校验（仅 int；float 不做静默截断）+ 计数 */
    int cnt = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        if (e.t != T_INT) tui_elem_type_err(vm, who, pname, cnt + 1, "int", e);
        cnt++;
    }
    *n = cnt;
    if (!cnt) return NULL;
    /* 第二趟：直取（已校验为 T_INT，无抛出点） */
    int64_t *out = tm_malloc(sizeof(int64_t) * (size_t)cnt);
    int k = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        out[k++] = e.u.i;
    }
    return out;
}

bool *tui_bools(VM *vm, TVal *v, const char *who, const char *pname, int *n) {
    TArr *a = tui_arr_of(vm, v, who, pname);
    /* 第一趟：校验（仅 bool）+ 计数 */
    int cnt = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        if (e.t != T_TRUE && e.t != T_FALSE)
            tui_elem_type_err(vm, who, pname, cnt + 1, "bool", e);
        cnt++;
    }
    *n = cnt;
    if (!cnt) return NULL;
    /* 第二趟：直取 */
    bool *out = tm_malloc(sizeof(bool) * (size_t)cnt);
    int k = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        out[k++] = (e.t == T_TRUE);
    }
    return out;
}

void tui_strs_free(char **a, int n) {
    if (!a) return;
    for (int i = 0; i < n; i++) tm_free(a[i]);
    tm_free(a);
}

void tui_arr_free(void *p) {
    tm_free(p);   /* tm_free(NULL) 安全；空数组返回 NULL 可直接传入 */
}
