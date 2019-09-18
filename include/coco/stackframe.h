#ifndef _COCO_STACKFRAME_H_
#define _COCO_STACKFRAME_H_

namespace coco {

typedef unsigned long reg_t;

struct StackFrame {
    /* first argument */
    reg_t rdi;
    /* callee-saved registers */
    reg_t r15;
    reg_t r14;
    reg_t r13;
    reg_t r12;
    reg_t rbp;
    reg_t rbx;
    /* first argument */
    reg_t rsp;
} __attribute__((packed));

} // namespace coco

#endif
