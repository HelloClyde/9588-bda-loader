/* SPDX-License-Identifier: Apache-2.0 */
#include "bda_sdk.h"

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define VX_HEADER_SIZE 24u
#define SCREEN_VX_SIZE (VX_HEADER_SIZE + SCREEN_WIDTH * SCREEN_HEIGHT * 2u)
#define PRESENT_STRIP_HEIGHT 80u
#define PRESENT_STRIP_VX_SIZE \
    (VX_HEADER_SIZE + SCREEN_WIDTH * PRESENT_STRIP_HEIGHT * 2u)

#define MAX_APPS 96u
#define PATH_CAPACITY 560u
#define TITLE_CAPACITY 17u
#define GRID_COLUMNS 3u
#define GRID_ROWS 3u
#define APPS_PER_PAGE (GRID_COLUMNS * GRID_ROWS)
#define ICON_SLOT_SIZE 0x8000u
#define ICON_SELECTED_SLOT_OFFSET 0x4000u
#define ICON_WIDTH 54
#define ICON_HEIGHT 54
#define ICON_PIXEL_SIZE (ICON_WIDTH * ICON_HEIGHT * 2u)
#define ICON_VX_SIZE (VX_HEADER_SIZE + ICON_PIXEL_SIZE)
#define ICON_SELECTED_WIDTH 58
#define ICON_SELECTED_HEIGHT 58
#define ICON_SELECTED_PIXEL_SIZE \
    (ICON_SELECTED_WIDTH * ICON_SELECTED_HEIGHT * 2u)
#define ICON_SELECTED_VX_SIZE \
    (VX_HEADER_SIZE + ICON_SELECTED_PIXEL_SIZE)
#define GRID_ICON_SIZE 54
#define VX_OPAQUE_BLACK_RGB565 0x0001u

#define BDA_HEADER_SIZE 0x88u
#define BDA_XOR_KEY 0x44525744u
#define BDA_CHECKSUM_XOR_KEY 0x322d464bu
#define BDA_MAGIC 0x004b4242u
#define BDA_WORD04 0x5d245562u

#define HEADER_HEIGHT 26
#define GRID_TOP HEADER_HEIGHT
#define GRID_CELL_WIDTH 80
#define GRID_CELL_HEIGHT 80
#define GRID_BOTTOM (GRID_TOP + (int)GRID_ROWS * GRID_CELL_HEIGHT)
#define PAGE_BAR_TOP GRID_BOTTOM
#define PAGE_BAR_HEIGHT 18
#define TAB_TOP (PAGE_BAR_TOP + PAGE_BAR_HEIGHT)
#define TAB_COLUMNS 5
#define TAB_ROWS 2
#define TAB_WIDTH 48
#define TAB_HEIGHT 18
#define BDA_GUI_INPUT_STATE_QUERY 0x72cu
#define INPUT_GUARD_MAX_TICKS 12u
#define TOUCH_KEY_GUARD_TICKS 8u
#define PACKET_ESCAPE_HOLD_TICKS 8u
#define SYSTEM_FONT_DATA_OFFSET 0x1a84b0u
#define SYSTEM_FONT_GLYPH_SIZE 24u
#define SYSTEM_FONT_GBK_GLYPH_COUNT 23940u
#define SYSTEM_FONT_CACHE_SIZE \
    (SYSTEM_FONT_GLYPH_SIZE * SYSTEM_FONT_GBK_GLYPH_COUNT)
#define SYSTEM_FONT_REQUIRED_SIZE \
    (SYSTEM_FONT_DATA_OFFSET + SYSTEM_FONT_CACHE_SIZE)
#define SYSTEM_FONT_READ_CHUNK 0x8000u

#define SYSTEM_FONT_LOAD_OK 1
#define SYSTEM_FONT_LOAD_MISSING 0
#define SYSTEM_FONT_LOAD_INVALID (-1)
#define SYSTEM_FONT_LOAD_NO_MEMORY (-2)

typedef struct loader_app {
    char path[PATH_CAPACITY];
    char title[TITLE_CAPACITY];
    u32 category;
    u32 icon_offset;
    u32 icon_size;
    u32 selected_icon_offset;
    u32 selected_icon_size;
} loader_app_t;

/*
 * Every BDA is loaded at 0x81c00020. Launching a second BDA therefore
 * overwrites this loader. The assembly entry records the stack and return
 * state supplied by the firmware. The loader borrows the outer menu frame in
 * saved s3 for drawing instead of registering a second top-level frame. This
 * keeps the menu handle alive across a long-running chained application.
 *
 * A BDA cannot synchronously re-enter the path loader while it still occupies
 * the fixed 0x81c00020 image. Instead, the loader patches the outer firmware
 * path-loader frame's saved caller return address to a small heap trampoline
 * and returns normally. Only after the outer loader has completed all of its
 * post-BDA cleanup does the trampoline call the firmware path loader again
 * with the new path and the original menu context. The target may then safely
 * overwrite this image. When the target eventually returns, the trampoline
 * frees itself and tail-returns to the original menu call site.
 *
 * The deferred trampoline uses only caller-saved registers. Once the target
 * returns, it tail-calls MEM_FREE with the original menu continuation already
 * in ra. This balances the menu timer exactly once, frees the trampoline, and
 * never returns to code in this overwritten image.
 */
volatile u32 g_bda_loader_entry_sp __attribute__((used));
volatile u32 g_bda_loader_entry_ra __attribute__((used));
volatile u32 g_bda_loader_entry_gp __attribute__((used));
volatile u32 g_bda_loader_entry_s0 __attribute__((used));
volatile u32 g_bda_loader_entry_s1 __attribute__((used));
volatile u32 g_bda_loader_entry_s2 __attribute__((used));
volatile u32 g_bda_loader_entry_s3 __attribute__((used));
volatile u32 g_bda_loader_entry_s4 __attribute__((used));
volatile u32 g_bda_loader_entry_s5 __attribute__((used));
volatile u32 g_bda_loader_entry_s6 __attribute__((used));
volatile u32 g_bda_loader_entry_s7 __attribute__((used));
volatile u32 g_bda_loader_entry_fp __attribute__((used));

int bda_loader_main(void);

__asm__(
    ".section .text.bda_main,\"ax\"\n"
    ".set noreorder\n"
    ".globl bda_main\n"
    ".type bda_main,@function\n"
    "bda_main:\n"
    "lui $t0,%hi(g_bda_loader_entry_sp)\n"
    "sw $sp,%lo(g_bda_loader_entry_sp)($t0)\n"
    "lui $t0,%hi(g_bda_loader_entry_ra)\n"
    "sw $ra,%lo(g_bda_loader_entry_ra)($t0)\n"
    "lui $t0,%hi(g_bda_loader_entry_gp)\n"
    "sw $gp,%lo(g_bda_loader_entry_gp)($t0)\n"
    "lui $t0,%hi(g_bda_loader_entry_s0)\n"
    "sw $s0,%lo(g_bda_loader_entry_s0)($t0)\n"
    "lui $t0,%hi(g_bda_loader_entry_s1)\n"
    "sw $s1,%lo(g_bda_loader_entry_s1)($t0)\n"
    "lui $t0,%hi(g_bda_loader_entry_s2)\n"
    "sw $s2,%lo(g_bda_loader_entry_s2)($t0)\n"
    "lui $t0,%hi(g_bda_loader_entry_s3)\n"
    "sw $s3,%lo(g_bda_loader_entry_s3)($t0)\n"
    "lui $t0,%hi(g_bda_loader_entry_s4)\n"
    "sw $s4,%lo(g_bda_loader_entry_s4)($t0)\n"
    "lui $t0,%hi(g_bda_loader_entry_s5)\n"
    "sw $s5,%lo(g_bda_loader_entry_s5)($t0)\n"
    "lui $t0,%hi(g_bda_loader_entry_s6)\n"
    "sw $s6,%lo(g_bda_loader_entry_s6)($t0)\n"
    "lui $t0,%hi(g_bda_loader_entry_s7)\n"
    "sw $s7,%lo(g_bda_loader_entry_s7)($t0)\n"
    "lui $t0,%hi(g_bda_loader_entry_fp)\n"
    "sw $fp,%lo(g_bda_loader_entry_fp)($t0)\n"
    "lui $t9,%hi(bda_loader_main)\n"
    "addiu $t9,$t9,%lo(bda_loader_main)\n"
    "jr $t9\n"
    "nop\n"
    ".size bda_main,.-bda_main\n"
    ".set reorder\n"
);

static const char k_bda_pattern[] =
    "A:\\\xd3\xa6\xd3\xc3\\\xb3\xcc\xd0\xf2\\*.bda";
static const char k_bda_directory[] =
    "A:\\\xd3\xa6\xd3\xc3\\\xb3\xcc\xd0\xf2\\";
static const char k_loader_title[] =
    "\x42\x44\x41\x20\xc6\xf4\xb6\xaf\xc6\xf7";
static const char k_no_apps[] =
    "\xc3\xbb\xd3\xd0\xd5\xd2\xb5\xbd\xbf\xc9\xd3\xc3\xb5\xc4\x20"
    "\x42\x44\x41";
static const char k_system_font_path[] =
    "A:\\\xcf\xb5\xcd\xb3\\\xca\xfd\xbe\xdd\\HZK_LIB.BIN";
static const char k_system_font_missing[] =
    "\xce\xb4\xd5\xd2\xb5\xbd\xcf\xb5\xcd\xb3\xd7\xd6\xbf\xe2\xa3\xac"
    "\xc7\xeb\xbc\xec\xb2\xe9\x20HZK_LIB.BIN";
static const char k_system_font_invalid[] =
    "\xcf\xb5\xcd\xb3\xd7\xd6\xbf\xe2\xb6\xc1\xc8\xa1\xca\xa7\xb0\xdc";
static const char k_system_font_no_memory[] =
    "\xcf\xb5\xcd\xb3\xd7\xd6\xbf\xe2\xc4\xda\xb4\xe6\xb2\xbb\xd7\xe3";
static const char k_loading_text[] = "LOADING BDA...";

static const char *const k_category_tabs[10] = {
    "\xc6\xe4\xcb\xfb",
    "\xcc\xfd\xcb\xb5",
    "\xd3\xef\xb7\xa8",
    "\xd4\xc4\xb6\xc1",
    "\xd3\xce\xcf\xb7",
    "\xbf\xbc\xca\xd4",
    "\xb1\xb3\xcb\xd0",
    "\xb4\xca\xb5\xe4",
    "\xd3\xe9\xc0\xd6",
    "\xb9\xa4\xbe\xdf",
};

static loader_app_t *g_apps;
static u8 *g_screen_vx;
static u8 *g_present_vx;
static u8 *g_icon_slots;
static u8 *g_system_font;
static u32 g_app_count;
static u32 g_current_category;
static u32 g_selected_ordinal;
static u32 g_previous_keys;
static bda_handle_t g_frame;
static bda_handle_t g_draw;
static bda_handle_t g_draw_owner;
static bda_handle_t g_back;
static void *g_draw_object;
static volatile int g_detached;
static volatile int g_dirty;
static int g_dirty_top;
static int g_dirty_bottom;
static int g_render_clip_top;
static int g_render_clip_bottom;
static int g_selected_icon_valid;
static int g_touch_contact_down;
static int g_touch_moved;
static int g_touch_handled_on_down;
static int g_touch_down_x;
static int g_touch_down_y;
static u32 g_touch_key_guard_ticks;
static int g_touch_key_resync_pending;
static u32 g_escape_packet_hold_ticks;
static u32 g_input_startup_guard_ticks;
static int g_loading;
static char g_footer_line[48];
static char g_launch_path[PATH_CAPACITY];

#ifdef BDA_LOADER_DIAGNOSTIC
static const char k_trace_path_a[] = "A:\\BDALOAD.LOG";
static const char k_trace_path_root[] = "\\BDALOAD.LOG";
static const char *g_trace_path;
static int g_trace_batch_file;
static char g_trace_line[128];
static u32 g_trace_present_count;
static u32 g_trace_window_count;
static u32 g_diag_findfirst_ms;
static u32 g_diag_findnext_ms;
static u32 g_diag_header_open_ms;
static u32 g_diag_header_read_seek_ms;
static u32 g_diag_header_close_ms;
static u32 g_diag_header_decode_ms;
static u32 g_diag_header_total_ms;
static u32 g_diag_header_max_ms;
static u32 g_diag_header_count;
static u32 g_diag_header_valid_count;
static u32 g_diag_sort_ms;
static u32 g_diag_scan_ms;
static u32 g_diag_icon_total_ms;
static u32 g_diag_icon_max_ms;
static u32 g_diag_icon_count;
static u32 g_diag_cpu_icon_composed;
static u32 g_diag_cpu_icon_transparent;
static u32 g_diag_selected_icon_load_count;
static u32 g_diag_selected_icon_load_total_ms;
static u32 g_diag_selected_icon_load_max_ms;
static char g_diag_header_slowest[PATH_CAPACITY];
#endif

static u32 read_u32_le(const u8 *data) {
    return (u32)data[0]
        | ((u32)data[1] << 8)
        | ((u32)data[2] << 16)
        | ((u32)data[3] << 24);
}

static void write_u16_le(u8 *data, u16 value) {
    data[0] = (u8)value;
    data[1] = (u8)(value >> 8);
}

static void write_u32_le(u8 *data, u32 value) {
    data[0] = (u8)value;
    data[1] = (u8)(value >> 8);
    data[2] = (u8)(value >> 16);
    data[3] = (u8)(value >> 24);
}

static u16 rgb565(u32 red, u32 green, u32 blue) {
    return (u16)(
        ((red & 0xf8u) << 8)
        | ((green & 0xfcu) << 3)
        | (blue >> 3)
    );
}

static u32 string_length(const char *text) {
    u32 length = 0;
    while (text[length]) {
        ++length;
    }
    return length;
}

static void copy_string(char *out, u32 capacity, const char *text) {
    u32 index = 0;

    if (!capacity) {
        return;
    }
    while (text[index] && index + 1u < capacity) {
        out[index] = text[index];
        ++index;
    }
    out[index] = 0;
}

static char *append_text(char *out, char *end, const char *text) {
    while (*text && out < end) {
        *out++ = *text++;
    }
    return out;
}

static char *append_u32(char *out, char *end, u32 value) {
    char digits[10];
    u32 count = 0;

    if (!value) {
        if (out < end) {
            *out++ = '0';
        }
        return out;
    }
    while (value && count < (u32)sizeof(digits)) {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (count && out < end) {
        *out++ = digits[--count];
    }
    return out;
}

#ifdef BDA_LOADER_DIAGNOSTIC
static int trace_open(const char *mode) {
    int file;

    if (g_trace_path) {
        return bda_fs_fopen_raw(g_trace_path, mode);
    }
    file = bda_fs_fopen_raw(k_trace_path_a, mode);
    if (bda_fs_file_is_valid(file)) {
        g_trace_path = k_trace_path_a;
        return file;
    }
    file = bda_fs_fopen_raw(k_trace_path_root, mode);
    if (bda_fs_file_is_valid(file)) {
        g_trace_path = k_trace_path_root;
    }
    return file;
}

static void trace_reset(void) {
    int file;

    if (bda_fs_file_is_valid(g_trace_batch_file)) {
        (void)bda_fs_close_raw(g_trace_batch_file);
    }
    g_trace_batch_file = 0;
    g_trace_path = 0;
    file = trace_open("wb");
    if (bda_fs_file_is_valid(file)) {
        (void)bda_fs_close_raw(file);
    }
}

static void trace_write_line(char *out) {
    char *end = g_trace_line + sizeof(g_trace_line) - 1u;
    int file;
    int close_after_write;

    out = append_text(out, end, "\r\n");
    *out = 0;
    close_after_write = !bda_fs_file_is_valid(g_trace_batch_file);
    file = close_after_write ? trace_open("ab") : g_trace_batch_file;
    if (!bda_fs_file_is_valid(file)) {
        return;
    }
    (void)bda_fs_write_raw(file, g_trace_line, (u32)(out - g_trace_line));
    if (close_after_write) {
        (void)bda_fs_close_raw(file);
    }
}

static void trace_batch_begin(void) {
    if (!bda_fs_file_is_valid(g_trace_batch_file)) {
        g_trace_batch_file = trace_open("ab");
    }
}

static void trace_batch_end(void) {
    if (bda_fs_file_is_valid(g_trace_batch_file)) {
        (void)bda_fs_close_raw(g_trace_batch_file);
    }
    g_trace_batch_file = 0;
}

static void trace_text(const char *text) {
    trace_write_line(append_text(
        g_trace_line,
        g_trace_line + sizeof(g_trace_line) - 1u,
        text
    ));
}

static void trace_value(const char *label, u32 value) {
    char *out = g_trace_line;
    char *end = g_trace_line + sizeof(g_trace_line) - 1u;

    out = append_text(out, end, label);
    out = append_u32(out, end, value);
    trace_write_line(out);
}

static void trace_labeled_text(const char *label, const char *value) {
    char *out = g_trace_line;
    char *end = g_trace_line + sizeof(g_trace_line) - 1u;

    out = append_text(out, end, label);
    out = append_text(out, end, value);
    trace_write_line(out);
}

static void trace_two_values(
    const char *left_label,
    u32 left_value,
    const char *right_label,
    u32 right_value
) {
    char *out = g_trace_line;
    char *end = g_trace_line + sizeof(g_trace_line) - 1u;

    out = append_text(out, end, left_label);
    out = append_u32(out, end, left_value);
    out = append_text(out, end, " ");
    out = append_text(out, end, right_label);
    out = append_u32(out, end, right_value);
    trace_write_line(out);
}

static void diag_finish_header(const char *path, u32 start, int valid) {
    u32 elapsed = bda_gui_millisecond_elapsed(
        start, bda_gui_millisecond_count()
    );

    ++g_diag_header_count;
    if (valid) {
        ++g_diag_header_valid_count;
    }
    g_diag_header_total_ms += elapsed;
    if (elapsed > g_diag_header_max_ms) {
        g_diag_header_max_ms = elapsed;
        copy_string(
            g_diag_header_slowest,
            sizeof(g_diag_header_slowest),
            path
        );
    }
}

#define TRACE_RESET() trace_reset()
#define TRACE_TEXT(text) trace_text(text)
#define TRACE_VALUE(label, value) trace_value((label), (u32)(value))
#define TRACE_LABELED_TEXT(label, value) trace_labeled_text((label), (value))
#define TRACE_TWO_VALUES(a, b, c, d) trace_two_values((a), (u32)(b), (c), (u32)(d))
#define TRACE_BATCH_BEGIN() trace_batch_begin()
#define TRACE_BATCH_END() trace_batch_end()
#else
#define TRACE_RESET() ((void)0)
#define TRACE_TEXT(text) ((void)0)
#define TRACE_VALUE(label, value) ((void)0)
#define TRACE_LABELED_TEXT(label, value) ((void)0)
#define TRACE_TWO_VALUES(a, b, c, d) ((void)0)
#define TRACE_BATCH_BEGIN() ((void)0)
#define TRACE_BATCH_END() ((void)0)
#endif

static const char *path_basename(const char *path) {
    const char *base = path;

    while (*path) {
        if (*path == '\\' || *path == '/') {
            base = path + 1;
        }
        ++path;
    }
    return base;
}

static int path_has_drive(const char *path) {
    return path[0] && path[1] == ':';
}

static int build_path(char *out, u32 capacity, const char *found_name) {
    u32 length;
    const char *base = path_basename(found_name);

    if (path_has_drive(found_name)) {
        if (string_length(found_name) + 1u > capacity) {
            return 0;
        }
        copy_string(out, capacity, found_name);
        return 1;
    }

    length = string_length(k_bda_directory);
    if (length + string_length(base) + 1u > capacity) {
        return 0;
    }
    copy_string(out, capacity, k_bda_directory);
    copy_string(out + length, capacity - length, base);
    return 1;
}

static int header_checksum_ok(const u8 *header) {
    u32 sum = 0;
    u32 offset;

    for (offset = 0; offset < 0x2cu; offset += 4u) {
        u32 decoded = read_u32_le(header + offset) ^ BDA_XOR_KEY;
        sum += decoded & 0xffu;
        sum += (decoded >> 8) & 0xffu;
        sum += (decoded >> 16) & 0xffu;
        sum += (decoded >> 24) & 0xffu;
    }
    for (offset = 0x2cu; offset < 0x84u; ++offset) {
        sum += header[offset];
    }
    return (read_u32_le(header + 0x84u) ^ BDA_CHECKSUM_XOR_KEY) == sum;
}

static void copy_fallback_title(
    char out[TITLE_CAPACITY],
    const char *path
) {
    const char *base = path_basename(path);
    u32 length = string_length(base);
    u32 index;

    if (length >= 4u
        && (base[length - 4u] == '.')
        && ((base[length - 3u] | 0x20) == 'b')
        && ((base[length - 2u] | 0x20) == 'd')
        && ((base[length - 1u] | 0x20) == 'a')) {
        length -= 4u;
    }
    if (length >= TITLE_CAPACITY) {
        length = TITLE_CAPACITY - 1u;
    }
    for (index = 0; index < length; ++index) {
        out[index] = base[index];
    }
    if (length && (u8)out[length - 1u] >= 0x81u) {
        --length;
    }
    out[length] = 0;
}

static int read_app_metadata(const char *path, loader_app_t *app) {
    u8 header[BDA_HEADER_SIZE];
    u32 words[11];
    u32 icon_offset;
    u32 icon_size;
    u32 selected_icon_offset;
    u32 selected_icon_size;
    u32 index;
    int file;
    int file_size;
    int read_count;
#ifdef BDA_LOADER_DIAGNOSTIC
    u32 total_start = bda_gui_millisecond_count();
    u32 phase_start = total_start;
    u32 phase_end;
#endif

    file = bda_fs_fopen_raw(path, "rb");
#ifdef BDA_LOADER_DIAGNOSTIC
    phase_end = bda_gui_millisecond_count();
    g_diag_header_open_ms += bda_gui_millisecond_elapsed(
        phase_start, phase_end
    );
#endif
    if (!bda_fs_file_is_valid(file)) {
#ifdef BDA_LOADER_DIAGNOSTIC
        diag_finish_header(path, total_start, 0);
#endif
        return 0;
    }
#ifdef BDA_LOADER_DIAGNOSTIC
    phase_start = phase_end;
#endif
    read_count = bda_fs_read_raw(file, header, sizeof(header));
    file_size = bda_fs_seek_raw(file, 0, BDA_SEEK_END);
#ifdef BDA_LOADER_DIAGNOSTIC
    phase_end = bda_gui_millisecond_count();
    g_diag_header_read_seek_ms += bda_gui_millisecond_elapsed(
        phase_start, phase_end
    );
    phase_start = phase_end;
#endif
    (void)bda_fs_close_raw(file);
#ifdef BDA_LOADER_DIAGNOSTIC
    phase_end = bda_gui_millisecond_count();
    g_diag_header_close_ms += bda_gui_millisecond_elapsed(
        phase_start, phase_end
    );
    phase_start = phase_end;
#endif
    if (read_count != (int)sizeof(header) || file_size < (int)sizeof(header)) {
#ifdef BDA_LOADER_DIAGNOSTIC
        g_diag_header_decode_ms += bda_gui_millisecond_elapsed(
            phase_start, bda_gui_millisecond_count()
        );
        diag_finish_header(path, total_start, 0);
#endif
        return 0;
    }

    for (index = 0; index < 11u; ++index) {
        words[index] = read_u32_le(header + index * 4u) ^ BDA_XOR_KEY;
    }
    if (words[0] != BDA_MAGIC
        || words[1] != BDA_WORD04
        || (words[2] & 0xffffu) < 0x0102u
        || (words[3] & 0xffffu) >= 10u
        || words[4] + 4u != (u32)file_size
        || words[5] >= (u32)file_size
        || !header_checksum_ok(header)) {
#ifdef BDA_LOADER_DIAGNOSTIC
        g_diag_header_decode_ms += bda_gui_millisecond_elapsed(
            phase_start, bda_gui_millisecond_count()
        );
        diag_finish_header(path, total_start, 0);
#endif
        return 0;
    }

    icon_offset = 0;
    icon_size = 0;
    selected_icon_offset = 0;
    selected_icon_size = 0;
    if (words[6] <= (u32)file_size
        && words[7] <= (u32)file_size - words[6]
        && words[8]
            <= (u32)file_size - words[6] - words[7]) {
        u32 candidate_offset = words[6] + words[7] + words[8];

        if (words[9] >= VX_HEADER_SIZE
            && candidate_offset < (u32)file_size
            && words[9] <= (u32)file_size - candidate_offset) {
            icon_offset = candidate_offset;
            icon_size = words[9];
            selected_icon_offset = icon_offset + icon_size;
            if (words[10] >= VX_HEADER_SIZE
                && selected_icon_offset < (u32)file_size
                && words[10]
                    <= (u32)file_size - selected_icon_offset) {
                selected_icon_size = words[10];
            } else {
                selected_icon_offset = 0;
            }
        }
    }

    copy_string(app->path, sizeof(app->path), path);
    app->category = words[3] & 0xffffu;
    app->icon_offset = icon_offset;
    app->icon_size = icon_size;
    app->selected_icon_offset = selected_icon_offset;
    app->selected_icon_size = selected_icon_size;
    for (index = 0; index < TITLE_CAPACITY - 1u; ++index) {
        app->title[index] = (char)header[0x2cu + index];
        if (!app->title[index]) {
            break;
        }
    }
    app->title[TITLE_CAPACITY - 1u] = 0;
    if (!app->title[0]) {
        copy_fallback_title(app->title, path);
    }
#ifdef BDA_LOADER_DIAGNOSTIC
    g_diag_header_decode_ms += bda_gui_millisecond_elapsed(
        phase_start, bda_gui_millisecond_count()
    );
    diag_finish_header(path, total_start, 1);
#endif
    return 1;
}

static int title_compare(const char *left, const char *right) {
    while (*left && *right && (u8)*left == (u8)*right) {
        ++left;
        ++right;
    }
    return (int)(u8)*left - (int)(u8)*right;
}

static u32 category_sort_key(u32 category) {
    return category ? category : 10u;
}

static int app_compare(const loader_app_t *left, const loader_app_t *right) {
    u32 left_category = category_sort_key(left->category);
    u32 right_category = category_sort_key(right->category);

    if (left_category != right_category) {
        return left_category < right_category ? -1 : 1;
    }
    return title_compare(left->title, right->title);
}

static void sort_apps(void) {
    u32 index;

    for (index = 1; index < g_app_count; ++index) {
        loader_app_t item;
        u32 position = index;

        (void)bda_memcpy(&item, &g_apps[index], sizeof(item));
        while (position && app_compare(&item, &g_apps[position - 1u]) < 0) {
            (void)bda_memcpy(
                &g_apps[position],
                &g_apps[position - 1u],
                sizeof(loader_app_t)
            );
            --position;
        }
        (void)bda_memcpy(
            &g_apps[position],
            &item,
            sizeof(loader_app_t)
        );
    }
}

static int scan_apps(void) {
    bda_fs_find_data_t find_data;
    int result;
    int first_result;
    int opened;
#ifdef BDA_LOADER_DIAGNOSTIC
    u32 scan_start;
    u32 phase_start;
    u32 phase_end;
#endif

    TRACE_TEXT("SCAN_ALLOC_BEGIN");
    g_apps = (loader_app_t *)bda_alloc(sizeof(loader_app_t) * MAX_APPS);
    TRACE_VALUE("SCAN_ALLOC_PTR=", g_apps);
    if (!g_apps || (u32)g_apps == 0xffffffffu) {
        g_apps = 0;
        return 0;
    }
    bda_memset(g_apps, 0, sizeof(loader_app_t) * MAX_APPS);
    bda_fs_find_data_init(&find_data);
    TRACE_TEXT("FINDFIRST_BEGIN");
#ifdef BDA_LOADER_DIAGNOSTIC
    g_diag_header_slowest[0] = 0;
    bda_gui_millisecond_timer_start();
    scan_start = bda_gui_millisecond_count();
    phase_start = scan_start;
#endif
    result = bda_fs_findfirst(k_bda_pattern, 0x27u, &find_data);
    first_result = result;
#ifdef BDA_LOADER_DIAGNOSTIC
    phase_end = bda_gui_millisecond_count();
    g_diag_findfirst_ms = bda_gui_millisecond_elapsed(
        phase_start, phase_end
    );
#endif
    opened = result != -1;
    while (result != -1 && g_app_count < MAX_APPS) {
        loader_app_t candidate;

        find_data.name_or_path[sizeof(find_data.name_or_path) - 1u] = 0;
        bda_memset(&candidate, 0, sizeof(candidate));
        if (build_path(
                candidate.path,
                sizeof(candidate.path),
                find_data.name_or_path
            )
            && read_app_metadata(candidate.path, &candidate)) {
            (void)bda_memcpy(
                &g_apps[g_app_count],
                &candidate,
                sizeof(candidate)
            );
            ++g_app_count;
        }
#ifdef BDA_LOADER_DIAGNOSTIC
        phase_start = bda_gui_millisecond_count();
#endif
        result = bda_fs_findnext(&find_data);
#ifdef BDA_LOADER_DIAGNOSTIC
        phase_end = bda_gui_millisecond_count();
        g_diag_findnext_ms += bda_gui_millisecond_elapsed(
            phase_start, phase_end
        );
#endif
    }
    if (opened) {
        (void)bda_fs_findclose(&find_data);
    }
#ifdef BDA_LOADER_DIAGNOSTIC
    phase_start = bda_gui_millisecond_count();
#endif
    sort_apps();
#ifdef BDA_LOADER_DIAGNOSTIC
    phase_end = bda_gui_millisecond_count();
    g_diag_sort_ms = bda_gui_millisecond_elapsed(phase_start, phase_end);
    g_diag_scan_ms = bda_gui_millisecond_elapsed(scan_start, phase_end);
    bda_gui_millisecond_timer_stop();
#endif
    TRACE_BATCH_BEGIN();
    TRACE_VALUE("FINDFIRST_RESULT=", first_result);
    TRACE_VALUE("SCAN_COUNT=", g_app_count);
    TRACE_VALUE("SCAN_TOTAL_MS=", g_diag_scan_ms);
    TRACE_VALUE("FIND_FIRST_MS=", g_diag_findfirst_ms);
    TRACE_VALUE("FIND_NEXT_TOTAL_MS=", g_diag_findnext_ms);
    TRACE_VALUE("HEADER_COUNT=", g_diag_header_count);
    TRACE_VALUE("HEADER_VALID_COUNT=", g_diag_header_valid_count);
    TRACE_VALUE("HEADER_OPEN_TOTAL_MS=", g_diag_header_open_ms);
    TRACE_VALUE("HEADER_READ_SEEK_TOTAL_MS=", g_diag_header_read_seek_ms);
    TRACE_VALUE("HEADER_CLOSE_TOTAL_MS=", g_diag_header_close_ms);
    TRACE_VALUE("HEADER_DECODE_TOTAL_MS=", g_diag_header_decode_ms);
    TRACE_VALUE("HEADER_TOTAL_MS=", g_diag_header_total_ms);
    TRACE_VALUE("HEADER_MAX_MS=", g_diag_header_max_ms);
    TRACE_LABELED_TEXT("HEADER_SLOWEST=", g_diag_header_slowest);
    TRACE_VALUE("SORT_MS=", g_diag_sort_ms);
    TRACE_BATCH_END();
    return g_app_count != 0u;
}

static u32 category_count(u32 category) {
    u32 count = 0;
    u32 index;

    for (index = 0; index < g_app_count; ++index) {
        if (g_apps[index].category == category) {
            ++count;
        }
    }
    return count;
}

static int app_index_at(u32 category, u32 ordinal) {
    u32 index;

    for (index = 0; index < g_app_count; ++index) {
        if (g_apps[index].category == category) {
            if (!ordinal) {
                return (int)index;
            }
            --ordinal;
        }
    }
    return -1;
}

static void select_first_category(void) {
    u32 category;

    for (category = 1; category <= 9u; ++category) {
        if (category_count(category)) {
            g_current_category = category;
            return;
        }
    }
    g_current_category = 0;
}

static void mark_full_dirty(void) {
    g_dirty = 1;
    g_dirty_top = 0;
    g_dirty_bottom = SCREEN_HEIGHT;
}

static void mark_selection_dirty(u32 old_ordinal, u32 new_ordinal) {
    u32 old_page = old_ordinal / APPS_PER_PAGE;
    u32 new_page = new_ordinal / APPS_PER_PAGE;
    int old_row;
    int new_row;
    int top;
    int bottom;

    if (old_page != new_page) {
        mark_full_dirty();
        return;
    }
    old_row = (int)(old_ordinal % APPS_PER_PAGE) / (int)GRID_COLUMNS;
    new_row = (int)(new_ordinal % APPS_PER_PAGE) / (int)GRID_COLUMNS;
    top = GRID_TOP
        + (old_row < new_row ? old_row : new_row) * GRID_CELL_HEIGHT;
    bottom = GRID_TOP
        + (old_row > new_row ? old_row + 1 : new_row + 1)
            * GRID_CELL_HEIGHT;
    if (!g_dirty) {
        g_dirty_top = top;
        g_dirty_bottom = bottom;
    } else {
        if (top < g_dirty_top) {
            g_dirty_top = top;
        }
        if (bottom > g_dirty_bottom) {
            g_dirty_bottom = bottom;
        }
    }
    g_dirty = 1;
}

static void select_category(u32 category) {
    TRACE_TWO_VALUES(
        "CATEGORY_TOUCH=", category,
        "CATEGORY_COUNT=", category_count(category)
    );
    if (category > 9u
        || !category_count(category)
        || category == g_current_category) {
        return;
    }
    g_current_category = category;
    g_selected_ordinal = 0;
    mark_full_dirty();
}

static void change_selection(int direction) {
    u32 count = category_count(g_current_category);
    u32 old_ordinal = g_selected_ordinal;
    int next;

    if (!count) {
        return;
    }
    next = (int)g_selected_ordinal + direction;
    while (next < 0) {
        next += (int)count;
    }
    while (next >= (int)count) {
        next -= (int)count;
    }
    g_selected_ordinal = (u32)next;
    mark_selection_dirty(old_ordinal, g_selected_ordinal);
}

static void change_page(int direction) {
    u32 count = category_count(g_current_category);
    u32 pages = (count + APPS_PER_PAGE - 1u) / APPS_PER_PAGE;
    u32 page;

    if (pages <= 1u) {
        return;
    }
    page = g_selected_ordinal / APPS_PER_PAGE;
    if (direction > 0) {
        page = (page + 1u) % pages;
    } else if (!page) {
        page = pages - 1u;
    } else {
        --page;
    }
    g_selected_ordinal = page * APPS_PER_PAGE;
    if (g_selected_ordinal >= count) {
        g_selected_ordinal = count - 1u;
    }
    mark_full_dirty();
}

static void init_vx_header(u8 *vx, u32 width, u32 height) {
    u32 index;

    bda_memset(vx, 0, VX_HEADER_SIZE);
    vx[0] = 'V';
    vx[1] = 'X';
    for (index = 2; index < 6u; ++index) {
        vx[index] = 0xccu;
    }
    write_u32_le(vx + 6u, width);
    write_u32_le(vx + 10u, height);
    for (index = 14; index < 20u; ++index) {
        vx[index] = 0xccu;
    }
    for (index = 20; index < VX_HEADER_SIZE; ++index) {
        vx[index] = 0xffu;
    }
}

static void screen_put_pixel(int x, int y, u16 color) {
    u32 offset;

    if (x < 0 || x >= SCREEN_WIDTH
        || y < 0 || y >= SCREEN_HEIGHT
        || y < g_render_clip_top || y >= g_render_clip_bottom) {
        return;
    }
    offset = VX_HEADER_SIZE + (u32)(y * SCREEN_WIDTH + x) * 2u;
    write_u16_le(g_screen_vx + offset, color);
}

static void screen_fill_rect(
    int left,
    int top,
    int width,
    int height,
    u16 color
) {
    int x;
    int y;
    int bottom = top + height;

    if (top < g_render_clip_top) {
        top = g_render_clip_top;
    }
    if (bottom > g_render_clip_bottom) {
        bottom = g_render_clip_bottom;
    }

    for (y = top; y < bottom; ++y) {
        for (x = left; x < left + width; ++x) {
            screen_put_pixel(x, y, color);
        }
    }
}

static void screen_frame_rect(
    int left,
    int top,
    int width,
    int height,
    u16 color
) {
    int index;

    for (index = 0; index < width; ++index) {
        screen_put_pixel(left + index, top, color);
        screen_put_pixel(left + index, top + height - 1, color);
    }
    for (index = 0; index < height; ++index) {
        screen_put_pixel(left, top + index, color);
        screen_put_pixel(left + width - 1, top + index, color);
    }
}

static void make_fallback_icon(u8 *icon) {
    int x;
    int y;
    u16 background = rgb565(34, 57, 72);
    u16 accent = rgb565(48, 205, 190);
    u16 foreground = rgb565(238, 246, 244);

    init_vx_header(icon, ICON_WIDTH, ICON_HEIGHT);
    for (y = 0; y < ICON_HEIGHT; ++y) {
        for (x = 0; x < ICON_WIDTH; ++x) {
            u16 color = background;
            int border = x < 2 || y < 2
                || x >= ICON_WIDTH - 2 || y >= ICON_HEIGHT - 2;
            int stem = x >= 25 && x <= 28 && y >= 35 && y <= 39;
            int dot = x >= 25 && x <= 28 && y >= 44 && y <= 47;
            int hook = (
                y >= 12 && y <= 32
                && x >= 16 && x <= 37
                && (y <= 16 || x >= 33 || (y >= 27 && x >= 24))
            );

            if (border) {
                color = accent;
            } else if (stem || dot || hook) {
                color = foreground;
            }
            write_u16_le(
                icon + VX_HEADER_SIZE
                    + (u32)(y * ICON_WIDTH + x) * 2u,
                color
            );
        }
    }
}

static int vx_resource_valid(
    const u8 *vx,
    u32 size,
    u32 width,
    u32 height
) {
    return size >= VX_HEADER_SIZE + width * height * 2u
        && vx[0] == 'V'
        && vx[1] == 'X'
        && read_u32_le(vx + 6u) == width
        && read_u32_le(vx + 10u) == height;
}

static int selected_menu_resource_valid(const u8 *resource, u32 size) {
    u32 file_size;
    u32 pixel_offset;
    u32 dib_size;
    u32 width;
    u32 height;
    u32 row_bytes;
    u32 pixel_bytes;
    u32 planes;
    u32 bits_per_pixel;
    u32 compression;

    if (vx_resource_valid(
            resource,
            size,
            ICON_SELECTED_WIDTH,
            ICON_SELECTED_HEIGHT
        )) {
        return 1;
    }

    /*
     * The stock C200 menu accepts both VX and BMP menu resources through
     * 0x800e49e8.  At least one factory BDA (飞天音乐) uses a 58x58,
     * uncompressed 24-bit BMP as its fourth/selected icon.
     */
    if (size < 54u || resource[0] != 'B' || resource[1] != 'M') {
        return 0;
    }
    file_size = read_u32_le(resource + 2u);
    pixel_offset = read_u32_le(resource + 10u);
    dib_size = read_u32_le(resource + 14u);
    width = read_u32_le(resource + 18u);
    height = read_u32_le(resource + 22u);
    planes = (u32)resource[26] | ((u32)resource[27] << 8);
    bits_per_pixel = (u32)resource[28] | ((u32)resource[29] << 8);
    compression = read_u32_le(resource + 30u);

    if (file_size < 54u
        || file_size > size
        || pixel_offset < 54u
        || pixel_offset > file_size
        || dib_size < 40u
        || width != ICON_SELECTED_WIDTH
        || (height != ICON_SELECTED_HEIGHT
            && height != (u32)(0u - ICON_SELECTED_HEIGHT))
        || planes != 1u
        || bits_per_pixel != 24u
        || compression != 0u) {
        return 0;
    }

    row_bytes = (ICON_SELECTED_WIDTH * 3u + 3u) & ~3u;
    pixel_bytes = row_bytes * ICON_SELECTED_HEIGHT;
    return pixel_bytes <= file_size - pixel_offset;
}

static const u8 *load_icon(int app_index) {
    u8 *icon = g_icon_slots;
    loader_app_t *app;
    int file;
    int count;
#ifdef BDA_LOADER_DIAGNOSTIC
    u32 start;
    u32 elapsed;
    int new_max;
#endif

    if (app_index < 0 || !icon) {
        return 0;
    }
#ifdef BDA_LOADER_DIAGNOSTIC
    bda_gui_millisecond_timer_start();
    start = bda_gui_millisecond_count();
#endif
    make_fallback_icon(icon);
    app = &g_apps[app_index];
    if (!app->icon_offset
        || app->icon_size > ICON_SELECTED_SLOT_OFFSET
        || app->icon_size < ICON_VX_SIZE) {
        goto finish;
    }

    file = bda_fs_fopen_raw(app->path, "rb");
    if (!bda_fs_file_is_valid(file)) {
        goto finish;
    }
    if (bda_fs_seek_raw(file, (s32)app->icon_offset, BDA_SEEK_SET)
        != (int)app->icon_offset) {
        (void)bda_fs_close_raw(file);
        goto finish;
    }
    count = bda_fs_read_raw(file, icon, app->icon_size);
    (void)bda_fs_close_raw(file);
    if (count != (int)app->icon_size
        || !vx_resource_valid(
            icon, app->icon_size, ICON_WIDTH, ICON_HEIGHT
        )) {
        make_fallback_icon(icon);
    }
finish:
#ifdef BDA_LOADER_DIAGNOSTIC
    elapsed = bda_gui_millisecond_elapsed(
        start, bda_gui_millisecond_count()
    );
    bda_gui_millisecond_timer_stop();
    ++g_diag_icon_count;
    g_diag_icon_total_ms += elapsed;
    new_max = elapsed > g_diag_icon_max_ms;
    if (elapsed > g_diag_icon_max_ms) {
        g_diag_icon_max_ms = elapsed;
    }
    if (g_diag_icon_count <= 8u || new_max) {
        TRACE_TWO_VALUES(
            "ICON_INDEX=", app_index,
            "ICON_LOAD_MS=", elapsed
        );
    }
#endif
    return icon;
}

static const u8 *load_selected_icon(int app_index) {
    u8 *icon;
    loader_app_t *app;
    int file;
    int count;
#ifdef BDA_LOADER_DIAGNOSTIC
    u32 start;
    u32 elapsed;
#endif

    if (app_index < 0 || !g_icon_slots) {
        return 0;
    }
    icon = g_icon_slots + ICON_SELECTED_SLOT_OFFSET;
    g_selected_icon_valid = 0;
#ifdef BDA_LOADER_DIAGNOSTIC
    bda_gui_millisecond_timer_start();
    start = bda_gui_millisecond_count();
#endif
    app = &g_apps[app_index];
    if (!app->selected_icon_offset
        || app->selected_icon_size
            > ICON_SLOT_SIZE - ICON_SELECTED_SLOT_OFFSET
        || app->selected_icon_size < 54u) {
        goto finish;
    }
    file = bda_fs_fopen_raw(app->path, "rb");
    if (!bda_fs_file_is_valid(file)) {
        goto finish;
    }
    if (bda_fs_seek_raw(
            file,
            (s32)app->selected_icon_offset,
            BDA_SEEK_SET
        ) != (int)app->selected_icon_offset) {
        (void)bda_fs_close_raw(file);
        goto finish;
    }
    count = bda_fs_read_raw(file, icon, app->selected_icon_size);
    (void)bda_fs_close_raw(file);
    if (count == (int)app->selected_icon_size
        && selected_menu_resource_valid(
            icon,
            app->selected_icon_size
        )) {
        g_selected_icon_valid = 1;
    }
finish:
#ifdef BDA_LOADER_DIAGNOSTIC
    elapsed = bda_gui_millisecond_elapsed(
        start, bda_gui_millisecond_count()
    );
    bda_gui_millisecond_timer_stop();
    ++g_diag_selected_icon_load_count;
    g_diag_selected_icon_load_total_ms += elapsed;
    if (elapsed > g_diag_selected_icon_load_max_ms) {
        g_diag_selected_icon_load_max_ms = elapsed;
    }
    TRACE_TWO_VALUES(
        "SELECTED_ICON_INDEX=", app_index,
        " SELECTED_ICON_LOAD_MS=", elapsed
    );
#endif
    return g_selected_icon_valid ? icon : 0;
}

static const u8 *visible_icon_vx(int app_index) {
    return load_icon(app_index);
}

static u32 screen_blit_menu_resource(
    int left,
    int top,
    const u8 *resource,
    u32 resource_size,
    int width,
    int height,
    u16 background
) {
    u32 opaque_count = 0;
    u32 bmp_pixel_offset = 0;
    u32 bmp_row_bytes = 0;
    int is_vx;
    int bmp_bottom_up = 0;
    int y;

    /*
     * V26 deliberately does not call any firmware image compositor. Every
     * destination pixel is written in Loader-owned RAM: exact RGB565 0xf81f
     * becomes the current card background and every other value is copied
     * unchanged. This guarantees that no pixel from a previous icon slot can
     * survive through transparent areas.
     */
    is_vx = vx_resource_valid(
        resource,
        resource_size,
        (u32)width,
        (u32)height
    );
    if (!is_vx) {
        if (!selected_menu_resource_valid(resource, resource_size)
            || resource[0] != 'B'
            || resource[1] != 'M') {
            return 0;
        }
        bmp_pixel_offset = read_u32_le(resource + 10u);
        bmp_row_bytes = ((u32)width * 3u + 3u) & ~3u;
        bmp_bottom_up =
            read_u32_le(resource + 22u) == (u32)height;
    }

    for (y = 0; y < height; ++y) {
        int screen_y = top + y;
        const u8 *source_row;
        u8 *destination_row;
        int first_x = 0;
        int end_x = width;
        int x;

        if (screen_y < g_render_clip_top
            || screen_y >= g_render_clip_bottom
            || screen_y < 0
            || screen_y >= SCREEN_HEIGHT) {
            continue;
        }
        if (left < 0) {
            first_x = -left;
        }
        if (left + end_x > SCREEN_WIDTH) {
            end_x = SCREEN_WIDTH - left;
        }
        if (first_x >= end_x) {
            continue;
        }
        if (is_vx) {
            source_row = resource + VX_HEADER_SIZE
                + (u32)y * (u32)width * 2u;
        } else {
            u32 source_y = bmp_bottom_up
                ? (u32)(height - 1 - y)
                : (u32)y;

            source_row = resource + bmp_pixel_offset
                + source_y * bmp_row_bytes;
        }
        destination_row = g_screen_vx + VX_HEADER_SIZE
            + (u32)(screen_y * SCREEN_WIDTH + left + first_x) * 2u;
        for (x = first_x; x < end_x; ++x) {
            u16 color;

            if (is_vx) {
                const u8 *source = source_row + (u32)x * 2u;

                color = (u16)source[0] | ((u16)source[1] << 8);
            } else {
                const u8 *source = source_row + (u32)x * 3u;

                color = rgb565(source[2], source[1], source[0]);
            }
            if (color == BDA_GUI_COLOR_KEY_MAGENTA_RGB565) {
                color = background;
            } else {
                if (color == 0u) {
                    /*
                     * C200's VX drawing path treats RGB565 0x0000 as
                     * transparent. 0x0001 is visually black but forces an
                     * actual write.
                     */
                    color = VX_OPAQUE_BLACK_RGB565;
                }
                ++opaque_count;
            }
            write_u16_le(
                destination_row + (u32)(x - first_x) * 2u,
                color
            );
        }
    }
    return opaque_count;
}

static void draw_icon_placeholder(int left, int top, u16 color) {
    screen_frame_rect(
        left + 2,
        top + 2,
        GRID_ICON_SIZE - 4,
        GRID_ICON_SIZE - 4,
        color
    );
    screen_frame_rect(
        left + (GRID_ICON_SIZE - 12) / 2,
        top + (GRID_ICON_SIZE - 12) / 2,
        12,
        12,
        color
    );
}

static void draw_left_arrow(int center_x, int center_y, u16 color) {
    int row;

    for (row = -9; row <= 9; ++row) {
        int width = 10 - (row < 0 ? -row : row);
        int column;
        for (column = 0; column < width; ++column) {
            screen_put_pixel(center_x - column, center_y + row, color);
        }
    }
}

static void draw_right_arrow(int center_x, int center_y, u16 color) {
    int row;

    for (row = -9; row <= 9; ++row) {
        int width = 10 - (row < 0 ? -row : row);
        int column;
        for (column = 0; column < width; ++column) {
            screen_put_pixel(center_x + column, center_y + row, color);
        }
    }
}

static void draw_up_arrow(int center_x, int center_y, u16 color) {
    int row;

    for (row = 0; row < 8; ++row) {
        int column;
        for (column = -row; column <= row; ++column) {
            screen_put_pixel(center_x + column, center_y + row, color);
        }
    }
}

static void draw_down_arrow(int center_x, int center_y, u16 color) {
    int row;

    for (row = 0; row < 8; ++row) {
        int width = 7 - row;
        int column;
        for (column = -width; column <= width; ++column) {
            screen_put_pixel(center_x + column, center_y + row, color);
        }
    }
}

static const u8 k_small_ascii_font[36][7] = {
    {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e},
    {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e},
    {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f},
    {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e},
    {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02},
    {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e},
    {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e},
    {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e},
    {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e},
    {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
    {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e},
    {0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f},
    {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e},
    {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f},
    {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10},
    {0x0f, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0f},
    {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11},
    {0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e},
    {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e},
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f},
    {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11},
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
    {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10},
    {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d},
    {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11},
    {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e},
    {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04},
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a},
    {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11},
    {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04},
    {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f}
};

static int small_ascii_glyph_index(u8 character) {
    if (character >= '0' && character <= '9') {
        return (int)(character - '0');
    }
    if (character >= 'a' && character <= 'z') {
        character = (u8)(character - ('a' - 'A'));
    }
    if (character >= 'A' && character <= 'Z') {
        return 10 + (int)(character - 'A');
    }
    return -1;
}

static int system_font_gbk_index(u16 key, u32 *out_index) {
    u32 high = (u32)(key >> 8);
    u32 low = (u32)(key & 0xffu);
    u32 index;

    if (high < 0x81u || high > 0xfeu
        || low < 0x40u || low > 0xfeu || low == 0x7fu) {
        return 0;
    }
    index = high * 190u + low
        - (low < 0x80u ? 0x5ffeu : 0x5fffu);
    if (index >= SYSTEM_FONT_GBK_GLYPH_COUNT) {
        return 0;
    }
    *out_index = index;
    return 1;
}

static const u8 *small_gbk_glyph(u16 key) {
    u32 index;

    if (!g_system_font || !system_font_gbk_index(key, &index)) {
        return 0;
    }
    return g_system_font + index * SYSTEM_FONT_GLYPH_SIZE;
}

static int load_system_font(void) {
    int file;
    int file_size;
    int read_count;
    u32 total = 0;
    u8 *font;

    file = bda_fs_fopen_raw(k_system_font_path, "rb");
    if (!bda_fs_file_is_valid(file)) {
        return SYSTEM_FONT_LOAD_MISSING;
    }
    file_size = bda_fs_seek_raw(file, 0, BDA_SEEK_END);
    TRACE_VALUE("SYSTEM_FONT_FILE_SIZE=", file_size);
    if (file_size < (int)SYSTEM_FONT_REQUIRED_SIZE) {
        (void)bda_fs_close_raw(file);
        return SYSTEM_FONT_LOAD_INVALID;
    }
    font = (u8 *)bda_alloc(SYSTEM_FONT_CACHE_SIZE);
    if (!font || (u32)font == 0xffffffffu) {
        (void)bda_fs_close_raw(file);
        return SYSTEM_FONT_LOAD_NO_MEMORY;
    }
    if (bda_fs_seek_raw(
            file, (s32)SYSTEM_FONT_DATA_OFFSET, BDA_SEEK_SET
        ) != (int)SYSTEM_FONT_DATA_OFFSET) {
        (void)bda_fs_close_raw(file);
        bda_free(font);
        return SYSTEM_FONT_LOAD_INVALID;
    }
    while (total < SYSTEM_FONT_CACHE_SIZE) {
        u32 remaining = SYSTEM_FONT_CACHE_SIZE - total;
        u32 chunk = remaining < SYSTEM_FONT_READ_CHUNK
            ? remaining
            : SYSTEM_FONT_READ_CHUNK;

        read_count = bda_fs_read_raw(file, font + total, chunk);
        if (read_count <= 0 || (u32)read_count > chunk) {
            (void)bda_fs_close_raw(file);
            bda_free(font);
            return SYSTEM_FONT_LOAD_INVALID;
        }
        total += (u32)read_count;
    }
    (void)bda_fs_close_raw(file);
    g_system_font = font;
    return SYSTEM_FONT_LOAD_OK;
}

static void screen_draw_small_ascii(
    int x,
    int y,
    u8 character,
    u16 color
) {
    int glyph = small_ascii_glyph_index(character);
    int row;
    int column;

    if (glyph >= 0) {
        for (row = 0; row < 7; ++row) {
            u8 bits = k_small_ascii_font[glyph][row];

            for (column = 0; column < 5; ++column) {
                if (bits & (1u << (4 - column))) {
                    screen_put_pixel(x + column, y + row + 2, color);
                }
            }
        }
    } else if (character == '-' || character == '_') {
        for (column = 0; column < 5; ++column) {
            screen_put_pixel(
                x + column,
                y + (character == '-' ? 5 : 9),
                color
            );
        }
    } else if (character != ' ') {
        screen_frame_rect(x, y + 2, 5, 7, color);
    }
}

static void screen_draw_small_gbk(
    int x,
    int y,
    u16 key,
    u16 color
) {
    const u8 *glyph = small_gbk_glyph(key);
    int row;
    int column;

    if (!glyph) {
        screen_frame_rect(x + 1, y, 10, 12, color);
        return;
    }
    for (row = 0; row < 12; ++row) {
        u16 bits = (u16)(
            ((u16)glyph[row * 2] << 8)
            | glyph[row * 2 + 1]
        );

        for (column = 0; column < 12; ++column) {
            if (bits & (0x8000u >> column)) {
                screen_put_pixel(x + column, y + row, color);
            }
        }
    }
}

static void screen_draw_small_text(
    int x,
    int y,
    const char *text,
    u16 color
) {
    u32 index = 0;

    while (text[index]) {
        if ((u8)text[index] >= 0x80u && text[index + 1u]) {
            u16 key = (u16)(
                ((u16)(u8)text[index] << 8)
                | (u8)text[index + 1u]
            );

            screen_draw_small_gbk(x, y, key, color);
            x += 12;
            index += 2u;
        } else {
            screen_draw_small_ascii(x, y, (u8)text[index], color);
            x += 6;
            ++index;
        }
    }
}

static int text_pixel_width(const char *text) {
    int width = 0;
    u32 index = 0;

    while (text[index]) {
        if ((u8)text[index] >= 0x80u && text[index + 1u]) {
            width += 12;
            index += 2u;
        } else {
            width += 6;
            ++index;
        }
    }
    return width;
}

static void compact_grid_title(
    char *out,
    u32 capacity,
    const char *text
) {
    u32 input_index = 0;
    u32 output_index = 0;
    int width = 0;

    if (!capacity) {
        return;
    }
    while (text[input_index] && output_index + 1u < capacity) {
        u32 bytes = 1u;
        int character_width = 6;

        if ((u8)text[input_index] >= 0x80u && text[input_index + 1u]) {
            bytes = 2u;
            character_width = 12;
        }
        if (width + character_width > 66
            || output_index + bytes + 1u > capacity) {
            break;
        }
        out[output_index++] = text[input_index++];
        if (bytes == 2u) {
            out[output_index++] = text[input_index++];
        }
        width += character_width;
    }
    out[output_index] = 0;
}

static void build_text_lines(void) {
    char *out;
    char *end;
    u32 count = category_count(g_current_category);
    u32 pages = (count + APPS_PER_PAGE - 1u) / APPS_PER_PAGE;
    u32 page = g_selected_ordinal / APPS_PER_PAGE + 1u;

    out = g_footer_line;
    end = g_footer_line + sizeof(g_footer_line) - 1u;
    out = append_u32(out, end, page);
    out = append_text(out, end, "/");
    out = append_u32(out, end, pages);
    *out = 0;
}

static void draw_header_brand(u16 accent, u16 violet) {
    int row;
    int column;

    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 3; ++column) {
            u16 color = (
                (row == 0 && column == 2)
                || (row == 2 && column == 0)
            ) ? violet : accent;
            screen_fill_rect(
                7 + column * 6,
                6 + row * 6,
                4,
                4,
                color
            );
        }
    }
}

static void render_loading_background(void) {
    u16 black = rgb565(8, 8, 10);
    u16 card = rgb565(10, 13, 18);
    u16 border = rgb565(42, 51, 62);
    u16 accent = rgb565(42, 224, 207);
    u16 violet = rgb565(140, 80, 240);
    int row;
    int column;

    screen_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, black);
    screen_fill_rect(0, 0, SCREEN_WIDTH, 3, accent);
    for (row = 0; row < 3; ++row) {
        for (column = 0; column < 3; ++column) {
            int left = 80 + column * 28;
            int top = 76 + row * 28;
            u16 fill = (
                (row == 0 && column == 2)
                || (row == 2 && column == 0)
            ) ? violet : card;

            screen_fill_rect(left, top, 22, 22, fill);
            screen_frame_rect(left, top, 22, 22, border);
        }
    }
    draw_right_arrow(120, 110, accent);
    screen_frame_rect(40, 218, 160, 8, border);
    screen_fill_rect(43, 221, 74, 2, accent);
}

static void render_background(void) {
    u16 black = rgb565(8, 8, 10);
    u16 header = rgb565(6, 9, 13);
    u16 card = rgb565(9, 13, 18);
    u16 selected = rgb565(7, 30, 34);
    u16 border = rgb565(36, 44, 54);
    u16 muted_border = rgb565(18, 23, 29);
    u16 accent = rgb565(42, 224, 207);
    u16 violet = rgb565(140, 80, 240);
    u16 tab_fill = rgb565(12, 17, 23);
    u16 foreground = rgb565(239, 246, 244);
    u16 muted = rgb565(118, 128, 140);
    u16 black_text = VX_OPAQUE_BLACK_RGB565;
    u32 page_start;
    u32 slot;
    u32 category;

    if (g_loading) {
        render_loading_background();
        return;
    }

    page_start = (
        g_selected_ordinal / APPS_PER_PAGE
    ) * APPS_PER_PAGE;
    screen_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, black);
    screen_fill_rect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, header);
    screen_fill_rect(0, HEADER_HEIGHT - 2, SCREEN_WIDTH, 2, accent);
    draw_header_brand(accent, violet);
    screen_draw_small_text(213, 7, "V35", muted);

    for (slot = 0; slot < APPS_PER_PAGE; ++slot) {
        u32 ordinal = page_start + slot;
        int app_index = app_index_at(g_current_category, ordinal);
        int column = (int)(slot % GRID_COLUMNS);
        int row = (int)(slot / GRID_COLUMNS);
        int left = column * GRID_CELL_WIDTH;
        int top = GRID_TOP + row * GRID_CELL_HEIGHT;
        int card_left = left + 3;
        int card_top = top + 3;
        int card_width = GRID_CELL_WIDTH - 6;
        int card_height = GRID_CELL_HEIGHT - 6;
        int is_selected = ordinal == g_selected_ordinal
            && app_index >= 0;

        screen_fill_rect(
            card_left,
            card_top,
            card_width,
            card_height,
            is_selected ? selected : card
        );
        if (is_selected) {
            screen_frame_rect(
                card_left,
                card_top,
                card_width,
                card_height,
                accent
            );
        }
        if (app_index >= 0) {
            int icon_left = left + (GRID_CELL_WIDTH - GRID_ICON_SIZE) / 2;
            int icon_top = top + 5;
            int icon_width = ICON_WIDTH;
            int icon_height = ICON_HEIGHT;
            u32 resource_size = ICON_VX_SIZE;
            u16 icon_background = is_selected ? selected : card;
            const u8 *resource = 0;
            char title[TITLE_CAPACITY];
            int title_width;

            /*
             * With no persistent cache, do not touch the filesystem for
             * cells outside the current dirty strip.
             */
            if (icon_top < g_render_clip_bottom
                && icon_top + ICON_SELECTED_HEIGHT
                    > g_render_clip_top) {
                resource = visible_icon_vx(app_index);
                if (resource && is_selected) {
                    const u8 *selected_resource =
                        load_selected_icon(app_index);

                    if (selected_resource) {
                        resource = selected_resource;
                        resource_size =
                            g_apps[app_index].selected_icon_size;
                        icon_width = ICON_SELECTED_WIDTH;
                        icon_height = ICON_SELECTED_HEIGHT;
                        icon_left -= 2;
                    }
                }
                screen_fill_rect(
                    icon_left,
                    icon_top,
                    icon_width,
                    icon_height,
                    icon_background
                );
                if (resource) {
                    u32 opaque_count = screen_blit_menu_resource(
                        icon_left,
                        icon_top,
                        resource,
                        resource_size,
                        icon_width,
                        icon_height,
                        icon_background
                    );
#ifdef BDA_LOADER_DIAGNOSTIC
                    ++g_diag_cpu_icon_composed;
                    g_diag_cpu_icon_transparent +=
                        (u32)(icon_width * icon_height)
                        - opaque_count;
#else
                    (void)opaque_count;
#endif
                } else {
                    draw_icon_placeholder(
                        icon_left,
                        icon_top,
                        is_selected ? accent : border
                    );
                }
            }
            compact_grid_title(
                title,
                sizeof(title),
                g_apps[app_index].title
            );
            title_width = text_pixel_width(title);
            screen_draw_small_text(
                left + (GRID_CELL_WIDTH - title_width) / 2,
                top + 62,
                title,
                is_selected ? accent : foreground
            );
        }
    }
    screen_fill_rect(
        0,
        PAGE_BAR_TOP,
        SCREEN_WIDTH,
        TAB_TOP - PAGE_BAR_TOP,
        header
    );
    draw_up_arrow(91, PAGE_BAR_TOP + 5, accent);
    draw_down_arrow(149, PAGE_BAR_TOP + 5, accent);
    screen_draw_small_text(
        (SCREEN_WIDTH - text_pixel_width(g_footer_line)) / 2,
        PAGE_BAR_TOP + 3,
        g_footer_line,
        muted
    );

    screen_fill_rect(
        0,
        TAB_TOP,
        SCREEN_WIDTH,
        SCREEN_HEIGHT - TAB_TOP,
        black
    );
    for (category = 0; category < 10u; ++category) {
        int column = (int)(category % TAB_COLUMNS);
        int row = (int)(category / TAB_COLUMNS);
        int left = column * TAB_WIDTH;
        int top = TAB_TOP + row * TAB_HEIGHT;
        int available = category_count(category) != 0u;
        int active = category == g_current_category;

        screen_fill_rect(
            left + 1,
            top + 1,
            TAB_WIDTH - 2,
            TAB_HEIGHT - 2,
            active ? accent : (available ? tab_fill : black)
        );
        screen_frame_rect(
            left + 1,
            top + 1,
            TAB_WIDTH - 2,
            TAB_HEIGHT - 2,
            active ? accent : (available ? border : muted_border)
        );
        screen_draw_small_text(
            left + (TAB_WIDTH - text_pixel_width(
                k_category_tabs[category]
            )) / 2,
            top + 3,
            k_category_tabs[category],
            active ? black_text : (available ? foreground : muted)
        );
    }
}

static int draw_screen_vx_to_back(int top, int bottom, u32 *strip_count) {
    int y;

    *strip_count = 0;
    if (top == 0 && bottom == SCREEN_HEIGHT) {
        *strip_count = 1;
        return bda_gui_draw_vx(g_back, 0, 0, g_screen_vx);
    }
    for (y = top; y < bottom; y += (int)PRESENT_STRIP_HEIGHT) {
        int height = bottom - y;
        u32 pixel_bytes;
        int result;

        if (height > (int)PRESENT_STRIP_HEIGHT) {
            height = (int)PRESENT_STRIP_HEIGHT;
        }
        init_vx_header(g_present_vx, SCREEN_WIDTH, (u32)height);
        pixel_bytes = (u32)height * SCREEN_WIDTH * 2u;
        (void)bda_memcpy(
            g_present_vx + VX_HEADER_SIZE,
            g_screen_vx + VX_HEADER_SIZE
                + (u32)y * SCREEN_WIDTH * 2u,
            pixel_bytes
        );
        result = bda_gui_draw_vx(g_back, 0, y, g_present_vx);
        ++*strip_count;
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

static int present_screen(void) {
    void *old_object;
    u32 foreground;
    u32 accent;
    u32 strip_count;
    u32 start_tick;
    u32 end_tick;
    int top;
    int bottom;
    int height;
    int draw_result;
    int copy_result;

    if (!g_draw || !g_back || !g_draw_object || !g_dirty) {
        return 0;
    }
    top = g_dirty_top;
    bottom = g_dirty_bottom;
    if (top < 0) {
        top = 0;
    }
    if (bottom > SCREEN_HEIGHT) {
        bottom = SCREEN_HEIGHT;
    }
    if (top >= bottom) {
        g_dirty = 0;
        return 0;
    }
    height = bottom - top;
    start_tick = bda_gui_tick_count_25ms();
#ifdef BDA_LOADER_DIAGNOSTIC
    ++g_trace_present_count;
    g_diag_cpu_icon_composed = 0;
    g_diag_cpu_icon_transparent = 0;
#endif
    g_dirty = 0;
    if (!g_loading) {
        build_text_lines();
    }
    g_render_clip_top = top;
    g_render_clip_bottom = bottom;
    render_background();

    foreground = (u32)bda_gui_rgb(g_back, 239, 246, 244);
    accent = (u32)bda_gui_rgb(g_back, 75, 226, 211);

    old_object = bda_gui_select_draw_object(g_back, g_draw_object);
    draw_result = draw_screen_vx_to_back(top, bottom, &strip_count);
#ifdef BDA_LOADER_DIAGNOSTIC
    if (g_trace_present_count <= 4u) {
        TRACE_TWO_VALUES(
            "PRESENT_VX_HEIGHT=", height,
            " PRESENT_VX_STRIPS=", strip_count
        );
    }
#endif
    if (draw_result != 0) {
        (void)bda_gui_select_draw_object(g_back, old_object);
        mark_full_dirty();
        TRACE_VALUE("DRAW_SCREEN_VX_FAILED=", draw_result);
        return 0;
    }
#ifdef BDA_LOADER_DIAGNOSTIC
    if (g_trace_present_count <= 4u) {
        TRACE_TWO_VALUES(
            "PRESENT_CPU_ICON_COMPOSED=", g_diag_cpu_icon_composed,
            " TRANSPARENT_PIXELS=", g_diag_cpu_icon_transparent
        );
    }
#endif
    (void)bda_gui_set_text_mode(g_back, 1u);
    if (g_loading) {
        if (top < 246 && bottom > 172) {
            int loading_width = text_pixel_width(k_loading_text);

            (void)bda_gui_set_text_color(g_back, foreground);
            (void)bda_gui_draw_text(
                g_back,
                (SCREEN_WIDTH - 60) / 2,
                172,
                "BDA LOADER",
                -1
            );
            (void)bda_gui_set_text_color(g_back, accent);
            (void)bda_gui_draw_text(
                g_back,
                (SCREEN_WIDTH - loading_width) / 2,
                194,
                k_loading_text,
                -1
            );
        }
    } else if (top < HEADER_HEIGHT) {
        (void)bda_gui_set_text_color(g_back, foreground);
        (void)bda_gui_draw_text(g_back, 29, 6, "BDA LOADER", -1);
    }

    (void)bda_gui_select_draw_object(g_back, old_object);

    (void)bda_gui_draw_guard_begin();
    old_object = bda_gui_select_draw_object(g_draw, g_draw_object);
    copy_result = bda_gui_context_copy(
        g_back, 0, top, SCREEN_WIDTH, height,
        g_draw, 0, top, BDA_GUI_COLOR_KEY_BLACK_RGB565
    );
    (void)bda_gui_select_draw_object(g_draw, old_object);
    (void)bda_gui_draw_guard_end();
    end_tick = bda_gui_tick_count_25ms();
#ifdef BDA_LOADER_DIAGNOSTIC
    if (g_trace_present_count <= 2u) {
        TRACE_TWO_VALUES(
            "PRESENT=", g_trace_present_count,
            "PRESENT_MS=", bda_gui_tick_elapsed_ms(start_tick, end_tick)
        );
    }
#endif
    if (copy_result != 0) {
        mark_full_dirty();
        TRACE_VALUE("CONTEXT_COPY_FAILED=", copy_result);
        return 0;
    }
    return 1;
}

static void release_draw_context(void) {
    bda_handle_t draw = g_draw;

    if (!draw || (s32)draw == -1) {
        g_draw = 0;
        g_draw_owner = 0;
        return;
    }
    g_draw = 0;
    g_draw_owner = 0;
    bda_gui_end_draw(draw);
}

static int acquire_draw_context(bda_handle_t owner) {
    if (g_draw && g_draw_owner == owner) {
        return 1;
    }
    release_draw_context();
    g_draw = bda_gui_current_draw(owner);
    if (!g_draw || (s32)g_draw == -1) {
        g_draw = 0;
        return 0;
    }
    g_draw_owner = owner;
    return 1;
}

static int loader_window_proc(
    bda_handle_t handle,
    u32 message,
    u32 wparam,
    u32 lparam
) {
#ifdef BDA_LOADER_DIAGNOSTIC
    int trace_this_message = g_trace_window_count < 16u;

    if (trace_this_message) {
        ++g_trace_window_count;
        TRACE_VALUE("WND_MESSAGE=", message);
    }
#endif
    if (message == BDA_MSG_DRAW_CONTEXT_ATTACH) {
        int acquire_result;

        TRACE_TEXT("WND_ATTACH_BEGIN");
        g_frame = handle;
        acquire_result = acquire_draw_context(handle);
        TRACE_VALUE("WND_ATTACH_ACQUIRE=", acquire_result);
        TRACE_VALUE("WND_ATTACH_DRAW=", g_draw);
        if (!g_draw_object) {
            TRACE_TEXT("WND_DRAW_OBJECT_BEGIN");
            g_draw_object = bda_gui_draw_object_create(7u);
            TRACE_VALUE("WND_DRAW_OBJECT=", g_draw_object);
        }
        mark_full_dirty();
        TRACE_TEXT("WND_ATTACH_DONE");
    } else if (message == BDA_MSG_REDRAW_INPUT) {
        mark_full_dirty();
        TRACE_TEXT("WND_REDRAW_DIRTY");
    } else if (message == BDA_MSG_DRAW_CONTEXT_DETACH) {
        TRACE_TEXT("WND_DETACH");
        if (!g_draw_owner || g_draw_owner == handle) {
            release_draw_context();
        }
        g_detached = 1;
    } else if (message == BDA_MSG_TOUCH_RELEASE
        || message == BDA_MSG_TOUCH_COORDINATE) {
        /*
         * The main loop consumes the GAMEBOY-style raw stream. Never feed the
         * same touch into the standard window-event path.
         */
        return 1;
    }
    {
        int default_result = bda_gui_default_proc(
            handle, message, wparam, lparam
        );
#ifdef BDA_LOADER_DIAGNOSTIC
        if (trace_this_message) {
            TRACE_VALUE("WND_DEFAULT_RESULT=", default_result);
        }
#endif
        return default_result;
    }
}

static u32 key_mask(const bda_gui_input_packet_t *packet) {
    u32 mask = 0;

    if (packet->bytes[BDA_INPUT_PACKET_RIGHT_INDEX] == 1u) {
        mask |= 1u << 0;
    }
    if (packet->bytes[BDA_INPUT_PACKET_LEFT_INDEX] == 1u) {
        mask |= 1u << 1;
    }
    if (packet->bytes[BDA_INPUT_PACKET_DOWN_INDEX] == 1u) {
        mask |= 1u << 2;
    }
    if (packet->bytes[BDA_INPUT_PACKET_UP_INDEX] == 1u) {
        mask |= 1u << 3;
    }
    if (packet->bytes[BDA_INPUT_PACKET_ESCAPE_INDEX] == 1u) {
        mask |= 1u << 4;
    }
    if (packet->bytes[BDA_INPUT_PACKET_ENTER_INDEX] == 1u) {
        mask |= 1u << 5;
    }
    return mask;
}

static void wait_key_release(u32 keycode) {
    bda_gui_input_packet_t packet;

    do {
        (void)bda_gui_input_packet(&packet);
        bda_sys_delay(1u);
    } while (bda_gui_input_packet_key_pressed(&packet, keycode));
}

static int selected_app_index(void) {
    return app_index_at(g_current_category, g_selected_ordinal);
}

static int handle_keys(void) {
    bda_gui_input_packet_t packet;
    u32 current;
    u32 pressed;

    (void)bda_gui_input_packet(&packet);
    current = key_mask(&packet);
    pressed = current & ~g_previous_keys;
    g_previous_keys = current;

    if (pressed & (1u << 0)) {
        change_selection(1);
    }
    if (pressed & (1u << 1)) {
        change_selection(-1);
    }
    if (pressed & (1u << 2)) {
        change_selection((int)GRID_COLUMNS);
    }
    if (pressed & (1u << 3)) {
        change_selection(-(int)GRID_COLUMNS);
    }
    if (current & (1u << 4)) {
        if (g_escape_packet_hold_ticks < PACKET_ESCAPE_HOLD_TICKS) {
            ++g_escape_packet_hold_ticks;
        }
        if (g_escape_packet_hold_ticks >= PACKET_ESCAPE_HOLD_TICKS) {
            TRACE_TEXT("PACKET_ESCAPE_HELD");
            g_escape_packet_hold_ticks = 0;
            wait_key_release(BDA_KEY_ESCAPE);
            return -2;
        }
    } else {
        g_escape_packet_hold_ticks = 0;
    }
    if (pressed & (1u << 5)) {
        int app_index = selected_app_index();

        if (app_index >= 0) {
            wait_key_release(BDA_KEY_ENTER);
            return app_index;
        }
    }
    return -1;
}

static int handle_touch(u32 packed) {
    int x = (s32)(s16)(packed & 0xffffu);
    int y = (s32)(s16)((packed >> 16) & 0xffffu);

    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        return -1;
    }
    if (y < GRID_TOP) {
        return -1;
    }
    if (y < GRID_BOTTOM) {
        u32 column = (u32)x / GRID_CELL_WIDTH;
        u32 row = (u32)(y - GRID_TOP) / GRID_CELL_HEIGHT;
        u32 slot = row * GRID_COLUMNS + column;
        u32 page_start = (
            g_selected_ordinal / APPS_PER_PAGE
        ) * APPS_PER_PAGE;
        u32 ordinal = page_start + slot;
        int app_index = app_index_at(g_current_category, ordinal);

        if (app_index >= 0) {
            g_selected_ordinal = ordinal;
            return app_index;
        }
        return -1;
    }
    if (y < TAB_TOP) {
        if (x < SCREEN_WIDTH / 2) {
            change_page(-1);
        } else {
            change_page(1);
        }
        return -1;
    }
    {
        u32 column = (u32)x / TAB_WIDTH;
        u32 row = (u32)(y - TAB_TOP) / TAB_HEIGHT;
        u32 category = row * TAB_COLUMNS + column;

        if (category < 10u) {
            select_category(category);
        }
    }
    return -1;
}

static int handle_touch_release(int x, int y) {
    TRACE_TWO_VALUES("TOUCH_RELEASE_X=", x, "TOUCH_RELEASE_Y=", y);
    if (g_touch_moved) {
        TRACE_TEXT("TOUCH_DRAG_CANCELLED");
        return -1;
    }
    return handle_touch((u32)(u16)x | ((u32)(u16)y << 16));
}

/*
 * Match the original GAMEBOY.BDA input path exactly: GUI+0x72c updates the
 * firmware input state before GUI+0x750 drains the low-level event stream.
 */
static int query_raw_input_state(void) {
    typedef int (*query_fn_t)(void);
    query_fn_t query = (query_fn_t)bda_sdk_internal_api(
        bda_sdk_internal_gui(),
        BDA_GUI_INPUT_STATE_QUERY
    );
    return query();
}

static int handle_raw_events(void) {
    u32 drained;

    (void)query_raw_input_state();
    for (drained = 0; drained < 4u; ++drained) {
        bda_gui_raw_event_t event;
        int result = bda_gui_raw_event_fetch(&event);

        if (result <= 0) {
            break;
        }
        if ((u32)event.code == BDA_INPUT_EVENT_TOUCH_DOWN) {
            u16 x = 0;
            u16 y = 0;

            bda_gui_touch_position(&x, &y);
            TRACE_TWO_VALUES("RAW_DOWN_X=", x, "RAW_DOWN_Y=", y);
            g_touch_key_guard_ticks = TOUCH_KEY_GUARD_TICKS;
            g_touch_key_resync_pending = 1;
            g_escape_packet_hold_ticks = 0;
            g_touch_contact_down = 1;
            g_touch_moved = 0;
            g_touch_handled_on_down = 0;
            g_touch_down_x = (int)x;
            g_touch_down_y = (int)y;
            if ((int)y >= TAB_TOP
                && (int)y < TAB_TOP + TAB_ROWS * TAB_HEIGHT) {
                g_touch_handled_on_down = 1;
                TRACE_TEXT("TOUCH_TAB_ON_DOWN");
                (void)handle_touch(
                    (u32)x | ((u32)y << 16)
                );
            }
        } else if ((u32)event.code == BDA_INPUT_EVENT_TOUCH_UP) {
            u16 x = 0;
            u16 y = 0;
            int accept = g_touch_contact_down;

            g_touch_contact_down = 0;
            g_touch_key_guard_ticks = TOUCH_KEY_GUARD_TICKS;
            g_touch_key_resync_pending = 1;
            g_escape_packet_hold_ticks = 0;
            if (accept) {
                bda_gui_touch_position(&x, &y);
                TRACE_TWO_VALUES("RAW_UP_X=", x, "RAW_UP_Y=", y);
                if (g_touch_handled_on_down) {
                    g_touch_handled_on_down = 0;
                    return -1;
                }
                return handle_touch_release((int)x, (int)y);
            }
        } else if ((u32)event.code == BDA_INPUT_EVENT_TOUCH_MOVE
                   && g_touch_contact_down) {
            u16 x = 0;
            u16 y = 0;
            int delta_x;
            int delta_y;

            g_touch_key_guard_ticks = TOUCH_KEY_GUARD_TICKS;
            g_touch_key_resync_pending = 1;
            g_escape_packet_hold_ticks = 0;
            bda_gui_touch_position(&x, &y);
            delta_x = (int)x - g_touch_down_x;
            delta_y = (int)y - g_touch_down_y;
            if (delta_x < 0) {
                delta_x = -delta_x;
            }
            if (delta_y < 0) {
                delta_y = -delta_y;
            }
            if (delta_x > 8 || delta_y > 8) {
                g_touch_moved = 1;
            }
        } else if ((u32)event.code == BDA_INPUT_EVENT_KEY_DOWN
                   && event.value == 9) {
            /*
             * Only a raw key event is accepted as the physical ESC button.
             * Bottom-screen touches can transiently set the packet ESC bit on
             * C200knl even though they are not a hardware key press.
             */
            TRACE_TEXT("RAW_ESCAPE_DOWN");
            return -2;
        }
    }
    return -1;
}

/*
 * Keep this small raw-event drain separate from handle_raw_events(): startup
 * events must never become a complete DOWN -> UP gesture in the loader.
 */
static u32 discard_raw_events(u32 budget, u32 *input_event_count) {
    u32 drained;
    u32 input_events = 0;

    g_touch_contact_down = 0;
    (void)query_raw_input_state();
    for (drained = 0; drained < budget; ++drained) {
        bda_gui_raw_event_t event;

        if (bda_gui_raw_event_fetch(&event) <= 0) {
            break;
        }
        if ((u32)event.code == BDA_INPUT_EVENT_TOUCH_DOWN
            || (u32)event.code == BDA_INPUT_EVENT_TOUCH_UP) {
            ++input_events;
        }
    }
    if (input_event_count) {
        *input_event_count = input_events;
    }
    return drained;
}

static void sync_key_state(void) {
    bda_gui_input_packet_t packet;

    (void)bda_gui_input_packet(&packet);
    g_previous_keys = key_mask(&packet);
}

static int allocate_ui_buffers(void) {
    g_screen_vx = (u8 *)bda_alloc(SCREEN_VX_SIZE);
    if (!g_screen_vx || (u32)g_screen_vx == 0xffffffffu) {
        g_screen_vx = 0;
        return 0;
    }
    g_present_vx = (u8 *)bda_alloc(PRESENT_STRIP_VX_SIZE);
    if (!g_present_vx || (u32)g_present_vx == 0xffffffffu) {
        g_present_vx = 0;
        return 0;
    }
    g_icon_slots = (u8 *)bda_alloc(ICON_SLOT_SIZE);
    if (!g_icon_slots || (u32)g_icon_slots == 0xffffffffu) {
        g_icon_slots = 0;
        return 0;
    }
    init_vx_header(g_screen_vx, SCREEN_WIDTH, SCREEN_HEIGHT);
    init_vx_header(
        g_present_vx,
        SCREEN_WIDTH,
        PRESENT_STRIP_HEIGHT
    );
    return 1;
}

static void free_heap_resources(void) {
    if (g_system_font) {
        bda_free(g_system_font);
        g_system_font = 0;
    }
    if (g_icon_slots) {
        bda_free(g_icon_slots);
        g_icon_slots = 0;
    }
    if (g_present_vx) {
        bda_free(g_present_vx);
        g_present_vx = 0;
    }
    if (g_screen_vx) {
        bda_free(g_screen_vx);
        g_screen_vx = 0;
    }
    if (g_apps) {
        bda_free(g_apps);
        g_apps = 0;
    }
}

static void release_loader_graphics(void) {
    if (g_back && (s32)g_back != -1) {
        TRACE_TEXT("BACK_FREE_BEGIN");
        bda_gui_compatible_context_free(g_back);
        g_back = 0;
        TRACE_TEXT("BACK_FREE_DONE");
    }
    TRACE_TEXT("DRAW_RELEASE_BEGIN");
    release_draw_context();
    TRACE_TEXT("DRAW_RELEASE_DONE");
}

static void release_borrowed_frame(void) {
    /*
     * g_frame is the outer firmware menu frame from saved s3. It is not owned
     * by this BDA and must never receive stop/release/close here. The outer
     * path launcher already suspended its timer before entering the loader
     * and resumes it after the final chained target returns.
     */
    TRACE_VALUE("BORROW_FRAME_RELEASE=", g_frame);
    release_loader_graphics();
    g_frame = 0;
    TRACE_TEXT("BORROW_FRAME_RELEASE_DONE");
}

typedef struct firmware_path_profile {
    u32 path_entry;
    u32 cache_barrier;
    u32 saved_caller_ra_offset;
    u32 launch_context_a2;
    const char *name;
} firmware_path_profile_t;

typedef struct known_firmware_path {
    u32 path_entry;
    u32 cache_barrier;
    const char *name;
} known_firmware_path_t;

static const known_firmware_path_t g_known_firmware_paths[] = {
    {0x8002e1c0u, 0x80004264u, "9588-JZ4720"},
    {0x80021098u, 0x80004150u, "9588-JZ4730"},
    {0x8002c5b0u, 0x80004264u, "9588-JZ4740"},
    {0x80021678u, 0x80004150u, "9688-JZ4730"},
    {0x8002e6b8u, 0x80004264u, "9688-JZ4740"}
};

static const char *firmware_path_name(
    u32 path_entry,
    u32 cache_barrier,
    const char *compatible_name
) {
    u32 index;

    for (index = 0;
         index < sizeof(g_known_firmware_paths)
            / sizeof(g_known_firmware_paths[0]);
         ++index) {
        if (g_known_firmware_paths[index].path_entry == path_entry
            && g_known_firmware_paths[index].cache_barrier
                == cache_barrier) {
            return g_known_firmware_paths[index].name;
        }
    }
    return compatible_name;
}

static u32 decode_direct_jump_target(u32 call_site, u32 instruction) {
    return ((call_site + 4u) & 0xf0000000u)
        | ((instruction & 0x03ffffffu) << 2);
}

static int validate_cache_barrier(u32 address) {
    /*
     * All three 9588 V3.30 kernels use this cache walk. JZ4720/JZ4740 add a
     * sync before returning while JZ4730 returns immediately, so validate the
     * common body and call the actual target decoded from the path-loader jal.
     */
    return address >= 0x80004000u
        && address < 0x80500000u
        && !(address & 3u)
        && *(volatile u32 *)(address + 0x00u) == 0x3c048000u
        && *(volatile u32 *)(address + 0x04u) == 0x3c038000u
        && *(volatile u32 *)(address + 0x08u) == 0x34843fffu
        && *(volatile u32 *)(address + 0x0cu) == 0xbc610000u
        && *(volatile u32 *)(address + 0x10u) == 0x24630020u
        && *(volatile u32 *)(address + 0x14u) == 0x0083102bu
        && *(volatile u32 *)(address + 0x18u) == 0x1040fffcu
        && *(volatile u32 *)(address + 0x1cu) == 0x00000000u;
}

static int validate_path_loader_tail(
    u32 return_address,
    u32 *cache_barrier
) {
    u32 cache_call_site = return_address - 0x10u;
    u32 cache_call = *(volatile u32 *)cache_call_site;
    u32 decoded_cache;

    if ((cache_call >> 26) != 3u
        || *(volatile u32 *)(return_address - 0x40u) != 0x3c0481c0u
        || *(volatile u32 *)(return_address - 0x28u) != 0x34840020u
        || *(volatile u32 *)(return_address - 0x24u) != 0x3c1281c0u
        || *(volatile u32 *)(return_address - 0x14u) != 0x36520020u
        || *(volatile u32 *)(return_address - 8u) != 0x0240f809u
        || *(volatile u32 *)return_address != 0x1660001fu) {
        return 0;
    }
    decoded_cache = decode_direct_jump_target(cache_call_site, cache_call);
    if (!validate_cache_barrier(decoded_cache)) {
        return 0;
    }
    *cache_barrier = decoded_cache;
    return 1;
}

static int resolve_firmware_path_profile(
    firmware_path_profile_t *profile
) {
    u32 return_address = g_bda_loader_entry_ra;
    u32 entry;
    u32 cache_barrier;

    /*
     * The JZ4730 and JZ4740 kernels share the s0/s6 path-loader layout:
     *
     *   9588 JZ4730 entry=0x80021098 return=0x8002128c
     *   9588 JZ4740 entry=0x8002c5b0 return=0x8002c7a4
     *   9688 JZ4730 entry=0x80021678 return=0x8002186c
     *   9688 JZ4740 entry=0x8002e6b8 return=0x8002e8ac
     *
     * JZ4720 keeps the same external ABI and 0xf0-byte stack frame, but stores
     * path in s2 and a2 in s4:
     *
     *   JZ4720 entry=0x8002e1c0 return=0x8002e3ec
     *
     * Resolve from the live BDA return address and validate both the prologue
     * and the complete 0x81c00020/cache/jalr tail. No chip-dependent path entry
     * or cache helper is called solely because it occupies a known address.
     */
    if (return_address < 0x8000422cu
        || return_address >= 0x80500000u
        || (return_address & 3u)
        || !validate_path_loader_tail(return_address, &cache_barrier)) {
        return 0;
    }

    entry = return_address - 0x1f4u;
    if ((cache_barrier == 0x80004150u
            || cache_barrier == 0x80004264u)
        && *(volatile u32 *)(entry + 0x00u) == 0x27bdff10u
        && *(volatile u32 *)(entry + 0x04u) == 0xafb000d0u
        && *(volatile u32 *)(entry + 0x08u) == 0x00808021u
        && *(volatile u32 *)(entry + 0x10u) == 0xafbf00ecu
        && *(volatile u32 *)(entry + 0x1cu) == 0x00c0b021u
        && *(volatile u32 *)(entry + 0x2cu) == 0x00a09821u
        && *(volatile u32 *)(entry + 0x44u) == 0x02002021u) {
        profile->path_entry = entry;
        profile->cache_barrier = cache_barrier;
        profile->saved_caller_ra_offset = 0xecu;
        profile->launch_context_a2 = g_bda_loader_entry_s6;
        profile->name = firmware_path_name(
            entry,
            cache_barrier,
            cache_barrier == 0x80004150u
                ? "JZ4730-COMPAT"
                : "JZ4740-COMPAT"
        );
        return 1;
    }

    entry = return_address - 0x22cu;
    if (cache_barrier == 0x80004264u
        && *(volatile u32 *)(entry + 0x00u) == 0x27bdff10u
        && *(volatile u32 *)(entry + 0x04u) == 0xafb200d8u
        && *(volatile u32 *)(entry + 0x08u) == 0x00809021u
        && *(volatile u32 *)(entry + 0x10u) == 0xafbf00ecu
        && *(volatile u32 *)(entry + 0x24u) == 0x00c0a021u
        && *(volatile u32 *)(entry + 0x28u) == 0x00a09821u
        && *(volatile u32 *)(entry + 0x44u) == 0x02402021u) {
        profile->path_entry = entry;
        profile->cache_barrier = cache_barrier;
        profile->saved_caller_ra_offset = 0xecu;
        profile->launch_context_a2 = g_bda_loader_entry_s4;
        profile->name = firmware_path_name(
            entry,
            cache_barrier,
            "JZ4720-COMPAT"
        );
        return 1;
    }
    return 0;
}

static u32 mips_lui(u32 reg, u32 value) {
    return 0x3c000000u | (reg << 16) | ((value >> 16) & 0xffffu);
}

static u32 mips_ori(u32 target, u32 source, u32 value) {
    return 0x34000000u
        | (source << 21)
        | (target << 16)
        | (value & 0xffffu);
}

static void emit_load_address(u32 *code, u32 *count, u32 reg, u32 value) {
    code[(*count)++] = mips_lui(reg, value);
    code[(*count)++] = mips_ori(reg, reg, value);
}

static int schedule_app_after_return(int app_index) {
    enum {
        REG_A0 = 4u,
        REG_A1 = 5u,
        REG_A2 = 6u,
        REG_T0 = 8u,
        REG_T9 = 25u,
        REG_RA = 31u,
        TRAMPOLINE_PATH_OFFSET = 0x60u
    };
    firmware_path_profile_t profile;
    u32 path_entry = 0;
    volatile u32 *saved_caller_ra_slot = 0;
    u32 caller_return = 0;
    u32 expected_jal;
    u32 allocation_size;
    u8 *trampoline;
    u32 *code;
    u32 count = 0;
    const char *path;

    copy_string(
        g_launch_path,
        sizeof(g_launch_path),
        g_apps[app_index].path
    );
    path = g_launch_path;
    if (resolve_firmware_path_profile(&profile)) {
        path_entry = profile.path_entry;
        saved_caller_ra_slot = (volatile u32 *)(
            g_bda_loader_entry_sp + profile.saved_caller_ra_offset
        );
        caller_return = *saved_caller_ra_slot;
    }
    expected_jal = 0x0c000000u
        | ((path_entry >> 2) & 0x03ffffffu);
    TRACE_BATCH_BEGIN();
    TRACE_VALUE("LAUNCH_INDEX=", app_index);
    TRACE_LABELED_TEXT("LAUNCH_PATH=", path);
    TRACE_TEXT("LAUNCH_MODE=DEFER_AFTER_RETURN");
    TRACE_LABELED_TEXT(
        "FIRMWARE_PROFILE=",
        path_entry ? profile.name : "UNSUPPORTED"
    );
    TRACE_VALUE("LAUNCH_ENTRY=", path_entry);
    TRACE_VALUE(
        "LAUNCH_CACHE_BARRIER=",
        path_entry ? profile.cache_barrier : 0u
    );
    TRACE_VALUE("OUTER_CALLER_RA_SLOT=", saved_caller_ra_slot);
    TRACE_VALUE("OUTER_CALLER_RA=", caller_return);
    TRACE_VALUE(
        "OUTER_CALL_WORD=",
        caller_return >= 8u
            ? *(volatile u32 *)(caller_return - 8u)
            : 0u
    );
    TRACE_VALUE("ENTRY_SP=", g_bda_loader_entry_sp);
    TRACE_VALUE("ENTRY_RA=", g_bda_loader_entry_ra);
    TRACE_VALUE("ENTRY_GP=", g_bda_loader_entry_gp);
    TRACE_VALUE("ENTRY_S0=", g_bda_loader_entry_s0);
    TRACE_VALUE("ENTRY_S2=", g_bda_loader_entry_s2);
    TRACE_VALUE("ENTRY_S3=", g_bda_loader_entry_s3);
    TRACE_VALUE("ENTRY_S6=", g_bda_loader_entry_s6);
    TRACE_VALUE("ENTRY_FP=", g_bda_loader_entry_fp);
    TRACE_VALUE("FW_C5B0_WORD=", *(volatile u32 *)0x8002c5b0u);
    TRACE_VALUE("FW_C794_WORD=", *(volatile u32 *)0x8002c794u);
    TRACE_VALUE("FW_C79C_WORD=", *(volatile u32 *)0x8002c79cu);
    TRACE_VALUE("FW_C878_WORD=", *(volatile u32 *)0x8002c878u);
    TRACE_VALUE(
        "FW_ENTRY_RA_WORD=",
        *(volatile u32 *)g_bda_loader_entry_ra
    );
    TRACE_VALUE("LAUNCH_CONTEXT_A1=", g_bda_loader_entry_s3);
    TRACE_VALUE(
        "LAUNCH_CONTEXT_A2=",
        path_entry ? profile.launch_context_a2 : 0u
    );
    if (!path_entry
        || caller_return < 0x80004008u
        || caller_return >= 0x80500000u
        || *(volatile u32 *)(caller_return - 8u) != expected_jal) {
        TRACE_TEXT("DEFER_CALLSITE_INVALID");
        TRACE_BATCH_END();
        return 0;
    }

    allocation_size =
        TRAMPOLINE_PATH_OFFSET + string_length(path) + 1u;
    trampoline = (u8 *)bda_alloc(allocation_size);
    if (!trampoline || (u32)trampoline == 0xffffffffu) {
        TRACE_TEXT("DEFER_ALLOC_FAILED");
        TRACE_BATCH_END();
        return 0;
    }
    bda_memset(trampoline, 0, allocation_size);
    copy_string(
        (char *)trampoline + TRAMPOLINE_PATH_OFFSET,
        allocation_size - TRAMPOLINE_PATH_OFFSET,
        path
    );
    code = (u32 *)trampoline;

    /* a0=path, a1=menu frame, a2=menu state */
    emit_load_address(
        code, &count, REG_A0,
        (u32)trampoline + TRAMPOLINE_PATH_OFFSET
    );
    emit_load_address(code, &count, REG_A1, g_bda_loader_entry_s3);
    emit_load_address(code, &count, REG_A2, profile.launch_context_a2);
    emit_load_address(code, &count, REG_T9, path_entry);
    code[count++] = 0x0320f809u; /* jalr t9 */
    code[count++] = 0x00000000u;

    /*
     * The target and its firmware path-loader have now returned. Free this
     * executable allocation without ever fetching another instruction from
     * it: set ra to the original caller continuation and tail-jump to MEM_FREE.
     */
    emit_load_address(code, &count, REG_A0, (u32)trampoline);
    code[count++] = mips_lui(REG_T0, 0x81c00000u);
    code[count++] = 0x8d080010u; /* lw t0,0x10(t0): MEM table */
    code[count++] = 0x8d19000cu; /* lw t9,0x0c(t0): MEM_FREE */
    emit_load_address(code, &count, REG_RA, caller_return);
    code[count++] = 0x03200008u; /* jr t9 */
    code[count++] = 0x00000000u;

    if (count * 4u > TRAMPOLINE_PATH_OFFSET) {
        bda_free(trampoline);
        TRACE_TEXT("DEFER_CODE_TOO_LARGE");
        TRACE_BATCH_END();
        return 0;
    }

    TRACE_VALUE("DEFER_TRAMPOLINE=", trampoline);
    TRACE_VALUE("DEFER_CODE_WORDS=", count);
    TRACE_VALUE("DEFER_PATCH_OLD=", *saved_caller_ra_slot);
    /*
     * Use the barrier called by this exact path-loader. JZ4730 uses
     * 0x80004150; JZ4720/JZ4740 use 0x80004264. The resolver decoded and
     * validated its body before this indirect call.
     */
    ((void (*)(void))profile.cache_barrier)();
    *saved_caller_ra_slot = (u32)trampoline;
    TRACE_VALUE("DEFER_PATCH_NEW=", *saved_caller_ra_slot);
    TRACE_TEXT("DEFER_READY_RETURN_NORMALLY");
    TRACE_BATCH_END();
    return 1;
}

int bda_loader_main(void) {
    bda_gui_input_packet_t packet;
    int launch_index = -1;
    int call_result;
    int font_result;
    int action = -1;
    u32 font_start;
    u32 last_input_tick;
    u32 startup_raw_drained;
    u32 startup_raw_input;

    g_apps = 0;
    g_screen_vx = 0;
    g_present_vx = 0;
    g_icon_slots = 0;
    g_system_font = 0;
    g_app_count = 0;
    g_current_category = 0;
    g_selected_ordinal = 0;
    g_previous_keys = 0;
    g_frame = 0;
    g_draw = 0;
    g_draw_owner = 0;
    g_back = 0;
    g_draw_object = 0;
    g_detached = 0;
    g_dirty = 1;
    g_dirty_top = 0;
    g_dirty_bottom = SCREEN_HEIGHT;
    g_render_clip_top = 0;
    g_render_clip_bottom = SCREEN_HEIGHT;
    g_selected_icon_valid = 0;
    g_touch_contact_down = 0;
    g_touch_moved = 0;
    g_touch_handled_on_down = 0;
    g_touch_down_x = 0;
    g_touch_down_y = 0;
    g_touch_key_guard_ticks = 0;
    g_touch_key_resync_pending = 0;
    g_escape_packet_hold_ticks = 0;
    g_input_startup_guard_ticks = INPUT_GUARD_MAX_TICKS;
    g_loading = 1;
    g_launch_path[0] = 0;
#ifdef BDA_LOADER_DIAGNOSTIC
    g_trace_batch_file = 0;
    g_trace_present_count = 0;
    g_trace_window_count = 0;
    g_diag_findfirst_ms = 0;
    g_diag_findnext_ms = 0;
    g_diag_header_open_ms = 0;
    g_diag_header_read_seek_ms = 0;
    g_diag_header_close_ms = 0;
    g_diag_header_decode_ms = 0;
    g_diag_header_total_ms = 0;
    g_diag_header_max_ms = 0;
    g_diag_header_count = 0;
    g_diag_header_valid_count = 0;
    g_diag_sort_ms = 0;
    g_diag_scan_ms = 0;
    g_diag_icon_total_ms = 0;
    g_diag_icon_max_ms = 0;
    g_diag_icon_count = 0;
    g_diag_cpu_icon_composed = 0;
    g_diag_cpu_icon_transparent = 0;
    g_diag_selected_icon_load_count = 0;
    g_diag_selected_icon_load_total_ms = 0;
    g_diag_selected_icon_load_max_ms = 0;
#endif
    TRACE_RESET();
    TRACE_TEXT("BDALOAD TRACE V35");
    TRACE_TEXT("MAIN_INIT_DONE");
    TRACE_TEXT("FRAME_MODE=BORROW_OUTER_S3");
    TRACE_TEXT("ICON_COMPOSITOR=MANUAL_EXACT_F81F");
    TRACE_TEXT("ICON_CACHE_DISABLED=1");
    TRACE_TEXT("VX_BLACK_REMAP=0_TO_1");
    TRACE_TEXT("SYSTEM_FONT=RUNTIME_HZK_LIB");

    TRACE_TEXT("SYSTEM_FONT_LOAD_BEGIN");
    font_start = bda_gui_millisecond_count();
    font_result = load_system_font();
    TRACE_VALUE("SYSTEM_FONT_LOAD_RESULT=", font_result);
    TRACE_VALUE(
        "SYSTEM_FONT_LOAD_MS=",
        bda_gui_millisecond_elapsed(
            font_start, bda_gui_millisecond_count()
        )
    );
    TRACE_VALUE("SYSTEM_FONT_CACHE_PTR=", g_system_font);
    TRACE_VALUE(
        "SYSTEM_FONT_CACHE_BYTES=",
        g_system_font ? SYSTEM_FONT_CACHE_SIZE : 0u
    );
    if (font_result != SYSTEM_FONT_LOAD_OK) {
        if (font_result == SYSTEM_FONT_LOAD_MISSING) {
            TRACE_TEXT("SYSTEM_FONT_MISSING");
            bda_msgbox(k_loader_title, k_system_font_missing);
        } else if (font_result == SYSTEM_FONT_LOAD_NO_MEMORY) {
            TRACE_TEXT("SYSTEM_FONT_NO_MEMORY");
            bda_msgbox(k_loader_title, k_system_font_no_memory);
        } else {
            TRACE_TEXT("SYSTEM_FONT_INVALID");
            bda_msgbox(k_loader_title, k_system_font_invalid);
        }
        free_heap_resources();
        return 6;
    }
    TRACE_TEXT("SYSTEM_FONT_LOAD_DONE");

    TRACE_TEXT("UI_ALLOC_BEGIN");
    if (!allocate_ui_buffers()) {
        TRACE_TEXT("UI_ALLOC_FAILED");
        bda_msgbox(k_loader_title, "OUT OF MEMORY");
        free_heap_resources();
        return 2;
    }
    TRACE_VALUE("SCREEN_VX_PTR=", g_screen_vx);
    TRACE_VALUE("PRESENT_VX_PTR=", g_present_vx);
    TRACE_VALUE("ICON_SLOTS_PTR=", g_icon_slots);
    TRACE_TEXT("UI_ALLOC_DONE");

    g_frame = (bda_handle_t)g_bda_loader_entry_s3;
    TRACE_VALUE("BORROW_FRAME=", g_frame);
    if (!g_frame || (s32)g_frame == -1) {
        TRACE_TEXT("BORROW_FRAME_INVALID");
        free_heap_resources();
        return 3;
    }
    TRACE_TEXT("BORROW_FRAME_ACTIVATE_BEGIN");
    call_result = bda_gui_frame_activate(g_frame, 0x100u);
    TRACE_VALUE("BORROW_FRAME_ACTIVATE_RESULT=", call_result);
    TRACE_TEXT("BORROW_DRAW_ACQUIRE_BEGIN");
    call_result = acquire_draw_context(g_frame);
    TRACE_VALUE("BORROW_DRAW_ACQUIRE_RESULT=", call_result);
    TRACE_VALUE("BORROW_DRAW=", g_draw);
    if (!g_draw_object) {
        TRACE_TEXT("BORROW_DRAW_OBJECT_BEGIN");
        g_draw_object = bda_gui_draw_object_create(7u);
        TRACE_VALUE("BORROW_DRAW_OBJECT=", g_draw_object);
    }
    if (!g_draw || !g_draw_object || (s32)(u32)g_draw_object == -1) {
        TRACE_TEXT("DRAW_SETUP_FAILED");
        release_borrowed_frame();
        free_heap_resources();
        return 4;
    }
    TRACE_TEXT("BACK_CREATE_BEGIN");
    g_back = bda_gui_compatible_context_create(g_draw);
    TRACE_VALUE("BACK_CREATE_RESULT=", g_back);
    if (!g_back || (s32)g_back == -1) {
        TRACE_TEXT("BACK_CREATE_FAILED");
        g_back = 0;
        release_borrowed_frame();
        free_heap_resources();
        return 5;
    }
    TRACE_TEXT("LOADING_PRESENT_CALL");
    call_result = present_screen();
    TRACE_VALUE("LOADING_PRESENT_RESULT=", call_result);

    TRACE_TEXT("SCAN_BEGIN");
    if (!scan_apps()) {
        TRACE_TEXT("SCAN_FAILED");
        release_borrowed_frame();
        bda_msgbox(k_loader_title, k_no_apps);
        free_heap_resources();
        return 1;
    }
    TRACE_VALUE("SCAN_DONE_COUNT=", g_app_count);
    select_first_category();
    TRACE_VALUE("FIRST_CATEGORY=", g_current_category);
    g_loading = 0;
    mark_full_dirty();
    TRACE_TEXT("LIST_PRESENT_CALL");
    call_result = present_screen();
    TRACE_VALUE("LIST_PRESENT_RESULT=", call_result);

    TRACE_TEXT("INPUT_PACKET_BEGIN");
    call_result = bda_gui_input_packet(&packet);
    TRACE_VALUE("INPUT_PACKET_RESULT=", call_result);
    g_previous_keys = key_mask(&packet);
    TRACE_VALUE("INITIAL_KEY_MASK=", g_previous_keys);
    TRACE_VALUE(
        "INPUT_GUARD_TICKS=",
        g_input_startup_guard_ticks
    );
    startup_raw_drained = discard_raw_events(512u, &startup_raw_input);
    TRACE_VALUE("STARTUP_RAW_DRAIN=", startup_raw_drained);
    TRACE_VALUE("STARTUP_RAW_INPUT=", startup_raw_input);
    sync_key_state();
    TRACE_VALUE("POST_DRAIN_KEY_MASK=", g_previous_keys);
    TRACE_TEXT("RAW_EVENT_LOOP_BEGIN");
    last_input_tick = bda_gui_tick_count_25ms() - 1u;

    for (;;) {
        u32 now = bda_gui_tick_count_25ms();

        if (bda_gui_tick_elapsed_25ms(last_input_tick, now) >= 1u) {
            last_input_tick = now;
            if (g_input_startup_guard_ticks) {
                (void)discard_raw_events(64u, 0);
                sync_key_state();
                --g_input_startup_guard_ticks;
                if (!g_input_startup_guard_ticks) {
                    TRACE_TEXT("INPUT_GUARD_DONE");
                }
                action = -1;
            } else {
                /*
                 * GAMEBOY.BDA drains GUI+0x750 before querying the 6-byte key
                 * packet. Reversing that order lets the packet path consume
                 * touch transitions before the raw-event loop sees them.
                 */
                action = handle_raw_events();
                if (action >= 0) {
                    launch_index = action;
                }
                /*
                 * Preserve the raw ESC sentinel. Calling handle_keys() in the
                 * same tick used to overwrite -2 with its normal -1 result,
                 * so the trace showed RAW_ESCAPE_DOWN but the loop continued.
                 */
                if (launch_index < 0 && action != -2) {
                    if (g_touch_key_guard_ticks) {
                        --g_touch_key_guard_ticks;
                        action = -1;
                    } else if (g_touch_key_resync_pending) {
                        sync_key_state();
                        g_touch_key_resync_pending = 0;
                        action = -1;
                    } else {
                        action = handle_keys();
                        if (action >= 0) {
                            launch_index = action;
                        } else if (action == -2) {
                            launch_index = -1;
                        }
                    }
                }
            }
            if (launch_index >= 0 || action == -2) {
                break;
            }
            if (g_detached) {
                action = -2;
                break;
            }
            (void)present_screen();
        }
        bda_sys_delay(1u);
    }

    TRACE_VALUE("CLOSE_ACTION=", action);
    TRACE_VALUE("CLOSE_LAUNCH_INDEX=", launch_index);
    TRACE_VALUE(
        "SELECTED_ICON_LOAD_COUNT=",
        g_diag_selected_icon_load_count
    );
    TRACE_VALUE(
        "SELECTED_ICON_LOAD_TOTAL_MS=",
        g_diag_selected_icon_load_total_ms
    );
    TRACE_VALUE(
        "SELECTED_ICON_LOAD_MAX_MS=",
        g_diag_selected_icon_load_max_ms
    );
    TRACE_TEXT("RAW_EVENT_LOOP_DONE");
    release_borrowed_frame();
    if (launch_index >= 0) {
        if (!schedule_app_after_return(launch_index)) {
            TRACE_TEXT("DEFER_SCHEDULE_FAILED");
        }
    }
    TRACE_TEXT("MAIN_FREE_HEAP");
    free_heap_resources();
    TRACE_TEXT("MAIN_RETURN");
    return 0;
}
