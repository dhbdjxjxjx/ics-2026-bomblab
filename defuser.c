#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

#define MAX_FIELDS 8

/*
 * 把 values 中的 count 个整数写成一行“空格分隔的无符号十进制整数”，存入 out。
 * 成功返回 0；count 为 0（还没有实现）、snprintf 出错或结果被截断时返回非零。
 */
static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32, i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return count == 0;
}

int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    /*
     * challenge 按 32 位切片：lo = 低 32 位，hi = 高 32 位，mid = challenge >> 16 的低 32 位。
     * p1_step1: 3*hi ^ (mid + 0x1d62)
     * p1_step0: (lo + 2*hi) ^ 0xdedd
     * p1_step2: 5*(step0 - step1) + 0x2820
     * 全部是 32 位无符号运算，自然按 2^32 回绕。
     */
    uint32_t lo = (uint32_t)challenge;
    uint32_t hi = (uint32_t)(challenge >> 32);
    uint32_t mid = (uint32_t)(challenge >> 16);

    uint32_t step1 = 3u * hi ^ (mid + UINT32_C(0x1d62));
    uint32_t step0 = (lo + 2u * hi) ^ UINT32_C(0xdedd);
    uint32_t answer = 5u * (step0 - step1) + UINT32_C(0x2820);

    uint32_t values[MAX_FIELDS] = {answer};
    return write_answer(out, cap, values, 1);
}

int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    /* 六个数：v0 由 challenge 的第 37 位起 5 位决定；v_i 依赖前一项，构成递推。 */
    uint32_t values[MAX_FIELDS] = {0};

    values[0] = (uint32_t)(((challenge >> 37) & UINT64_C(0x1f)) * 2 + 4);
    for (uint32_t i = 1; i < 6; ++i) {
        uint32_t key = (uint32_t)((challenge >> (i + 10)) & UINT64_C(0x1f)) + UINT32_C(0x0f);
        values[i] = key ^ (3u * values[i - 1] - i);
    }
    return write_answer(out, cap, values, 6);
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    /* 第一个数是跳转表下标 (challenge & 7)；第二个数由 p3_dispatch 按该下标分派计算。 */
    uint32_t index = (uint32_t)(challenge & UINT64_C(7));
    uint32_t arg = (uint32_t)(challenge >> 3) ^ (uint32_t)(challenge >> 32);
    uint32_t value;

    switch (index) {
    case 0: value = (arg ^ UINT32_C(0x6fa3)) + (arg >> 1); break;      /* p3_case_3 */
    case 1: value = (4u * arg) ^ UINT32_C(0xede); break;               /* p3_case_1 */
    case 2: value = 3u * arg + UINT32_C(0x1071); break;                /* p3_case_6 */
    case 3: value = 5u * arg + UINT32_C(0xbd48); break;                /* p3_case_2 */
    case 4: value = (arg ^ UINT32_C(0x7128)) + (arg >> 1); break;      /* p3_case_5 */
    case 5: value = (4u * arg) ^ UINT32_C(0xe060); break;              /* p3_case_7 */
    case 6: value = 5u * arg + UINT32_C(0x10423); break;               /* p3_case_4 */
    default: value = 5u * arg + UINT32_C(0x3cb13); break;              /* p3_case_0 */
    }

    uint32_t values[2] = {index, value};
    return write_answer(out, cap, values, 2);
}

/* 二分查找 target：mid == target 时返回 (mid + 0x95) ^ d，回溯时逐层折算。 */
static uint32_t p4_walk(uint32_t target, uint32_t lo, uint32_t hi, uint32_t d) {
    uint32_t mid = lo + (hi - lo) / 2;
    if (mid == target)
        return (mid + UINT32_C(0x95)) ^ d;

    uint32_t mix = mid + d;
    uint32_t inner;
    if (target < mid) {
        inner = p4_walk(target, lo, mid - 1, d);
        return 5u * inner ^ mix ^ UINT32_C(0xb);
    }
    inner = p4_walk(target, mid + 1, hi, d);
    return 5u * inner ^ mix ^ UINT32_C(0x1a);
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t target = (uint32_t)(challenge & UINT64_C(0x7f));
    uint32_t d = (uint32_t)(challenge >> 7) ^ (uint32_t)(challenge >> 32);
    uint32_t answer = p4_walk(target, 0, UINT32_C(0x7f), d);

    uint32_t values[MAX_FIELDS] = {answer};
    return write_answer(out, cap, values, 1);
}

/* phase_05 的链表：节点按 .rodata 中的顺序编号，next 为 -1 表示空。 */
struct p5_node {
    uint32_t tag;
    uint32_t value;
    int next;
};

static const struct p5_node p5_nodes[12] = {
    /* 0  */ {0x80d, 0x22ca, 8},
    /* 1  */ {0x608, 0xfa26, 2},
    /* 2  */ {0xb2e, 0xfc17, -1},
    /* 3  */ {0x33b, 0x400f, 7},
    /* 4  */ {0xa17, 0xdfa2, 1},
    /* 5  */ {0x20c, 0x68cd, 10},
    /* 6  */ {0x034, 0xb30f, 11},
    /* 7  */ {0x513, 0xf048, 6},
    /* 8  */ {0x473, 0xc0d4, 4},
    /* 9  */ {0x762, 0x688b, 5},
    /* 10 */ {0x118, 0x61f1, 3},
    /* 11 */ {0x925, 0x4164, 0},
};

/* p5_heads[i] 是与 (challenge & 7) 对应的起始节点下标。 */
static const int p5_heads[8] = {9, 5, 10, 3, 7, 6, 11, 0};

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    uint32_t index = (uint32_t)(challenge & UINT64_C(7));
    uint32_t extra = (uint32_t)((challenge >> 3) & UINT64_C(3));
    uint32_t d = (uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32);

    int cur = p5_heads[index];
    uint32_t acc = (d ^ UINT32_C(0x2128)) + p5_nodes[cur].value;
    acc = 5u * acc;
    uint32_t tag = p5_nodes[cur].tag;
    acc ^= tag;

    for (uint32_t i = 0; i < extra; ++i) {
        cur = p5_nodes[cur].next;
        acc += p5_nodes[cur].value;
        acc = 5u * acc;
        tag = p5_nodes[cur].tag;
        acc ^= tag;
    }

    uint32_t values[2] = {acc, tag};
    return write_answer(out, cap, values, 2);
}

/* phase_06 的函数指针表：下标 0..7 依次对应 p6_op_3/4/0/2/1/5/1/5 的语义。 */
static uint32_t p6_op(uint32_t sel, uint32_t x) {
    switch (sel) {
    case 0: return (x ^ UINT32_C(0xbbb3)) + 4u * x;             /* p6_op_3 */
    case 1: return ((x << 5) | (x >> 27)) ^ UINT32_C(0xc7eb);   /* p6_op_4: rol 5 */
    case 2: return (x >> 7) | (x << 25);                        /* p6_op_0: ror 7 */
    case 3: return ((x << 5) + UINT32_C(0xc6b4)) ^ (7u * x);    /* p6_op_2 */
    case 4: return ((x >> 2) ^ x) + UINT32_C(0x2774);           /* p6_op_1 */
    case 5: return (x + UINT32_C(0xf9d9)) ^ (x >> 2);           /* p6_op_5 */
    case 6: return ((x >> 2) ^ x) + UINT32_C(0x2774);           /* p6_op_1 */
    default: return (x + UINT32_C(0xf9d9)) ^ (x >> 2);          /* p6_op_5 */
    }
}

int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    uint32_t x = (uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32);
    uint32_t inner = p6_op((uint32_t)(challenge & UINT64_C(7)), x) ^ UINT32_C(0xaa20);
    uint32_t answer = p6_op((uint32_t)((challenge & UINT64_C(0x38)) >> 3), inner) + UINT32_C(0x7f75);

    uint32_t values[MAX_FIELDS] = {answer};
    return write_answer(out, cap, values, 1);
}

int solve_secret(uint64_t challenge, uint64_t context, char *out, size_t cap) {
    /*
     * 行格式：32 字节前缀 + "OPEN" + 空格 + 三个无符号十进制整数。
     * secret_gate 在偏移 0x20 处比较 "OPEN"，secret_phase 从偏移 0x24 解析三个数。
     */
    static const char prefix[] = "ics-2026fall-bomblab-honor-pad--"; /* 正好 32 字节 */

    uint32_t k = (uint32_t)(context & UINT64_C(3));
    uint32_t x = (uint32_t)challenge ^ (uint32_t)(challenge >> 32);
    uint32_t y = (uint32_t)context ^ (uint32_t)(context >> 32);
    uint32_t z = (uint32_t)(context >> 16);

    uint32_t t = x ^ y;
    uint32_t mixed = ((t >> 3) | (t << 29)) ^ UINT32_C(0x60e6); /* secret_mix: ror 3 */
    uint32_t b = mixed ^ (k * UINT32_C(0xf798)) ^ z;
    uint32_t c = b ^ (UINT32_C(0x977e) + y + k + (x >> 3));
    uint32_t a = k;

    int size = snprintf(out, cap, "%sOPEN %" PRIu32 " %" PRIu32 " %" PRIu32,
                        prefix, a, b, c);
    return size < 0 || (size_t)size >= cap;
}
