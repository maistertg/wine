/*
 * Copyright 2012 Henri Verbeet for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wined3d_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d);

enum wined3d_cs_op
{
    WINED3D_CS_OP_FENCE,
    WINED3D_CS_OP_PRESENT,
    WINED3D_CS_OP_CLEAR,
    WINED3D_CS_OP_DRAW,
    WINED3D_CS_OP_STATEBLOCK,
    WINED3D_CS_OP_SET_RENDER_TARGET,
    WINED3D_CS_OP_SET_VS_CONSTS_F,
    WINED3D_CS_OP_SET_PS_CONSTS_F,
    WINED3D_CS_OP_RESET_STATE,
    WINED3D_CS_OP_GLFINISH,
    WINED3D_CS_OP_STOP,
};

struct wined3d_cs_stop
{
    enum wined3d_cs_op opcode;
};

struct wined3d_cs_fence
{
    enum wined3d_cs_op opcode;
    BOOL *signalled;
};

#define CS_PRESENT_SRC_RECT 1
#define CS_PRESENT_DST_RECT 2
#define CS_PRESENT_DIRTY_RGN 4
struct wined3d_cs_present
{
    enum wined3d_cs_op opcode;
    HWND dst_window_override;
    struct wined3d_swapchain *swapchain;
    RECT src_rect;
    RECT dst_rect;
    RGNDATA dirty_region;
    DWORD flags;
    DWORD set_data;
};

struct wined3d_cs_clear
{
    enum wined3d_cs_op opcode;
    DWORD rect_count;
    DWORD flags;
    struct wined3d_color color;
    float depth;
    DWORD stencil;
    RECT rects[1];
};

struct wined3d_cs_draw
{
    enum wined3d_cs_op opcode;
    UINT start_idx;
    UINT index_count;
    UINT start_instance;
    UINT instance_count;
    BOOL indexed;
};

struct wined3d_cs_stateblock
{
    enum wined3d_cs_op opcode;
    struct wined3d_state state;
};

struct wined3d_cs_set_render_target
{
    enum wined3d_cs_op opcode;
    UINT render_target_idx;
    struct wined3d_surface *render_target;
};

struct wined3d_cs_set_consts_f
{
    enum wined3d_cs_op opcode;
    UINT start_register, vector4f_count;
    float constants[4];
};

struct wined3d_cs_reset_state
{
    enum wined3d_cs_op opcode;
};

struct wined3d_cs_finish
{
    enum wined3d_cs_op opcode;
};

static CRITICAL_SECTION wined3d_cs_list_mutex;
static CRITICAL_SECTION_DEBUG wined3d_cs_list_mutex_debug =
{
    0, 0, &wined3d_cs_list_mutex,
    {&wined3d_cs_list_mutex_debug.ProcessLocksList,
    &wined3d_cs_list_mutex_debug.ProcessLocksList},
    0, 0, {(DWORD_PTR)(__FILE__ ": wined3d_cs_list_mutex")}
};
static CRITICAL_SECTION wined3d_cs_list_mutex = {&wined3d_cs_list_mutex_debug, -1, 0, 0, 0, 0};

/* FIXME: The list synchronization probably isn't particularly fast. */
static void wined3d_cs_list_enqueue(struct wined3d_cs_list *list, struct wined3d_cs_block *block)
{
    EnterCriticalSection(&wined3d_cs_list_mutex);
    list_add_tail(&list->blocks, &block->entry);
    LeaveCriticalSection(&wined3d_cs_list_mutex);
}

static struct wined3d_cs_block *wined3d_cs_list_dequeue(struct wined3d_cs_list *list)
{
    struct list *head;

    EnterCriticalSection(&wined3d_cs_list_mutex);
    if (!(head = list_head(&list->blocks)))
    {
        LeaveCriticalSection(&wined3d_cs_list_mutex);
        return NULL;
    }
    list_remove(head);
    LeaveCriticalSection(&wined3d_cs_list_mutex);

    return LIST_ENTRY(head, struct wined3d_cs_block, entry);
}

static struct wined3d_cs_block *wined3d_cs_list_dequeue_blocking(struct wined3d_cs_list *list)
{
    struct wined3d_cs_block *block;

    /* FIXME: Use an event to wait after a couple of spins. */
    for (;;)
    {
        if ((block = wined3d_cs_list_dequeue(list)))
            return block;
    }
}

static void wined3d_cs_list_init(struct wined3d_cs_list *list)
{
    list_init(&list->blocks);
}

static struct wined3d_cs_block *wined3d_cs_get_thread_block(const struct wined3d_cs *cs)
{
    return TlsGetValue(cs->tls_idx);
}

static void wined3d_cs_set_thread_block(const struct wined3d_cs *cs, struct wined3d_cs_block *block)
{
    if (!TlsSetValue(cs->tls_idx, block))
        ERR("Failed to set thread block.\n");
}

static void wined3d_cs_flush(struct wined3d_cs *cs)
{
    wined3d_cs_list_enqueue(&cs->exec_list, wined3d_cs_get_thread_block(cs));
    wined3d_cs_set_thread_block(cs, NULL);
}

static struct wined3d_cs_block *wined3d_cs_get_block(struct wined3d_cs *cs)
{
    struct wined3d_cs_block *block;

    if (!(block = wined3d_cs_list_dequeue(&cs->free_list)))
    {
        if (!(block = HeapAlloc(GetProcessHeap(), 0, sizeof(*block))))
        {
            ERR("Failed to get new block.\n");
            return NULL;
        }
    }

    block->pos = 0;

    return block;
}

static UINT wined3d_cs_exec_fence(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_fence *op = data;

    InterlockedExchange(op->signalled, TRUE);

    return sizeof(*op);
}

static void wined3d_cs_emit_fence(struct wined3d_cs *cs, BOOL *signalled)
{
    struct wined3d_cs_fence *op;

    *signalled = FALSE;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_FENCE;
    op->signalled = signalled;
}

static UINT wined3d_cs_exec_present(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_present *op = data;
    struct wined3d_swapchain *swapchain;
    const RECT *src_rect = op->set_data & CS_PRESENT_SRC_RECT ? &op->src_rect : NULL;
    const RECT *dst_rect = op->set_data & CS_PRESENT_DST_RECT ? &op->dst_rect : NULL;
    const RGNDATA *dirty_region = op->set_data & CS_PRESENT_DIRTY_RGN ? &op->dirty_region : NULL;

    swapchain = op->swapchain;
    wined3d_swapchain_set_window(swapchain, op->dst_window_override);

    swapchain->swapchain_ops->swapchain_present(swapchain,
            src_rect, dst_rect, dirty_region, op->flags);

    return sizeof(*op);
}

void wined3d_cs_emit_present(struct wined3d_cs *cs, struct wined3d_swapchain *swapchain,
        const RECT *src_rect, const RECT *dst_rect, HWND dst_window_override,
        const RGNDATA *dirty_region, DWORD flags)
{
    struct wined3d_cs_present *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_PRESENT;
    op->dst_window_override = dst_window_override;
    op->swapchain = swapchain;
    op->set_data = 0;
    if (src_rect)
    {
        op->src_rect = *src_rect;
        op->set_data |= CS_PRESENT_SRC_RECT;
    }
    if (dst_rect)
    {
        op->dst_rect = *dst_rect;
        op->set_data |= CS_PRESENT_DST_RECT;
    }
    if (dirty_region)
    {
        op->dirty_region = *dirty_region;
        op->set_data = CS_PRESENT_DIRTY_RGN;
    }
    op->flags = flags;

    cs->ops->submit(cs);
}

static UINT wined3d_cs_exec_clear(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_clear *op = data;
    struct wined3d_device *device = cs->device;
    const struct wined3d_fb_state *fb = &cs->state.fb;
    RECT draw_rect;
    unsigned int extra_rects = op->rect_count ? op->rect_count - 1 : 0;

    wined3d_get_draw_rect(&cs->state, &draw_rect);
    device_clear_render_targets(device, device->adapter->gl_info.limits.buffers,
            fb, op->rect_count, op->rect_count ? op->rects : NULL, &draw_rect, op->flags,
            &op->color, op->depth, op->stencil);

    return sizeof(*op) + sizeof(*op->rects) * extra_rects;
}

void wined3d_cs_emit_clear(struct wined3d_cs *cs, DWORD rect_count, const RECT *rects,
        DWORD flags, const struct wined3d_color *color, float depth, DWORD stencil)
{
    struct wined3d_cs_clear *op;
    unsigned int extra_rects = rect_count ? rect_count - 1 : 0;

    op = cs->ops->require_space(cs, sizeof(*op) + sizeof(*op->rects) * extra_rects);
    op->opcode = WINED3D_CS_OP_CLEAR;
    op->rect_count = rect_count;
    if (rect_count)
        memcpy(op->rects, rects, rect_count * sizeof(*rects));
    op->flags = flags;
    op->color = *color;
    op->depth = depth;
    op->stencil = stencil;

    cs->ops->submit(cs);
}

static UINT wined3d_cs_exec_draw(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_draw *op = data;

    draw_primitive(cs->device, &cs->state, op->start_idx, op->index_count,
            op->start_instance, op->instance_count, op->indexed);

    return sizeof(*op);
}

void wined3d_cs_emit_draw(struct wined3d_cs *cs, UINT start_idx, UINT index_count,
        UINT start_instance, UINT instance_count, BOOL indexed)
{
    struct wined3d_cs_draw *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_DRAW;
    op->start_idx = start_idx;
    op->index_count = index_count;
    op->start_instance = start_instance;
    op->instance_count = instance_count;
    op->indexed = indexed;

    cs->ops->submit(cs);
}

static UINT wined3d_cs_exec_transfer_stateblock(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_stateblock *op = data;

    /* Don't memcpy the entire struct, we'll remove single items as we add dedicated
     * ops for setting states */
    cs->state.fb.depth_stencil = op->state.fb.depth_stencil;

    cs->state.vertex_declaration = op->state.vertex_declaration;
    memcpy(cs->state.stream_output, op->state.stream_output, sizeof(cs->state.stream_output));
    memcpy(cs->state.streams, op->state.streams, sizeof(cs->state.streams));
    cs->state.index_buffer = op->state.index_buffer;
    cs->state.index_format = op->state.index_format;
    cs->state.base_vertex_index = op->state.base_vertex_index;
    cs->state.load_base_vertex_index = op->state.load_base_vertex_index;
    cs->state.gl_primitive_type = op->state.gl_primitive_type;

    cs->state.vertex_shader = op->state.vertex_shader;
    memcpy(cs->state.vs_cb, op->state.vs_cb, sizeof(cs->state.vs_cb));
    memcpy(cs->state.vs_sampler, op->state.vs_sampler, sizeof(cs->state.vs_sampler));
    memcpy(cs->state.vs_consts_b, op->state.vs_consts_b, sizeof(cs->state.vs_consts_b));
    memcpy(cs->state.vs_consts_i, op->state.vs_consts_i, sizeof(cs->state.vs_consts_i));

    cs->state.geometry_shader = op->state.geometry_shader;
    memcpy(cs->state.gs_cb, op->state.gs_cb, sizeof(cs->state.gs_cb));
    memcpy(cs->state.gs_sampler, op->state.gs_sampler, sizeof(cs->state.gs_sampler));

    cs->state.pixel_shader = op->state.pixel_shader;
    memcpy(cs->state.ps_cb, op->state.ps_cb, sizeof(cs->state.ps_cb));
    memcpy(cs->state.ps_sampler, op->state.ps_sampler, sizeof(cs->state.ps_sampler));
    memcpy(cs->state.ps_consts_b, op->state.ps_consts_b, sizeof(cs->state.ps_consts_b));
    memcpy(cs->state.ps_consts_i, op->state.ps_consts_i, sizeof(cs->state.ps_consts_i));

    memcpy(cs->state.textures, op->state.textures, sizeof(cs->state.textures));
    memcpy(cs->state.sampler_states, op->state.sampler_states, sizeof(cs->state.sampler_states));
    memcpy(cs->state.texture_states, op->state.texture_states, sizeof(cs->state.texture_states));
    cs->state.lowest_disabled_stage = op->state.lowest_disabled_stage;

    memcpy(cs->state.transforms, op->state.transforms, sizeof(cs->state.transforms));
    memcpy(cs->state.clip_planes, op->state.clip_planes, sizeof(cs->state.clip_planes));
    cs->state.material = op->state.material;
    cs->state.viewport = op->state.viewport;
    cs->state.scissor_rect = op->state.scissor_rect;

    memcpy(cs->state.lights, op->state.lights, sizeof(cs->state.lights));

    memcpy(cs->state.render_states, op->state.render_states, sizeof(cs->state.render_states));

    return sizeof(*op);
}

void wined3d_cs_emit_transfer_stateblock(struct wined3d_cs *cs, const struct wined3d_state *state)
{
    struct wined3d_cs_stateblock *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_STATEBLOCK;

    /* Don't memcpy the entire struct, we'll remove single items as we add dedicated
     * ops for setting states */
    op->state.fb.depth_stencil = state->fb.depth_stencil;

    op->state.vertex_declaration = state->vertex_declaration;
    memcpy(op->state.stream_output, state->stream_output, sizeof(op->state.stream_output));
    memcpy(op->state.streams, state->streams, sizeof(op->state.streams));
    op->state.index_buffer = state->index_buffer;
    op->state.index_format = state->index_format;
    op->state.base_vertex_index = state->base_vertex_index;
    op->state.load_base_vertex_index = state->load_base_vertex_index;
    op->state.gl_primitive_type = state->gl_primitive_type;

    op->state.vertex_shader = state->vertex_shader;
    memcpy(op->state.vs_cb, state->vs_cb, sizeof(op->state.vs_cb));
    memcpy(op->state.vs_sampler, state->vs_sampler, sizeof(op->state.vs_sampler));
    memcpy(op->state.vs_consts_b, state->vs_consts_b, sizeof(op->state.vs_consts_b));
    memcpy(op->state.vs_consts_i, state->vs_consts_i, sizeof(op->state.vs_consts_i));

    op->state.geometry_shader = state->geometry_shader;
    memcpy(op->state.gs_cb, state->gs_cb, sizeof(op->state.gs_cb));
    memcpy(op->state.gs_sampler, state->gs_sampler, sizeof(op->state.gs_sampler));

    op->state.pixel_shader = state->pixel_shader;
    memcpy(op->state.ps_cb, state->ps_cb, sizeof(op->state.ps_cb));
    memcpy(op->state.ps_sampler, state->ps_sampler, sizeof(op->state.ps_sampler));
    memcpy(op->state.ps_consts_b, state->ps_consts_b, sizeof(op->state.ps_consts_b));
    memcpy(op->state.ps_consts_i, state->ps_consts_i, sizeof(op->state.ps_consts_i));

    memcpy(op->state.textures, state->textures, sizeof(op->state.textures));
    memcpy(op->state.sampler_states, state->sampler_states, sizeof(op->state.sampler_states));
    memcpy(op->state.texture_states, state->texture_states, sizeof(op->state.texture_states));
    op->state.lowest_disabled_stage = state->lowest_disabled_stage;

    memcpy(op->state.transforms, state->transforms, sizeof(op->state.transforms));
    memcpy(op->state.clip_planes, state->clip_planes, sizeof(op->state.clip_planes));
    op->state.material = state->material;
    op->state.viewport = state->viewport;
    op->state.scissor_rect = state->scissor_rect;

    /* FIXME: This is not ideal. CS is still running synchronously, so this is ok.
     * It will go away soon anyway. */
    memcpy(op->state.lights, state->lights, sizeof(op->state.lights));

    memcpy(op->state.render_states, state->render_states, sizeof(op->state.render_states));

    cs->ops->submit(cs);
}

static UINT wined3d_cs_exec_set_render_target(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_render_target *op = data;

    cs->state.fb.render_targets[op->render_target_idx] = op->render_target;

    device_invalidate_state(cs->device, STATE_FRAMEBUFFER);

    return sizeof(*op);
}

void wined3d_cs_emit_set_render_target(struct wined3d_cs *cs, UINT render_target_idx,
        struct wined3d_surface *render_target)
{
    struct wined3d_cs_set_render_target *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_SET_RENDER_TARGET;
    op->render_target_idx = render_target_idx;
    op->render_target = render_target;

    cs->ops->submit(cs);
}

static UINT wined3d_cs_exec_set_vs_consts_f(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_consts_f *op = data;
    struct wined3d_device *device = cs->device;

    memcpy(cs->state.vs_consts_f + op->start_register * 4, op->constants,
            sizeof(*cs->state.vs_consts_f) * 4 * op->vector4f_count);

    device->shader_backend->shader_update_float_vertex_constants(device,
            op->start_register, op->vector4f_count);

    return sizeof(*op) + sizeof(op->constants) * (op->vector4f_count - 1);
}

static UINT wined3d_cs_exec_set_ps_consts_f(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_set_consts_f *op = data;
    struct wined3d_device *device = cs->device;

    memcpy(cs->state.ps_consts_f + op->start_register * 4, op->constants,
            sizeof(*cs->state.ps_consts_f) * 4 * op->vector4f_count);

    device->shader_backend->shader_update_float_pixel_constants(device,
            op->start_register, op->vector4f_count);

    return sizeof(*op) + sizeof(op->constants) * (op->vector4f_count - 1);
}

void wined3d_cs_emit_set_consts_f(struct wined3d_cs *cs, UINT start_register,
        const float *constants, UINT vector4f_count, enum wined3d_shader_type type)
{
    struct wined3d_cs_set_consts_f *op;
    UINT extra_space = vector4f_count - 1;

    op = cs->ops->require_space(cs, sizeof(*op) + sizeof(op->constants) * extra_space);
    switch (type)
    {
        case WINED3D_SHADER_TYPE_PIXEL:
            op->opcode = WINED3D_CS_OP_SET_PS_CONSTS_F;
            break;

        case WINED3D_SHADER_TYPE_VERTEX:
            op->opcode = WINED3D_CS_OP_SET_VS_CONSTS_F;
            break;

        case WINED3D_SHADER_TYPE_GEOMETRY:
            FIXME("Invalid for geometry shaders\n");
            return;
    }
    op->start_register = start_register;
    op->vector4f_count = vector4f_count;
    memcpy(op->constants, constants, sizeof(*constants) * 4 * vector4f_count);

    cs->ops->submit(cs);
}

static UINT wined3d_cs_exec_reset_state(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_reset_state *op = data;

    state_init_default(&cs->state, cs->device);

    return sizeof(*op);
}

void wined3d_cs_emit_reset_state(struct wined3d_cs *cs)
{
    struct wined3d_cs_reset_state *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_RESET_STATE;

    cs->ops->submit(cs);
}

static UINT wined3d_cs_exec_glfinish(struct wined3d_cs *cs, const void *data)
{
    const struct wined3d_cs_reset_state *op = data;
    struct wined3d_device *device = cs->device;
    struct wined3d_context *context;

    context = context_acquire(device, NULL);
    context->gl_info->gl_ops.gl.p_glFinish();
    context_release(context);

    return sizeof(*op);
}

void wined3d_cs_emit_glfinish(struct wined3d_cs *cs)
{
    struct wined3d_cs_reset_state *op;

    op = cs->ops->require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_GLFINISH;

    cs->ops->submit(cs);
}

static UINT (* const wined3d_cs_op_handlers[])(struct wined3d_cs *cs, const void *data) =
{
    /* WINED3D_CS_OP_FENCE                  */ wined3d_cs_exec_fence,
    /* WINED3D_CS_OP_PRESENT                */ wined3d_cs_exec_present,
    /* WINED3D_CS_OP_CLEAR                  */ wined3d_cs_exec_clear,
    /* WINED3D_CS_OP_DRAW                   */ wined3d_cs_exec_draw,
    /* WINED3D_CS_OP_STATEBLOCK             */ wined3d_cs_exec_transfer_stateblock,
    /* WINED3D_CS_OP_SET_RENDER_TARGET      */ wined3d_cs_exec_set_render_target,
    /* WINED3D_CS_OP_SET_VS_CONSTS_F        */ wined3d_cs_exec_set_vs_consts_f,
    /* WINED3D_CS_OP_SET_PS_CONSTS_F        */ wined3d_cs_exec_set_ps_consts_f,
    /* WINED3D_CS_OP_RESET_STATE            */ wined3d_cs_exec_reset_state,
    /* WINED3D_CS_OP_GLFINISH               */ wined3d_cs_exec_glfinish,
};

static void *wined3d_cs_mt_require_space(struct wined3d_cs *cs, size_t size)
{
    struct wined3d_cs_block *block = wined3d_cs_get_thread_block(cs);
    void *data;

    if (!block || block->pos + size > sizeof(block->data))
    {
        if (block)
            wined3d_cs_flush(cs);
        block = wined3d_cs_get_block(cs);
        wined3d_cs_set_thread_block(cs, block);
    }

    data = &block->data[block->pos];
    block->pos += size;

    return data;
}

/* FIXME: wined3d_device_uninit_3d() should either flush and wait, or be an
 * OP itself. */
static void wined3d_cs_emit_stop(struct wined3d_cs *cs)
{
    struct wined3d_cs_stop *op;

    op = wined3d_cs_mt_require_space(cs, sizeof(*op));
    op->opcode = WINED3D_CS_OP_STOP;

    wined3d_cs_flush(cs);
}

static void wined3d_cs_flush_and_wait(struct wined3d_cs *cs)
{
    BOOL fence;

    wined3d_cs_emit_fence(cs, &fence);
    wined3d_cs_flush(cs);

    /* A busy wait should be fine, we're not supposed to have to wait very
     * long. */
    while (!InterlockedCompareExchange(&fence, TRUE, TRUE));
}

static const struct wined3d_cs_ops wined3d_cs_mt_ops =
{
    wined3d_cs_mt_require_space,
    wined3d_cs_flush_and_wait,
    wined3d_cs_flush_and_wait,
};

static void wined3d_cs_st_submit(struct wined3d_cs *cs)
{
    struct wined3d_cs_block *block = wined3d_cs_get_thread_block(cs);
    UINT pos = 0;

    while (pos < block->pos)
    {
        enum wined3d_cs_op opcode = *(const enum wined3d_cs_op *)&block->data[pos];

        if (opcode >= WINED3D_CS_OP_STOP)
        {
            ERR("Invalid opcode %#x.\n", opcode);
            goto done;
        }

        pos += wined3d_cs_op_handlers[opcode](cs, &block->data[pos]);
    }

done:
    block->pos = 0;
}

static void *wined3d_cs_st_require_space(struct wined3d_cs *cs, size_t size)
{
    struct wined3d_cs_block *block = wined3d_cs_get_thread_block(cs);
    void *data;

    if (!block)
    {
        if (!(block = HeapAlloc(GetProcessHeap(), 0, sizeof(*block))))
        {
            ERR("Failed to get new block.\n");
            return NULL;
        }
        block->pos = 0;
        wined3d_cs_set_thread_block(cs, block);
    }
    else if (block->pos + size > sizeof(block->data))
    {
        wined3d_cs_st_submit(cs);
    }

    data = &block->data[block->pos];
    block->pos += size;

    return data;
}

static const struct wined3d_cs_ops wined3d_cs_st_ops =
{
    wined3d_cs_st_require_space,
    wined3d_cs_st_submit,
    wined3d_cs_st_submit,
};

static DWORD WINAPI wined3d_cs_run(void *thread_param)
{
    struct wined3d_cs *cs = thread_param;

    TRACE("Started.\n");

    for (;;)
    {
        struct wined3d_cs_block *block;
        UINT pos = 0;

        block = wined3d_cs_list_dequeue_blocking(&cs->exec_list);
        while (pos < block->pos)
        {
            enum wined3d_cs_op opcode = *(const enum wined3d_cs_op *)&block->data[pos];

            if (opcode >= WINED3D_CS_OP_STOP)
            {
                if (opcode > WINED3D_CS_OP_STOP)
                    ERR("Invalid opcode %#x.\n", opcode);
                goto done;
            }

            pos += wined3d_cs_op_handlers[opcode](cs, &block->data[pos]);
        }
        wined3d_cs_list_enqueue(&cs->free_list, block);
    }

done:
    TRACE("Stopped.\n");
    return 0;
}

/* We could also create a single thread for all of wined3d, instead of one for
 * each device, at the cost of some extra overhead for each block. I'm not
 * sure that we'd gain anything from that though. */
struct wined3d_cs *wined3d_cs_create(struct wined3d_device *device)
{
    struct wined3d_cs *cs = NULL;
    const struct wined3d_adapter *adapter = device->adapter;
    const struct wined3d_d3d_info *d3d_info = &adapter->d3d_info;
    const struct wined3d_gl_info *gl_info = &adapter->gl_info;
    DWORD ret;

    if (!(cs = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*cs))))
    {
        ERR("Failed to allocate wined3d command stream memory.\n");
        return NULL;
    }

    if ((cs->tls_idx = TlsAlloc()) == TLS_OUT_OF_INDEXES)
    {
        ERR("Failed to allocate cs TLS index, err %#x.\n", GetLastError());
        goto err;
    }

    if (FAILED(state_init(&cs->state, d3d_info, gl_info)))
    {
        ERR("Failed to init state\n");
        goto err;
    }

    if (wined3d_settings.cs_multithreaded)
    {
        wined3d_cs_list_init(&cs->free_list);
        wined3d_cs_list_init(&cs->exec_list);

        if (!(cs->thread = CreateThread(NULL, 0, wined3d_cs_run, cs, 0, &ret)))
        {
            ERR("Failed to create wined3d command stream thread.\n");
            goto err;
        }

        cs->ops = &wined3d_cs_mt_ops;
    }
    else
    {
        cs->ops = &wined3d_cs_st_ops;
    }
    cs->device = device;

    return cs;

err:
    state_cleanup(&cs->state, FALSE);
    if (cs->tls_idx != TLS_OUT_OF_INDEXES && !TlsFree(cs->tls_idx))
        ERR("Failed to free cs TLS index, err %#x.\n", GetLastError());
    HeapFree(GetProcessHeap(), 0, cs);
    return NULL;
}

void wined3d_cs_destroy(struct wined3d_cs *cs)
{
    DWORD ret;

    if (wined3d_settings.cs_multithreaded)
    {
        wined3d_cs_emit_stop(cs);

        ret = WaitForSingleObject(cs->thread, INFINITE);
        CloseHandle(cs->thread);
        if (ret != WAIT_OBJECT_0)
            ERR("Wait failed (%#x).\n", ret);

        /* FIXME: Cleanup the block lists on thread exit. */
#if 0
        wined3d_cs_list_cleanup(&cs->exec_list);
        wined3d_cs_list_cleanup(&cs->free_list);
#endif
    }

    /* The cs does not hold references to its bound, resources because this would delay
     * resource destruction and private data release, until the CS releases its reference,
     * making the CS visible to the client libraries and applications. */
    state_cleanup(&cs->state, FALSE);

    if (!TlsFree(cs->tls_idx))
        ERR("Failed to free cs TLS index, err %#x.\n", GetLastError());

    HeapFree(GetProcessHeap(), 0, cs);
}
