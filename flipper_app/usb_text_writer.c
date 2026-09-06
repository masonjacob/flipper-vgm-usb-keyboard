#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <expansion/expansion.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define APP_TAG "UsbTextWriter"

#define UART_BAUD 460800U
#define DOCUMENT_MAX 8192U
#define VIEW_COLUMNS 21U
#define VIEW_ROWS 7U

#define KBW_MAGIC_LEN 4U
#define KBW_FRAME_MAGIC 0xE7U
#define KBW_EVENT_KEY 0x10U

#define KEY_A 0x04U
#define KEY_S 0x16U
#define KEY_Z 0x1DU
#define KEY_1 0x1EU
#define KEY_0 0x27U
#define KEY_ENTER 0x28U
#define KEY_ESC 0x29U
#define KEY_BACKSPACE 0x2AU
#define KEY_TAB 0x2BU
#define KEY_SPACE 0x2CU
#define KEY_MINUS 0x2DU
#define KEY_EQUAL 0x2EU
#define KEY_LEFTBRACKET 0x2FU
#define KEY_RIGHTBRACKET 0x30U
#define KEY_BACKSLASH 0x31U
#define KEY_SEMICOLON 0x33U
#define KEY_APOSTROPHE 0x34U
#define KEY_GRAVE 0x35U
#define KEY_COMMA 0x36U
#define KEY_DOT 0x37U
#define KEY_SLASH 0x38U
#define KEY_CAPSLOCK 0x39U
#define KEY_F1 0x3AU
#define KEY_F12 0x45U
#define KEY_PRINTSCREEN 0x46U
#define KEY_SCROLLLOCK 0x47U
#define KEY_PAUSE 0x48U
#define KEY_INSERT 0x49U
#define KEY_HOME 0x4AU
#define KEY_PAGEUP 0x4BU
#define KEY_DELETE 0x4CU
#define KEY_END 0x4DU
#define KEY_PAGEDOWN 0x4EU
#define KEY_RIGHT 0x4FU
#define KEY_LEFT 0x50U
#define KEY_DOWN 0x51U
#define KEY_UP 0x52U

#define MOD_LCTRL 0x01U
#define MOD_LSHIFT 0x02U
#define MOD_LALT 0x04U
#define MOD_LGUI 0x08U
#define MOD_RCTRL 0x10U
#define MOD_RSHIFT 0x20U
#define MOD_RALT 0x40U
#define MOD_RGUI 0x80U

typedef struct {
    ViewPort* view_port;
    Gui* gui;
    FuriMessageQueue* input_queue;

    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx;

    char* text;
    size_t len;
    size_t cursor;

    uint16_t top_line;
    bool running;
    bool connected;
    bool caps_lock;

    uint8_t rx_state;
} UsbTextWriterApp;

typedef struct {
    uint8_t type;
    InputEvent input;
} AppEvent;

static const uint8_t host_magic[KBW_MAGIC_LEN] = {'K', 'B', 'W', '1'};
static const uint8_t ack_magic[KBW_MAGIC_LEN] = {'A', 'C', 'K', '1'};

static size_t line_start(const char* text, size_t pos) {
    while(pos > 0 && text[pos - 1] != '\n') pos--;
    return pos;
}

static size_t line_end(const char* text, size_t len, size_t pos) {
    while(pos < len && text[pos] != '\n') pos++;
    return pos;
}

static void ensure_cursor_visible(UsbTextWriterApp* app) {
    size_t pos = 0;
    uint16_t line = 0;

    while(pos < app->cursor) {
        if(app->text[pos] == '\n') line++;
        pos++;
    }

    if(line < app->top_line) {
        app->top_line = line;
    } else if(line >= (uint16_t)(app->top_line + VIEW_ROWS)) {
        app->top_line = (uint16_t)(line - VIEW_ROWS + 1);
    }
}

static void insert_byte(UsbTextWriterApp* app, char ch) {
    if(app->len + 1 >= DOCUMENT_MAX) return;

    memmove(
        app->text + app->cursor + 1,
        app->text + app->cursor,
        app->len - app->cursor);

    app->text[app->cursor] = ch;
    app->cursor++;
    app->len++;
}

static void delete_prev(UsbTextWriterApp* app) {
    if(app->cursor == 0) return;

    memmove(
        app->text + app->cursor - 1,
        app->text + app->cursor,
        app->len - app->cursor);

    app->cursor--;
    app->len--;
}

static void delete_at_cursor(UsbTextWriterApp* app) {
    if(app->cursor >= app->len) return;

    memmove(
        app->text + app->cursor,
        app->text + app->cursor + 1,
        app->len - app->cursor - 1);

    app->len--;
}

static size_t visual_column(const char* text, size_t pos) {
    size_t start = line_start(text, pos);
    return pos - start;
}

static void move_left(UsbTextWriterApp* app) {
    if(app->cursor > 0) app->cursor--;
    ensure_cursor_visible(app);
}

static void move_right(UsbTextWriterApp* app) {
    if(app->cursor < app->len) app->cursor++;
    ensure_cursor_visible(app);
}

static void move_home(UsbTextWriterApp* app) {
    app->cursor = line_start(app->text, app->cursor);
    ensure_cursor_visible(app);
}

static void move_end(UsbTextWriterApp* app) {
    app->cursor = line_end(app->text, app->len, app->cursor);
    ensure_cursor_visible(app);
}

static void move_vertical(UsbTextWriterApp* app, int direction) {
    size_t current_start = line_start(app->text, app->cursor);
    size_t col = app->cursor - current_start;

    if(direction < 0) {
        if(current_start == 0) return;
        size_t prev_end = current_start - 1;
        size_t prev_start = line_start(app->text, prev_end);
        size_t prev_len = prev_end - prev_start;
        if(col > prev_len) col = prev_len;
        app->cursor = prev_start + col;
    } else {
        size_t current_end = line_end(app->text, app->len, app->cursor);
        if(current_end >= app->len) return;

        size_t next_start = current_end + 1;
        size_t next_end = line_end(app->text, app->len, next_start);
        size_t next_len = next_end - next_start;
        if(col > next_len) col = next_len;
        app->cursor = next_start + col;
    }

    ensure_cursor_visible(app);
}

static char usage_to_ascii(uint8_t usage, uint8_t modifiers, bool caps_lock) {
    bool shift =
        (modifiers & (MOD_LSHIFT | MOD_RSHIFT)) != 0;

    if(usage >= KEY_A && usage <= KEY_Z) {
        char c = (char)('a' + (usage - KEY_A));
        if(shift ^ caps_lock) c = (char)toupper((unsigned char)c);
        return c;
    }

    if(usage >= KEY_1 && usage <= KEY_0) {
        static const char normal[] = "1234567890";
        static const char shifted[] = "!@#$%^&*()";
        size_t index = usage - KEY_1;
        return shift ? shifted[index] : normal[index];
    }

    switch(usage) {
    case KEY_SPACE: return ' ';
    case KEY_TAB: return '\t';
    case KEY_ENTER: return '\n';
    case KEY_MINUS: return shift ? '_' : '-';
    case KEY_EQUAL: return shift ? '+' : '=';
    case KEY_LEFTBRACKET: return shift ? '{' : '[';
    case KEY_RIGHTBRACKET: return shift ? '}' : ']';
    case KEY_BACKSLASH: return shift ? '|' : '\\';
    case KEY_SEMICOLON: return shift ? ':' : ';';
    case KEY_APOSTROPHE: return shift ? '"' : '\'';
    case KEY_GRAVE: return shift ? '~' : '`';
    case KEY_COMMA: return shift ? '<' : ',';
    case KEY_DOT: return shift ? '>' : '.';
    case KEY_SLASH: return shift ? '?' : '/';
    default: return 0;
    }
}

static bool save_file(UsbTextWriterApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    storage_common_mkdir(storage, "/ext/apps_data");
    storage_common_mkdir(storage, "/ext/apps_data/usb_text_writer");

    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(
        file,
        "/ext/apps_data/usb_text_writer/notes.txt",
        FSAM_WRITE,
        FSOM_CREATE_ALWAYS);

    if(ok) {
        ok = (storage_file_write(file, app->text, app->len) == app->len);
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void draw_callback(Canvas* canvas, void* context) {
    UsbTextWriterApp* app = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    size_t pos = 0;
    uint16_t line = 0;
    uint8_t y = 8;

    while(pos < app->len && line < app->top_line) {
        if(app->text[pos++] == '\n') line++;
    }

    size_t draw_pos = pos;

    for(uint16_t row = 0; row < VIEW_ROWS; ++row) {
        char line_buf[VIEW_COLUMNS + 1];
        memset(line_buf, 0, sizeof(line_buf));

        size_t count = 0;
        size_t cursor_in_line = SIZE_MAX;

        while(draw_pos < app->len && app->text[draw_pos] != '\n' &&
              count < VIEW_COLUMNS) {
            if(draw_pos == app->cursor) cursor_in_line = count;
            line_buf[count++] = app->text[draw_pos++];
        }

        if(draw_pos == app->cursor) cursor_in_line = count;
        line_buf[count] = '\0';

        canvas_draw_str(canvas, 0, y, line_buf);

        if(row == VIEW_ROWS - 1) {
            canvas_draw_str(canvas, 0, 63, app->connected ? "SAVE:OK" : "NO VGM");
        }

        if(draw_pos < app->len && app->text[draw_pos] == '\n') {
            if(app->cursor == draw_pos) cursor_in_line = count;
            draw_pos++;
        }

        y += 8;
    }

    char status[32];
    snprintf(
        status,
        sizeof(status),
        "%s %u/%u",
        app->connected ? "USB" : "---",
        (unsigned)app->len,
        (unsigned)DOCUMENT_MAX - 1U);

    canvas_draw_str(canvas, 0, 63, status);

    /* Cursor is rendered as a 1-pixel vertical bar when it falls in the
     * currently rendered viewport. */
    size_t cur = app->cursor;
    pos = 0;
    line = 0;

    while(pos < cur) {
        if(app->text[pos++] == '\n') line++;
    }

    if(line >= app->top_line && line < app->top_line + VIEW_ROWS) {
        size_t col = cur - line_start(app->text, cur);
        int x = (int)(col * 6U);
        int cy = (int)((line - app->top_line) * 8U);
        canvas_draw_line(canvas, x, cy, x, cy + 7);
    }
}

static void input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    FuriMessageQueue* queue = context;

    AppEvent app_event = {
        .type = 1,
        .input = *event,
    };
    furi_message_queue_put(queue, &app_event, 0);
}

static bool handshake(UsbTextWriterApp* app) {
    furi_hal_serial_tx(app->serial, host_magic, sizeof(host_magic));
    furi_hal_serial_tx_wait_complete(app->serial);

    size_t matched = 0;
    absolute_time_t deadline = make_timeout_time_ms(1000);

    while(absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
        uint8_t byte = 0;
        if(furi_stream_buffer_receive(app->rx, &byte, 1, 10) == 1) {
            if(byte == ack_magic[matched]) {
                matched++;
                if(matched == sizeof(ack_magic)) {
                    return true;
                }
            } else {
                matched = (byte == ack_magic[0]) ? 1 : 0;
            }
        }
    }

    return false;
}

static void rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    (void)event;

    UsbTextWriterApp* app = context;

    while(furi_hal_serial_async_rx_available(handle)) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(app->rx, &byte, 1, 0);
    }
}

static void handle_key(UsbTextWriterApp* app, uint8_t modifiers, uint8_t usage) {
    bool ctrl = (modifiers & (MOD_LCTRL | MOD_RCTRL)) != 0;
    bool shift = (modifiers & (MOD_LSHIFT | MOD_RSHIFT)) != 0;

    if(usage == KEY_CAPSLOCK) {
        app->caps_lock = !app->caps_lock;
        return;
    }

    if(ctrl) {
        switch(usage) {
        case KEY_A:
            app->cursor = 0;
            ensure_cursor_visible(app);
            return;
        case KEY_S:
            save_file(app);
            return;
        case KEY_C:
        case KEY_V:
        case KEY_X:
            /* Clipboard operations are intentionally not implemented yet. */
            return;
        default:
            break;
        }
    }

    switch(usage) {
    case KEY_BACKSPACE:
        delete_prev(app);
        ensure_cursor_visible(app);
        return;
    case KEY_DELETE:
        delete_at_cursor(app);
        return;
    case KEY_LEFT:
        move_left(app);
        return;
    case KEY_RIGHT:
        move_right(app);
        return;
    case KEY_UP:
        move_vertical(app, -1);
        return;
    case KEY_DOWN:
        move_vertical(app, +1);
        return;
    case KEY_HOME:
        move_home(app);
        return;
    case KEY_END:
        move_end(app);
        return;
    case KEY_PAGEUP:
        app->top_line = (app->top_line > VIEW_ROWS) ?
            (uint16_t)(app->top_line - VIEW_ROWS) : 0;
        return;
    case KEY_PAGEDOWN:
        app->top_line += VIEW_ROWS;
        return;
    default:
        break;
    }

    char ch = usage_to_ascii(usage, modifiers, app->caps_lock);
    if(ch != 0) {
        if(ch == '\t') {
            for(uint8_t i = 0; i < 4; ++i) insert_byte(app, ' ');
        } else if(ch == '\n') {
            insert_byte(app, '\n');
        } else if(isprint((unsigned char)ch)) {
            insert_byte(app, ch);
        }
        ensure_cursor_visible(app);
    }

    (void)shift;
}

static bool load_file(UsbTextWriterApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool ok = false;
    if(storage_file_open(
           file,
           "/ext/apps_data/usb_text_writer/notes.txt",
           FSAM_READ,
           FSOM_OPEN_EXISTING)) {
        uint16_t n = storage_file_read(
            file,
            app->text,
            (uint16_t)(DOCUMENT_MAX - 1U));
        app->len = n;
        app->text[app->len] = '\0';
        app->cursor = app->len;
        storage_file_close(file);
        ok = true;
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

int32_t usb_text_writer_app(void* p) {
    UNUSED(p);

    UsbTextWriterApp app = {0};
    app.text = malloc(DOCUMENT_MAX);
    if(!app.text) return -1;

    app.text[0] = '\0';
    app.running = true;
    load_file(&app);

    app.input_queue = furi_message_queue_alloc(16, sizeof(AppEvent));
    app.rx = furi_stream_buffer_alloc(256, 1);

    app.view_port = view_port_alloc();
    view_port_draw_callback_set(app.view_port, draw_callback, &app);
    view_port_input_callback_set(app.view_port, input_callback, app.input_queue);

    app.gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    /*
     * Take exclusive control of the expansion UART away from the normal
     * expansion-module service while this application is active.
     */
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    app.serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);

    if(app.serial) {
        furi_hal_serial_init(app.serial, UART_BAUD);
        furi_hal_serial_configure_framing(
            app.serial,
            FuriHalSerialDataBits8,
            FuriHalSerialParityNone,
            FuriHalSerialStopBits1);

        furi_hal_serial_async_rx_start(
            app.serial,
            rx_callback,
            &app,
            true);

        app.connected = handshake(&app);
    }

    while(app.running) {
        AppEvent event;

        if(furi_message_queue_get(
               app.input_queue,
               &event,
               50) == FuriStatusOk) {

            if(event.input.type == InputTypeShort) {
                switch(event.input.key) {
                case InputKeyOk:
                    save_file(&app);
                    break;
                case InputKeyBack:
                    save_file(&app);
                    app.running = false;
                    break;
                case InputKeyUp:
                    if(app.top_line > 0) app.top_line--;
                    break;
                case InputKeyDown:
                    app.top_line++;
                    break;
                case InputKeyLeft:
                    move_left(&app);
                    break;
                case InputKeyRight:
                    move_right(&app);
                    break;
                default:
                    break;
                }
            }
        }

        /*
         * The VGM event format is fixed at four bytes:
         * E7, 10, modifiers, usage.
         */
        while(furi_stream_buffer_bytes_available(app.rx) >= 4) {
            uint8_t frame[4];
            if(furi_stream_buffer_receive(app.rx, frame, sizeof(frame), 0) != sizeof(frame)) {
                break;
            }

            if(frame[0] != KBW_FRAME_MAGIC || frame[1] != KBW_EVENT_KEY) {
                app.connected = false;
                continue;
            }

            app.connected = true;
            handle_key(&app, frame[2], frame[3]);
        }

        view_port_update(app.view_port);
    }

    if(app.serial) {
        furi_hal_serial_async_rx_stop(app.serial);
        furi_hal_serial_deinit(app.serial);
        furi_hal_serial_control_release(app.serial);
        app.serial = NULL;
    }

    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app.input_queue);
    furi_stream_buffer_free(app.rx);
    free(app.text);

    return 0;
}
