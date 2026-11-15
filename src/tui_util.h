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

/* ---- 类注册（teb 同款模式；tui_cls_new/add_ctor/add_prop 留在 tui.c） ---- */
void tui_cls_add_native(TClass *cls, const char *name, void *fn, bool is_static);

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

#endif /* TUI_UTIL_H */
