#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <input/input.h>

#include <storage/storage.h>
#include <expansion/expansion.h>

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_TAG "VgmUsbKeyboard"

/* -------------------------------------------------------------------------- */
/* VGM UART configuration                                                     */
/* -------------------------------------------------------------------------- */

#define UART_BAUD 460800U

/* -------------------------------------------------------------------------- */
/* Editor configuration                                                       */
/* -------------------------------------------------------------------------- */

#define DOCUMENT_MAX 8192U
#define VIEW_COLUMNS 21U
#define VIEW_ROWS 7U

#define APP_DATA_DIR "/ext/apps_data/vgm_usb_keyboard"
#define APP_FILE_PATH APP_DATA_DIR "/notes.txt"

/* -------------------------------------------------------------------------- */
/* VGM keyboard protocol                                                      */
/* -------------------------------------------------------------------------- */

#define KBW_MAGIC_LEN 4U
#define KBW_FRAME_MAGIC 0xE7U
#define KBW_EVENT_KEY 0x10U

static const uint8_t host_magic[KBW_MAGIC_LEN] = {
    'K',
    'B',
    'W',
    '1',
};

static const uint8_t ack_magic[KBW_MAGIC_LEN] = {
    'A',
    'C',
    'K',
    '1',
};

/* -------------------------------------------------------------------------- */
/* USB HID keyboard usage IDs                                                 */
/* -------------------------------------------------------------------------- */

#define KEY_A 0x04U
#define KEY_B 0x05U
#define KEY_C 0x06U
#define KEY_D 0x07U
#define KEY_E 0x08U
#define KEY_F 0x09U
#define KEY_G 0x0AU
#define KEY_H 0x0BU
#define KEY_I 0x0CU
#define KEY_J 0x0DU
#define KEY_K 0x0EU
#define KEY_L 0x0FU
#define KEY_M 0x10U
#define KEY_N 0x11U
#define KEY_O 0x12U
#define KEY_P 0x13U
#define KEY_Q 0x14U
#define KEY_R 0x15U
#define KEY_S 0x16U
#define KEY_T 0x17U
#define KEY_U 0x18U
#define KEY_V 0x19U
#define KEY_W 0x1AU
#define KEY_X 0x1BU
#define KEY_Y 0x1CU
#define KEY_Z 0x1DU

#define KEY_1 0x1EU
#define KEY_2 0x1FU
#define KEY_3 0x20U
#define KEY_4 0x21U
#define KEY_5 0x22U
#define KEY_6 0x23U
#define KEY_7 0x24U
#define KEY_8 0x25U
#define KEY_9 0x26U
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

/* -------------------------------------------------------------------------- */
/* USB HID modifier bitmap                                                    */
/* -------------------------------------------------------------------------- */

#define MOD_LCTRL 0x01U
#define MOD_LSHIFT 0x02U
#define MOD_LALT 0x04U
#define MOD_LGUI 0x08U

#define MOD_RCTRL 0x10U
#define MOD_RSHIFT 0x20U
#define MOD_RALT 0x40U
#define MOD_RGUI 0x80U

/* -------------------------------------------------------------------------- */
/* Application                                                                */
/* -------------------------------------------------------------------------- */

typedef struct {
    ViewPort* view_port;
    Gui* gui;

    FuriMessageQueue* input_queue;
    FuriStreamBuffer* rx;

    FuriHalSerialHandle* serial;

    char* text;

    size_t len;
    size_t cursor;

    uint16_t top_line;

    bool running;
    bool connected;
    bool caps_lock;
} VgmUsbKeyboardApp;

/* -------------------------------------------------------------------------- */
/* Text helpers                                                               */
/* -------------------------------------------------------------------------- */

static size_t text_line_start(const char* text, size_t pos) {
    while(pos > 0 && text[pos - 1] != '\n') {
        pos--;
    }

    return pos;
}

static size_t text_line_end(const char* text, size_t len, size_t pos) {
    while(pos < len && text[pos] != '\n') {
        pos++;
    }

    return pos;
}

static uint16_t cursor_line_number(const VgmUsbKeyboardApp* app) {
    uint16_t line = 0;

    for(size_t i = 0; i < app->cursor; i++) {
        if(app->text[i] == '\n') {
            line++;
        }
    }

    return line;
}

static void ensure_cursor_visible(VgmUsbKeyboardApp* app) {
    const uint16_t line = cursor_line_number(app);

    if(line < app->top_line) {
        app->top_line = line;
    } else if(line >= (uint16_t)(app->top_line + VIEW_ROWS)) {
        app->top_line = (uint16_t)(line - VIEW_ROWS + 1U);
    }
}

/* -------------------------------------------------------------------------- */
/* Editing                                                                    */
/* -------------------------------------------------------------------------- */

static void insert_byte(VgmUsbKeyboardApp* app, char ch) {
    if(app->len >= (DOCUMENT_MAX - 1U)) {
        return;
    }

    memmove(
        app->text + app->cursor + 1U,
        app->text + app->cursor,
        app->len - app->cursor);

    app->text[app->cursor] = ch;

    app->cursor++;
    app->len++;

    app->text[app->len] = '\0';
}

static void delete_previous(VgmUsbKeyboardApp* app) {
    if(app->cursor == 0) {
        return;
    }

    memmove(
        app->text + app->cursor - 1U,
        app->text + app->cursor,
        app->len - app->cursor);

    app->cursor--;
    app->len--;

    app->text[app->len] = '\0';
}

static void delete_at_cursor(VgmUsbKeyboardApp* app) {
    if(app->cursor >= app->len) {
        return;
    }

    memmove(
        app->text + app->cursor,
        app->text + app->cursor + 1U,
        app->len - app->cursor - 1U);

    app->len--;

    app->text[app->len] = '\0';
}

static void move_cursor_left(VgmUsbKeyboardApp* app) {
    if(app->cursor > 0) {
        app->cursor--;
    }

    ensure_cursor_visible(app);
}

static void move_cursor_right(VgmUsbKeyboardApp* app) {
    if(app->cursor < app->len) {
        app->cursor++;
    }

    ensure_cursor_visible(app);
}

static void move_cursor_home(VgmUsbKeyboardApp* app) {
    app->cursor = text_line_start(app->text, app->cursor);
    ensure_cursor_visible(app);
}

static void move_cursor_end(VgmUsbKeyboardApp* app) {
    app->cursor = text_line_end(app->text, app->len, app->cursor);
    ensure_cursor_visible(app);
}

static void move_cursor_vertical(VgmUsbKeyboardApp* app, int direction) {
    const size_t current_start =
        text_line_start(app->text, app->cursor);

    size_t column = app->cursor - current_start;

    if(direction < 0) {
        /*
         * Already on the first line.
         */
        if(current_start == 0) {
            return;
        }

        const size_t previous_end = current_start - 1U;
        const size_t previous_start =
            text_line_start(app->text, previous_end);

        const size_t previous_len =
            previous_end - previous_start;

        if(column > previous_len) {
            column = previous_len;
        }

        app->cursor = previous_start + column;

    } else {
        const size_t current_end =
            text_line_end(app->text, app->len, app->cursor);

        /*
         * Already on the final line.
         */
        if(current_end >= app->len) {
            return;
        }

        const size_t next_start = current_end + 1U;
        const size_t next_end =
            text_line_end(app->text, app->len, next_start);

        const size_t next_len =
            next_end - next_start;

        if(column > next_len) {
            column = next_len;
        }

        app->cursor = next_start + column;
    }

    ensure_cursor_visible(app);
}

/* -------------------------------------------------------------------------- */
/* HID -> ASCII                                                               */
/* -------------------------------------------------------------------------- */

static char usage_to_ascii(
    uint8_t usage,
    uint8_t modifiers,
    bool caps_lock) {

    const bool shift =
        (modifiers & (MOD_LSHIFT | MOD_RSHIFT)) != 0;

    /*
     * Letters.
     */
    if(usage >= KEY_A && usage <= KEY_Z) {
        char c = (char)('a' + (usage - KEY_A));

        if(shift ^ caps_lock) {
            c = (char)toupper((unsigned char)c);
        }

        return c;
    }

    /*
     * Number row.
     */
    if(usage >= KEY_1 && usage <= KEY_0) {
        static const char normal[] = "1234567890";
        static const char shifted[] = "!@#$%^&*()";

        const size_t index = usage - KEY_1;

        return shift ? shifted[index] : normal[index];
    }

    switch(usage) {
    case KEY_SPACE:
        return ' ';

    case KEY_TAB:
        return '\t';

    case KEY_ENTER:
        return '\n';

    case KEY_MINUS:
        return shift ? '_' : '-';

    case KEY_EQUAL:
        return shift ? '+' : '=';

    case KEY_LEFTBRACKET:
        return shift ? '{' : '[';

    case KEY_RIGHTBRACKET:
        return shift ? '}' : ']';

    case KEY_BACKSLASH:
        return shift ? '|' : '\\';

    case KEY_SEMICOLON:
        return shift ? ':' : ';';

    case KEY_APOSTROPHE:
        return shift ? '"' : '\'';

    case KEY_GRAVE:
        return shift ? '~' : '`';

    case KEY_COMMA:
        return shift ? '<' : ',';

    case KEY_DOT:
        return shift ? '>' : '.';

    case KEY_SLASH:
        return shift ? '?' : '/';

    default:
        return 0;
    }
}

/* -------------------------------------------------------------------------- */
/* File I/O                                                                   */
/* -------------------------------------------------------------------------- */

static bool save_file(VgmUsbKeyboardApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    storage_common_mkdir(storage, "/ext/apps_data");
    storage_common_mkdir(storage, APP_DATA_DIR);

    File* file = storage_file_alloc(storage);

    bool success = storage_file_open(
        file,
        APP_FILE_PATH,
        FSAM_WRITE,
        FSOM_CREATE_ALWAYS);

    if(success) {
        const size_t written =
            storage_file_write(file, app->text, app->len);

        success = (written == app->len);

        storage_file_close(file);
    }

    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);

    return success;
}

static bool load_file(VgmUsbKeyboardApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* file = storage_file_alloc(storage);

    bool success = storage_file_open(
        file,
        APP_FILE_PATH,
        FSAM_READ,
        FSOM_OPEN_EXISTING);

    if(success) {
        const size_t read_count =
            storage_file_read(
                file,
                app->text,
                DOCUMENT_MAX - 1U);

        app->len = read_count;

        if(app->len >= DOCUMENT_MAX) {
            app->len = DOCUMENT_MAX - 1U;
        }

        app->text[app->len] = '\0';
        app->cursor = app->len;

        storage_file_close(file);
    }

    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);

    return success;
}

/* -------------------------------------------------------------------------- */
/* GUI                                                                        */
/* -------------------------------------------------------------------------- */

static void draw_callback(Canvas* canvas, void* context) {
    VgmUsbKeyboardApp* app = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    /*
     * Locate first character belonging to top_line.
     */
    size_t draw_pos = 0;
    uint16_t line = 0;

    while(draw_pos < app->len && line < app->top_line) {
        if(app->text[draw_pos] == '\n') {
            line++;
        }

        draw_pos++;
    }

    /*
     * Draw document.
     *
     * Leave bottom 8 pixels for status text.
     */
    for(uint16_t row = 0; row < VIEW_ROWS; row++) {
        const int y = 7 + ((int)row * 8);

        char line_buffer[VIEW_COLUMNS + 1U];

        memset(line_buffer, 0, sizeof(line_buffer));

        size_t count = 0;

        while(
            draw_pos < app->len &&
            app->text[draw_pos] != '\n' &&
            count < VIEW_COLUMNS) {

            char c = app->text[draw_pos];

            /*
             * Render tabs as spaces.
             */
            if(c == '\t') {
                c = ' ';
            }

            line_buffer[count] = c;

            count++;
            draw_pos++;
        }

        line_buffer[count] = '\0';

        canvas_draw_str(
            canvas,
            0,
            y,
            line_buffer);

        /*
         * If the physical line is longer than VIEW_COLUMNS, skip the
         * invisible remainder.
         */
        while(
            draw_pos < app->len &&
            app->text[draw_pos] != '\n') {

            draw_pos++;
        }

        if(
            draw_pos < app->len &&
            app->text[draw_pos] == '\n') {

            draw_pos++;
        }
    }

    /*
     * Cursor position.
     */
    const uint16_t cursor_line =
        cursor_line_number(app);

    if(
        cursor_line >= app->top_line &&
        cursor_line < (uint16_t)(app->top_line + VIEW_ROWS)) {

        const size_t start =
            text_line_start(app->text, app->cursor);

        const size_t column =
            app->cursor - start;

        /*
         * Only draw it if it's visible horizontally.
         */
        if(column < VIEW_COLUMNS) {
            const int x = (int)(column * 6U);
            const int row =
                (int)(cursor_line - app->top_line);

            const int y0 = row * 8;
            const int y1 = y0 + 7;

            canvas_draw_line(
                canvas,
                x,
                y0,
                x,
                y1);
        }
    }

    /*
     * Status.
     */
    char status[32];

    snprintf(
        status,
        sizeof(status),
        "%s %u/%u",
        app->connected ? "VGM" : "NO VGM",
        (unsigned int)app->len,
        (unsigned int)(DOCUMENT_MAX - 1U));

    canvas_draw_str(
        canvas,
        0,
        63,
        status);
}

/* -------------------------------------------------------------------------- */
/* Flipper buttons                                                            */
/* -------------------------------------------------------------------------- */

static void input_callback(InputEvent* event, void* context) {
    FuriMessageQueue* queue = context;

    furi_message_queue_put(
        queue,
        event,
        0);
}

/* -------------------------------------------------------------------------- */
/* Serial                                                                     */
/* -------------------------------------------------------------------------- */

static void serial_rx_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {

    UNUSED(event);

    VgmUsbKeyboardApp* app = context;

    while(furi_hal_serial_async_rx_available(handle)) {
        const uint8_t byte =
            furi_hal_serial_async_rx(handle);

        furi_stream_buffer_send(
            app->rx,
            &byte,
            1,
            0);
    }
}

/* -------------------------------------------------------------------------- */
/* VGM handshake                                                              */
/* -------------------------------------------------------------------------- */

static bool handshake(VgmUsbKeyboardApp* app) {
    furi_stream_buffer_reset(app->rx);

    furi_hal_serial_tx(
        app->serial,
        host_magic,
        sizeof(host_magic));

    furi_hal_serial_tx_wait_complete(
        app->serial);

    size_t matched = 0;

    const uint32_t start_tick =
        furi_get_tick();

    const uint32_t timeout_ticks =
        furi_ms_to_ticks(1000);

    while(
        (uint32_t)(furi_get_tick() - start_tick) <
        timeout_ticks) {

        uint8_t byte = 0;

        const size_t received =
            furi_stream_buffer_receive(
                app->rx,
                &byte,
                1,
                furi_ms_to_ticks(10));

        if(received != 1) {
            continue;
        }

        if(byte == ack_magic[matched]) {
            matched++;

            if(matched == sizeof(ack_magic)) {
                return true;
            }
        } else {
            matched =
                (byte == ack_magic[0]) ? 1U : 0U;
        }
    }

    return false;
}

/* -------------------------------------------------------------------------- */
/* Keyboard event handling                                                    */
/* -------------------------------------------------------------------------- */

static void handle_key(
    VgmUsbKeyboardApp* app,
    uint8_t modifiers,
    uint8_t usage) {

    const bool ctrl =
        (modifiers & (MOD_LCTRL | MOD_RCTRL)) != 0;

    /*
     * Lock keys.
     */
    if(usage == KEY_CAPSLOCK) {
        app->caps_lock = !app->caps_lock;
        return;
    }

    /*
     * Control shortcuts.
     */
    if(ctrl) {
        switch(usage) {
        case KEY_A:
            /*
             * Currently Ctrl+A means jump to beginning.
             *
             * Selection support can be added later.
             */
            app->cursor = 0;
            ensure_cursor_visible(app);
            return;

        case KEY_S:
            save_file(app);
            return;

        case KEY_C:
        case KEY_V:
        case KEY_X:
            /*
             * Clipboard is intentionally not implemented yet.
             */
            return;

        default:
            break;
        }
    }

    /*
     * Editing/navigation keys.
     */
    switch(usage) {
    case KEY_BACKSPACE:
        delete_previous(app);
        ensure_cursor_visible(app);
        return;

    case KEY_DELETE:
        delete_at_cursor(app);
        ensure_cursor_visible(app);
        return;

    case KEY_LEFT:
        move_cursor_left(app);
        return;

    case KEY_RIGHT:
        move_cursor_right(app);
        return;

    case KEY_UP:
        move_cursor_vertical(app, -1);
        return;

    case KEY_DOWN:
        move_cursor_vertical(app, 1);
        return;

    case KEY_HOME:
        move_cursor_home(app);
        return;

    case KEY_END:
        move_cursor_end(app);
        return;

    case KEY_PAGEUP:
        if(app->top_line > VIEW_ROWS) {
            app->top_line =
                (uint16_t)(app->top_line - VIEW_ROWS);
        } else {
            app->top_line = 0;
        }
        return;

    case KEY_PAGEDOWN:
        app->top_line =
            (uint16_t)(app->top_line + VIEW_ROWS);
        return;

    case KEY_ESC:
        return;

    default:
        break;
    }

    /*
     * Printable character.
     */
    const char ch =
        usage_to_ascii(
            usage,
            modifiers,
            app->caps_lock);

    if(ch == 0) {
        return;
    }

    if(ch == '\t') {
        /*
         * Convert Tab to four spaces.
         */
        for(uint8_t i = 0; i < 4U; i++) {
            insert_byte(app, ' ');
        }

    } else if(ch == '\n') {
        insert_byte(app, '\n');

    } else if(isprint((unsigned char)ch)) {
        insert_byte(app, ch);
    }

    ensure_cursor_visible(app);
}

/* -------------------------------------------------------------------------- */
/* UART frame parser                                                          */
/* -------------------------------------------------------------------------- */

static void process_serial_data(VgmUsbKeyboardApp* app) {
    /*
     * Protocol:
     *
     *   byte 0: 0xE7
     *   byte 1: 0x10
     *   byte 2: HID modifiers
     *   byte 3: HID usage ID
     *
     * Parse byte-by-byte so the receiver can recover if it becomes
     * misaligned.
     */

    static uint8_t frame[4];
    static size_t frame_pos = 0;

    uint8_t byte = 0;

    while(
        furi_stream_buffer_receive(
            app->rx,
            &byte,
            1,
            0) == 1) {

        if(frame_pos == 0) {
            if(byte != KBW_FRAME_MAGIC) {
                continue;
            }

            frame[frame_pos++] = byte;
            continue;
        }

        if(frame_pos == 1) {
            if(byte != KBW_EVENT_KEY) {
                /*
                 * Could this byte itself be the next magic byte?
                 */
                if(byte == KBW_FRAME_MAGIC) {
                    frame[0] = byte;
                    frame_pos = 1;
                } else {
                    frame_pos = 0;
                }

                continue;
            }

            frame[frame_pos++] = byte;
            continue;
        }

        frame[frame_pos++] = byte;

        if(frame_pos == sizeof(frame)) {
            app->connected = true;

            handle_key(
                app,
                frame[2],
                frame[3]);

            frame_pos = 0;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Main                                                                       */
/* -------------------------------------------------------------------------- */

int32_t vgm_usb_keyboard_app(void* p) {
    UNUSED(p);

    VgmUsbKeyboardApp app = {0};

    /*
     * Allocate editor buffer.
     */
    app.text = malloc(DOCUMENT_MAX);

    if(app.text == NULL) {
        FURI_LOG_E(
            APP_TAG,
            "Failed to allocate document buffer");

        return -1;
    }

    app.text[0] = '\0';
    app.running = true;

    /*
     * Load previous document if present.
     */
    load_file(&app);

    /*
     * Event/serial queues.
     */
    app.input_queue =
        furi_message_queue_alloc(
            16,
            sizeof(InputEvent));

    app.rx =
        furi_stream_buffer_alloc(
            512,
            1);

    /*
     * GUI.
     */
    app.view_port = view_port_alloc();

    view_port_draw_callback_set(
        app.view_port,
        draw_callback,
        &app);

    view_port_input_callback_set(
        app.view_port,
        input_callback,
        app.input_queue);

    app.gui =
        furi_record_open(RECORD_GUI);

    gui_add_view_port(
        app.gui,
        app.view_port,
        GuiLayerFullscreen);

    /*
     * Disable the standard expansion worker while we directly use the
     * expansion UART.
     */
    Expansion* expansion =
        furi_record_open(RECORD_EXPANSION);

    expansion_disable(expansion);

    /*
     * Acquire USART connected to the expansion interface.
     */
    app.serial =
        furi_hal_serial_control_acquire(
            FuriHalSerialIdUsart);

    if(app.serial != NULL) {
        furi_hal_serial_init(
            app.serial,
            UART_BAUD);

        furi_hal_serial_configure_framing(
            app.serial,
            FuriHalSerialDataBits8,
            FuriHalSerialParityNone,
            FuriHalSerialStopBits1);

        furi_hal_serial_async_rx_start(
            app.serial,
            serial_rx_callback,
            &app,
            true);

        app.connected =
            handshake(&app);

        if(app.connected) {
            FURI_LOG_I(
                APP_TAG,
                "VGM connected");
        } else {
            FURI_LOG_W(
                APP_TAG,
                "VGM handshake failed");
        }

    } else {
        FURI_LOG_E(
            APP_TAG,
            "Could not acquire expansion USART");

        app.connected = false;
    }

    /*
     * Main loop.
     */
    while(app.running) {
        InputEvent input_event;

        const FuriStatus status =
            furi_message_queue_get(
                app.input_queue,
                &input_event,
                furi_ms_to_ticks(50));

        if(status == FuriStatusOk) {
            if(input_event.type == InputTypeShort) {
                switch(input_event.key) {
                case InputKeyOk:
                    save_file(&app);
                    break;

                case InputKeyBack:
                    save_file(&app);
                    app.running = false;
                    break;

                case InputKeyUp:
                    if(app.top_line > 0) {
                        app.top_line--;
                    }
                    break;

                case InputKeyDown:
                    app.top_line++;
                    break;

                case InputKeyLeft:
                    move_cursor_left(&app);
                    break;

                case InputKeyRight:
                    move_cursor_right(&app);
                    break;

                default:
                    break;
                }
            }
        }

        /*
         * Process VGM keyboard packets.
         */
        process_serial_data(&app);

        /*
         * Redraw.
         */
        view_port_update(
            app.view_port);
    }

    /*
     * Save one final time.
     */
    save_file(&app);

    /*
     * Serial cleanup.
     */
    if(app.serial != NULL) {
        furi_hal_serial_async_rx_stop(
            app.serial);

        furi_hal_serial_deinit(
            app.serial);

        furi_hal_serial_control_release(
            app.serial);

        app.serial = NULL;
    }

    /*
     * Restore normal expansion handling.
     */
    expansion_enable(expansion);

    furi_record_close(
        RECORD_EXPANSION);

    /*
     * GUI cleanup.
     */
    gui_remove_view_port(
        app.gui,
        app.view_port);

    view_port_free(
        app.view_port);

    furi_record_close(
        RECORD_GUI);

    /*
     * Queue cleanup.
     */
    furi_message_queue_free(
        app.input_queue);

    furi_stream_buffer_free(
        app.rx);

    free(app.text);

    return 0;
}
