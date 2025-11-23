#include <assert.h>
#include <stddef.h>

#include "common.h"

#define DEFAULT_SEGMENT_SIZE (64 * 1024)

static void stack_segment_insert(Stack *stack) {
    StackSegment *new_segment =
        calloc(1, sizeof(StackSegment) + DEFAULT_SEGMENT_SIZE);
    if (new_segment == NULL) {
        panic("out of memory");
    }

    new_segment->cap = DEFAULT_SEGMENT_SIZE;
    new_segment->sp = 0;
    new_segment->bp = 0;

    new_segment->next = stack->top;
    stack->top = new_segment;
}

Stack stack_new(void) {
    return (Stack){
        .top = NULL,
    };
}

static inline uptr align_forward(uptr p, uptr align) {
    return (p + (align - 1)) & ~(align - 1);
}

typedef struct {
    uptr bp;
    uptr off;  // Used to get the object pointer from `alloc`
    u8 alloc[];
} Frame;

#define FRAME_ALLOC_OFFSET sizeof(uptr) * 2

typedef struct {
    uptr aligned_size;
    uptr frame_addr;
    uptr aligned_frame_addr;
    uptr obj_addr;
    uptr aligned_obj_addr;
} FrameDescriptor;

static inline FrameDescriptor get_frame_descriptor(StackSegment *seg, uptr size,
                                                   uptr align) {
    uptr sp_addr = (uptr)&seg->data[seg->sp];
    uptr frame_addr = align_forward(sp_addr, _Alignof(Frame));
    uptr obj_addr = FRAME_ALLOC_OFFSET + frame_addr;
    uptr aligned_obj_addr = align_forward(obj_addr, align);
    return (FrameDescriptor){
        .aligned_size = (aligned_obj_addr + size) - sp_addr,
        .frame_addr = sp_addr,
        .aligned_frame_addr = frame_addr,
        .obj_addr = obj_addr,
        .aligned_obj_addr = aligned_obj_addr,
    };
}

void *stack_push(Stack *stack, uptr size, uptr align) {
    assert(size <= (DEFAULT_SEGMENT_SIZE - 2 * _Alignof(max_align_t)));

    if (stack->top == NULL) {
        stack_segment_insert(stack);
    }

    StackSegment *seg = stack->top;

    if (get_frame_descriptor(seg, size, align).aligned_size >
        seg->cap - seg->sp) {
        stack_segment_insert(stack);
        seg = stack->top;
    }

    // NOTE: At this point we know we have a large enough segment stored in the
    // variable `seg`

    uptr saved_bp = seg->bp;

    FrameDescriptor frame_desc = get_frame_descriptor(seg, size, align);
    seg->bp = frame_desc.aligned_frame_addr - (uptr)&seg->data;
    seg->sp =
        (frame_desc.frame_addr + frame_desc.aligned_size) - (uptr)&seg->data;

    Frame *new_frame = (Frame *)frame_desc.aligned_frame_addr;
    new_frame->bp = saved_bp;
    new_frame->off = frame_desc.aligned_obj_addr - frame_desc.obj_addr;

    assert((uptr)&new_frame->alloc == frame_desc.obj_addr);
    assert((uptr)&new_frame->alloc[new_frame->off] ==
           frame_desc.aligned_obj_addr);

    return (void *)frame_desc.aligned_obj_addr;
}

bool stack_empty(Stack *stack) { return stack->top == NULL; }

void stack_pop(Stack *stack) {
    StackSegment *seg = stack->top;

    assert(seg);
    assert(seg->sp != 0);

    Frame *frame = (Frame *)&seg->data[seg->bp];
    uptr new_bp = frame->bp;
    seg->sp = seg->bp;
    seg->bp = new_bp;

    if (seg->sp == 0) {
        stack->top = seg->next;
        free(seg);
    }
}

void *stack_top(Stack *stack) {
    assert(!stack_empty(stack));

    StackSegment *seg = stack->top;

    assert(seg);
    assert(seg->sp != 0);

    Frame *frame = (Frame *)&seg->data[seg->bp];
    return &frame->alloc[frame->off];
}
