#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <tchar.h>
#include <stdlib.h>
#include <string.h>

#include "resource.h"

#define APP_CLASS_NAME TEXT("CEBrainfuckWindow")
#define APP_TITLE TEXT("CE Brainfuck")
#define BF_TAPE_SIZE 65536
#define WM_APP_RUN_COMPLETE (WM_APP + 1)

#ifndef ARRAYSIZE
#define ARRAYSIZE(values) (sizeof(values) / sizeof((values)[0]))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

typedef enum RUN_STATUS_TAG {
    RUN_STATUS_OK = 0,
    RUN_STATUS_CANCELLED = 1,
    RUN_STATUS_MISMATCHED_BRACKETS = 2,
    RUN_STATUS_OUT_OF_MEMORY = 3
} RUN_STATUS;

typedef struct BYTE_BUFFER_TAG {
    unsigned char* data;
    size_t length;
    size_t capacity;
} BYTE_BUFFER;

typedef struct RUN_RESULT_TAG {
    TCHAR* text;
    RUN_STATUS status;
} RUN_RESULT;

typedef struct RUN_REQUEST_TAG {
    HWND window;
    volatile LONG* cancel_flag;
    unsigned char* code;
    size_t code_length;
    unsigned char* input;
    size_t input_length;
} RUN_REQUEST;

typedef struct APP_STATE_TAG {
    HINSTANCE instance;
    HWND window;
    HICON icon;
    HICON small_icon;
    HACCEL accelerator;
    HWND command_bar;
    HWND code_label;
    HWND code_edit;
    HWND input_label;
    HWND input_edit;
    HWND output_label;
    HWND output_edit;
    HMENU menu;
    HFONT ui_font;
    HFONT mono_font;
    HANDLE run_thread;
    volatile LONG cancel_requested;
    int is_running;
} APP_STATE;

static APP_STATE g_app;
static LRESULT CALLBACK App_WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);

static void Buffer_Init(BYTE_BUFFER* buffer) {
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

static void Buffer_Free(BYTE_BUFFER* buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

static int Buffer_Reserve(BYTE_BUFFER* buffer, size_t needed_length) {
    size_t new_capacity;
    unsigned char* new_data;

    if (needed_length <= buffer->capacity) {
        return 1;
    }

    new_capacity = buffer->capacity == 0 ? 256 : buffer->capacity;
    while (new_capacity < needed_length) {
        if (new_capacity > ((size_t)-1) / 2) {
            new_capacity = needed_length;
            break;
        }
        new_capacity *= 2;
    }

    new_data = (unsigned char*)realloc(buffer->data, new_capacity);
    if (!new_data) {
        return 0;
    }

    buffer->data = new_data;
    buffer->capacity = new_capacity;
    return 1;
}

static int Buffer_AppendByte(BYTE_BUFFER* buffer, unsigned char value) {
    if (!Buffer_Reserve(buffer, buffer->length + 1)) {
        return 0;
    }
    buffer->data[buffer->length++] = value;
    return 1;
}

static unsigned char* App_GetEditBytes(HWND edit, size_t* length_out) {
    int text_length;
    TCHAR* text;
    unsigned char* bytes;
    int index;

    *length_out = 0;
    text_length = GetWindowTextLength(edit);
    text = (TCHAR*)malloc((text_length + 1) * sizeof(TCHAR));
    if (!text) {
        return NULL;
    }

    GetWindowText(edit, text, text_length + 1);
    bytes = (unsigned char*)malloc((text_length > 0 ? text_length : 1) * sizeof(unsigned char));
    if (!bytes) {
        free(text);
        return NULL;
    }

    for (index = 0; index < text_length; ++index) {
        bytes[index] = (unsigned char)(text[index] & 0xff);
    }

    *length_out = (size_t)text_length;
    free(text);
    return bytes;
}

static TCHAR* App_TextFromBytes(const unsigned char* bytes, size_t length, int normalize_newlines) {
    size_t max_chars;
    TCHAR* text;
    size_t source_index;
    size_t target_index;
    unsigned char previous_byte;

    max_chars = normalize_newlines ? (length * 2 + 1) : (length + 1);
    text = (TCHAR*)malloc(max_chars * sizeof(TCHAR));
    if (!text) {
        return NULL;
    }

    target_index = 0;
    previous_byte = 0;
    for (source_index = 0; source_index < length; ++source_index) {
        unsigned char value;

        value = bytes[source_index];
        if (normalize_newlines && value == '\n' && previous_byte != '\r') {
            text[target_index++] = TEXT('\r');
        }

        if (value == 0) {
            text[target_index++] = TEXT('?');
        } else {
            text[target_index++] = (TCHAR)value;
        }

        previous_byte = value;
    }

    text[target_index] = 0;
    return text;
}

static TCHAR* App_DuplicateText(LPCTSTR source) {
    size_t length;
    TCHAR* duplicate;

    length = _tcslen(source);
    duplicate = (TCHAR*)malloc((length + 1) * sizeof(TCHAR));
    if (!duplicate) {
        return NULL;
    }

    memcpy(duplicate, source, (length + 1) * sizeof(TCHAR));
    return duplicate;
}

static TCHAR* App_CombineText(TCHAR* left, LPCTSTR right) {
    size_t left_length;
    size_t right_length;
    TCHAR* combined;

    left_length = left ? _tcslen(left) : 0;
    right_length = _tcslen(right);
    combined = (TCHAR*)malloc((left_length + right_length + 1) * sizeof(TCHAR));
    if (!combined) {
        free(left);
        return NULL;
    }

    if (left_length > 0) {
        memcpy(combined, left, left_length * sizeof(TCHAR));
    }
    memcpy(combined + left_length, right, (right_length + 1) * sizeof(TCHAR));
    free(left);
    return combined;
}

static RUN_STATUS App_RunInterpreter(const unsigned char* code, size_t code_length, const unsigned char* input, size_t input_length, volatile LONG* cancel_flag, BYTE_BUFFER* output) {
    unsigned char tape[BF_TAPE_SIZE];
    unsigned char* filtered_code;
    size_t filtered_length;
    size_t index;
    size_t input_position;
    size_t program_counter;
    int tape_position;

    memset(tape, 0, sizeof(tape));
    filtered_code = (unsigned char*)malloc(code_length > 0 ? code_length : 1);
    if (!filtered_code) {
        return RUN_STATUS_OUT_OF_MEMORY;
    }

    filtered_length = 0;
    for (index = 0; index < code_length; ++index) {
        switch (code[index]) {
            case '>':
            case '<':
            case '+':
            case '-':
            case ',':
            case '.':
            case '[':
            case ']':
                filtered_code[filtered_length++] = code[index];
                break;
            default:
                break;
        }
    }

    tape_position = 0;
    input_position = 0;
    program_counter = 0;

    while (program_counter < filtered_length) {
        if (*cancel_flag) {
            free(filtered_code);
            return RUN_STATUS_CANCELLED;
        }

        switch (filtered_code[program_counter]) {
            case '>':
                tape_position++;
                if (tape_position >= BF_TAPE_SIZE) {
                    tape_position = 0;
                }
                program_counter++;
                break;
            case '<':
                if (tape_position == 0) {
                    tape_position = BF_TAPE_SIZE - 1;
                } else {
                    tape_position--;
                }
                program_counter++;
                break;
            case '+':
                tape[tape_position]++;
                program_counter++;
                break;
            case '-':
                tape[tape_position]--;
                program_counter++;
                break;
            case ',':
                if (input_position < input_length) {
                    tape[tape_position] = input[input_position++];
                } else {
                    tape[tape_position] = 0;
                }
                program_counter++;
                break;
            case '.':
                if (!Buffer_AppendByte(output, tape[tape_position])) {
                    free(filtered_code);
                    return RUN_STATUS_OUT_OF_MEMORY;
                }
                program_counter++;
                break;
            case '[':
                if (tape[tape_position] == 0) {
                    size_t bracket_depth;

                    bracket_depth = 1;
                    while (bracket_depth > 0) {
                        program_counter++;
                        if (program_counter >= filtered_length) {
                            free(filtered_code);
                            return RUN_STATUS_MISMATCHED_BRACKETS;
                        }
                        if (filtered_code[program_counter] == '[') {
                            bracket_depth++;
                        } else if (filtered_code[program_counter] == ']') {
                            bracket_depth--;
                        }
                    }
                }
                program_counter++;
                break;
            case ']':
                if (tape[tape_position] != 0) {
                    size_t bracket_depth;

                    bracket_depth = 1;
                    while (bracket_depth > 0) {
                        if (program_counter == 0) {
                            free(filtered_code);
                            return RUN_STATUS_MISMATCHED_BRACKETS;
                        }
                        program_counter--;
                        if (filtered_code[program_counter] == ']') {
                            bracket_depth++;
                        } else if (filtered_code[program_counter] == '[') {
                            bracket_depth--;
                        }
                    }
                } else {
                    program_counter++;
                }
                break;
        }
    }

    free(filtered_code);
    return RUN_STATUS_OK;
}

static void RunResult_Free(RUN_RESULT* result) {
    if (!result) {
        return;
    }
    free(result->text);
    free(result);
}

static DWORD WINAPI App_RunThreadProc(LPVOID parameter) {
    static const TCHAR mismatch_text[] = TEXT("\r\n\r\nMismatched brackets.");
    static const TCHAR memory_text[] = TEXT("Out of memory while running the program.");
    RUN_REQUEST* request;
    RUN_RESULT* result;
    BYTE_BUFFER output;
    RUN_STATUS status;
    volatile LONG* cancel_flag;

    request = (RUN_REQUEST*)parameter;
    cancel_flag = request->cancel_flag;
    result = (RUN_RESULT*)calloc(1, sizeof(RUN_RESULT));
    Buffer_Init(&output);

    if (!result) {
        free(request->code);
        free(request->input);
        free(request);
        return 0;
    }

    status = App_RunInterpreter(
        request->code,
        request->code_length,
        request->input,
        request->input_length,
        request->cancel_flag,
        &output
    );

    result->status = status;
    if (status != RUN_STATUS_CANCELLED) {
        result->text = App_TextFromBytes(output.data, output.length, 1);
        if (!result->text) {
            result->status = RUN_STATUS_OUT_OF_MEMORY;
            result->text = App_DuplicateText(memory_text);
        } else if (status == RUN_STATUS_MISMATCHED_BRACKETS) {
            result->text = App_CombineText(result->text, mismatch_text);
            if (!result->text) {
                result->status = RUN_STATUS_OUT_OF_MEMORY;
                result->text = App_DuplicateText(memory_text);
            }
        }
    }

    if (result->status == RUN_STATUS_OUT_OF_MEMORY && !result->text) {
        result->text = App_DuplicateText(memory_text);
    }

    Buffer_Free(&output);
    free(request->code);
    free(request->input);
    free(request);

    if (status == RUN_STATUS_CANCELLED || *cancel_flag) {
        RunResult_Free(result);
        return 0;
    }

    if (!PostMessage(g_app.window, WM_APP_RUN_COMPLETE, 0, (LPARAM)result)) {
        RunResult_Free(result);
    }

    return 0;
}

static HFONT App_CreateMonospaceFont(void) {
    LOGFONT log_font;
    HFONT font;

    memset(&log_font, 0, sizeof(log_font));
    log_font.lfHeight = -12;
    log_font.lfWeight = FW_NORMAL;
    log_font.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    _tcscpy(log_font.lfFaceName, TEXT("Courier New"));

    font = CreateFontIndirect(&log_font);

    if (!font) {
        font = (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
    }
    return font;
}

static HACCEL App_CreateAccelerators(void) {
    ACCEL entries[] = {
        { FVIRTKEY | FCONTROL, 'N', ID_FILE_NEW },
        { FVIRTKEY | FCONTROL, 'O', ID_FILE_OPEN },
        { FVIRTKEY | FCONTROL, 'R', ID_FILE_RUN },
        { FVIRTKEY | FALT, VK_F4, ID_FILE_EXIT },
        { FVIRTKEY | FCONTROL, 'X', ID_EDIT_CUT },
        { FVIRTKEY | FCONTROL, 'C', ID_EDIT_COPY },
        { FVIRTKEY | FCONTROL, 'V', ID_EDIT_PASTE },
        { FVIRTKEY | FCONTROL, 'A', ID_EDIT_SELECT_ALL }
    };

    return CreateAcceleratorTable(entries, (int)ARRAYSIZE(entries));
}

static void App_ApplyFonts(APP_STATE* app) {
    HFONT label_font;

    label_font = app->ui_font ? app->ui_font : (HFONT)GetStockObject(SYSTEM_FONT);
    SendMessage(app->code_label, WM_SETFONT, (WPARAM)label_font, TRUE);
    SendMessage(app->input_label, WM_SETFONT, (WPARAM)label_font, TRUE);
    SendMessage(app->output_label, WM_SETFONT, (WPARAM)label_font, TRUE);
    SendMessage(app->code_edit, WM_SETFONT, (WPARAM)app->mono_font, TRUE);
    SendMessage(app->input_edit, WM_SETFONT, (WPARAM)app->mono_font, TRUE);
    SendMessage(app->output_edit, WM_SETFONT, (WPARAM)app->mono_font, TRUE);
}

static void App_DestroyThreadHandle(APP_STATE* app) {
    if (app->run_thread) {
        CloseHandle(app->run_thread);
        app->run_thread = NULL;
    }
}

static void App_SetRunning(APP_STATE* app, int is_running) {
    app->is_running = is_running;
    if (app->command_bar) {
        CommandBar_DrawMenuBar(app->command_bar, 0);
    }
}

static void App_UpdateMenuState(APP_STATE* app) {
    HMENU file_menu;
    HWND focused_window;
    UINT output_state;
    UINT edit_state;

    if (!app->menu) {
        return;
    }

    file_menu = GetSubMenu(app->menu, 0);
    if (file_menu) {
        output_state = GetWindowTextLength(app->output_edit) > 0 ? MF_ENABLED : (MF_DISABLED | MF_GRAYED);
        EnableMenuItem(file_menu, ID_FILE_RUN, MF_BYCOMMAND | (app->is_running ? MF_GRAYED : MF_ENABLED));
        EnableMenuItem(file_menu, ID_FILE_COPY_OUTPUT, MF_BYCOMMAND | output_state);
        EnableMenuItem(file_menu, ID_FILE_CLEAR_OUTPUT, MF_BYCOMMAND | output_state);
    }

    focused_window = GetFocus();
    edit_state = (focused_window == app->code_edit || focused_window == app->input_edit || focused_window == app->output_edit)
        ? MF_ENABLED
        : (MF_DISABLED | MF_GRAYED);

    EnableMenuItem(GetSubMenu(app->menu, 1), ID_EDIT_CUT, MF_BYCOMMAND | edit_state);
    EnableMenuItem(GetSubMenu(app->menu, 1), ID_EDIT_COPY, MF_BYCOMMAND | edit_state);
    EnableMenuItem(GetSubMenu(app->menu, 1), ID_EDIT_PASTE, MF_BYCOMMAND | edit_state);
    EnableMenuItem(GetSubMenu(app->menu, 1), ID_EDIT_SELECT_ALL, MF_BYCOMMAND | edit_state);
}

static void App_Layout(APP_STATE* app) {
    RECT client;
    int command_height;
    int margin;
    int label_height;
    int edit_gap;
    int section_gap;
    int top;
    int width;
    int usable_height;
    int edit_space;
    int code_height;
    int input_height;
    int y;

    GetClientRect(app->window, &client);

    margin = 10;
    label_height = 18;
    edit_gap = 4;
    section_gap = 10;

    command_height = 0;
    if (app->command_bar) {
        command_height = CommandBar_Height(app->command_bar);
        MoveWindow(app->command_bar, 0, 0, client.right - client.left, command_height, TRUE);
    }

    top = command_height + margin;
    width = max(10, client.right - margin * 2);
    usable_height = max(150, client.bottom - top - margin);
    edit_space = usable_height - (label_height * 3) - (edit_gap * 3) - (section_gap * 2);
    if (edit_space < 120) {
        edit_space = 120;
    }

    code_height = max(70, (edit_space * 40) / 100);
    input_height = max(50, (edit_space * 24) / 100);
    y = top;
    MoveWindow(app->code_label, margin, y, width, label_height, TRUE);
    y += label_height + edit_gap;
    MoveWindow(app->code_edit, margin, y, width, code_height, TRUE);

    y += code_height + section_gap;
    MoveWindow(app->input_label, margin, y, width, label_height, TRUE);
    y += label_height + edit_gap;
    MoveWindow(app->input_edit, margin, y, width, input_height, TRUE);

    y += input_height + section_gap;
    MoveWindow(app->output_label, margin, y, width, label_height, TRUE);
    y += label_height + edit_gap;
    MoveWindow(app->output_edit, margin, y, width, max(40, client.bottom - y - margin), TRUE);
}

static HWND App_GetFocusedEdit(APP_STATE* app) {
    HWND focused_window;

    focused_window = GetFocus();
    if (focused_window == app->code_edit || focused_window == app->input_edit || focused_window == app->output_edit) {
        return focused_window;
    }
    return NULL;
}

static int App_CopyWindowText(HWND owner, HWND source) {
    int text_length;
    HGLOBAL memory;
    TCHAR* text;

    text_length = GetWindowTextLength(source);
    if (text_length <= 0) {
        return 0;
    }

    memory = GlobalAlloc(GMEM_MOVEABLE, (text_length + 1) * sizeof(TCHAR));
    if (!memory) {
        return 0;
    }

    text = (TCHAR*)GlobalLock(memory);
    if (!text) {
        GlobalFree(memory);
        return 0;
    }

    GetWindowText(source, text, text_length + 1);
    GlobalUnlock(memory);

    if (!OpenClipboard(owner)) {
        GlobalFree(memory);
        return 0;
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, memory);
    CloseClipboard();
    return 1;
}

static void App_HandleOpen(APP_STATE* app) {
    OPENFILENAME file_name;
    TCHAR path[MAX_PATH];

    memset(&file_name, 0, sizeof(file_name));
    memset(path, 0, sizeof(path));

    file_name.lStructSize = sizeof(file_name);
    file_name.hwndOwner = app->window;
    file_name.lpstrFile = path;
    file_name.nMaxFile = ARRAYSIZE(path);
    file_name.lpstrFilter = TEXT("Brainfuck Source (*.bf;*.b)\0*.bf;*.b\0All Files (*.*)\0*.*\0");
    file_name.nFilterIndex = 1;
    file_name.lpstrTitle = TEXT("Open Brainfuck Source");
    file_name.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileName(&file_name)) {
        HANDLE file_handle;
        DWORD file_size;
        DWORD bytes_read;
        unsigned char* file_bytes;
        TCHAR* text;

        file_handle = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file_handle == INVALID_HANDLE_VALUE) {
            MessageBox(app->window, TEXT("Could not open the selected file."), APP_TITLE, MB_OK | MB_ICONERROR);
            return;
        }

        file_size = GetFileSize(file_handle, NULL);
        if (file_size == INVALID_FILE_SIZE) {
            CloseHandle(file_handle);
            MessageBox(app->window, TEXT("Could not determine the file size."), APP_TITLE, MB_OK | MB_ICONERROR);
            return;
        }

        file_bytes = (unsigned char*)malloc(file_size > 0 ? file_size : 1);
        if (!file_bytes) {
            CloseHandle(file_handle);
            MessageBox(app->window, TEXT("Out of memory while opening the file."), APP_TITLE, MB_OK | MB_ICONERROR);
            return;
        }

        if (!ReadFile(file_handle, file_bytes, file_size, &bytes_read, NULL)) {
            free(file_bytes);
            CloseHandle(file_handle);
            MessageBox(app->window, TEXT("Could not read the selected file."), APP_TITLE, MB_OK | MB_ICONERROR);
            return;
        }

        CloseHandle(file_handle);
        text = App_TextFromBytes(file_bytes, bytes_read, 1);
        free(file_bytes);

        if (!text) {
            MessageBox(app->window, TEXT("Out of memory while decoding the file."), APP_TITLE, MB_OK | MB_ICONERROR);
            return;
        }

        SetWindowText(app->code_edit, text);
        SetFocus(app->code_edit);
        free(text);
    }
}

static void App_StartRun(APP_STATE* app) {
    RUN_REQUEST* request;
    size_t code_length;
    size_t input_length;

    if (app->is_running) {
        return;
    }

    request = (RUN_REQUEST*)calloc(1, sizeof(RUN_REQUEST));
    if (!request) {
        MessageBox(app->window, TEXT("Out of memory while starting the interpreter."), APP_TITLE, MB_OK | MB_ICONERROR);
        return;
    }

    request->code = App_GetEditBytes(app->code_edit, &code_length);
    request->input = App_GetEditBytes(app->input_edit, &input_length);
    if (!request->code || !request->input) {
        free(request->code);
        free(request->input);
        free(request);
        MessageBox(app->window, TEXT("Out of memory while collecting input."), APP_TITLE, MB_OK | MB_ICONERROR);
        return;
    }

    request->window = app->window;
    request->cancel_flag = &app->cancel_requested;
    request->code_length = code_length;
    request->input_length = input_length;

    InterlockedExchange(&app->cancel_requested, 0);
    SetWindowText(app->output_edit, TEXT(""));
    app->run_thread = CreateThread(NULL, 0, App_RunThreadProc, request, 0, NULL);
    if (!app->run_thread) {
        free(request->code);
        free(request->input);
        free(request);
        MessageBox(app->window, TEXT("Could not create the interpreter thread."), APP_TITLE, MB_OK | MB_ICONERROR);
        return;
    }

    App_SetRunning(app, 1);
}

static void App_HandleRunResult(APP_STATE* app, RUN_RESULT* result) {
    if (result && result->text) {
        SetWindowText(app->output_edit, result->text);
    } else {
        SetWindowText(app->output_edit, TEXT(""));
    }

    RunResult_Free(result);
    App_DestroyThreadHandle(app);
    App_SetRunning(app, 0);
}

static void App_HandleEditCommand(APP_STATE* app, UINT command_id) {
    HWND edit;

    edit = App_GetFocusedEdit(app);
    if (!edit) {
        return;
    }

    switch (command_id) {
        case ID_EDIT_CUT:
            SendMessage(edit, WM_CUT, 0, 0);
            break;
        case ID_EDIT_COPY:
            SendMessage(edit, WM_COPY, 0, 0);
            break;
        case ID_EDIT_PASTE:
            SendMessage(edit, WM_PASTE, 0, 0);
            break;
        case ID_EDIT_SELECT_ALL:
            SendMessage(edit, EM_SETSEL, 0, -1);
            break;
    }
}

static void App_HandleCommand(APP_STATE* app, UINT command_id) {
    switch (command_id) {
        case ID_FILE_NEW:
            SetWindowText(app->code_edit, TEXT(""));
            SetWindowText(app->input_edit, TEXT(""));
            SetWindowText(app->output_edit, TEXT(""));
            SetFocus(app->code_edit);
            break;
        case ID_FILE_OPEN:
            App_HandleOpen(app);
            break;
        case ID_FILE_RUN:
            App_StartRun(app);
            break;
        case ID_FILE_COPY_OUTPUT:
            App_CopyWindowText(app->window, app->output_edit);
            break;
        case ID_FILE_CLEAR_OUTPUT:
            SetWindowText(app->output_edit, TEXT(""));
            break;
        case ID_FILE_EXIT:
            DestroyWindow(app->window);
            break;
        case ID_EDIT_CUT:
        case ID_EDIT_COPY:
        case ID_EDIT_PASTE:
        case ID_EDIT_SELECT_ALL:
            App_HandleEditCommand(app, command_id);
            break;
        case ID_HELP_ABOUT:
            MessageBox(
                app->window,
                TEXT("CE Brainfuck\n\nDesktop-style Windows CE Brainfuck interpreter with pull-down menus and a three-pane editor layout."),
                APP_TITLE,
                MB_OK | MB_ICONINFORMATION
            );
            break;
    }
}

static int App_RegisterClass(HINSTANCE instance) {
    WNDCLASS window_class;

    memset(&window_class, 0, sizeof(window_class));
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = App_WindowProc;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_APP));
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    window_class.lpszClassName = APP_CLASS_NAME;
    return RegisterClass(&window_class) != 0;
}

static LRESULT CALLBACK App_WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    APP_STATE* app;

    app = &g_app;
    switch (message) {
        case WM_CREATE:
            app->window = window;
            app->icon = (HICON)LoadImage(app->instance, MAKEINTRESOURCE(IDI_APP), IMAGE_ICON, 32, 32, 0);
            app->small_icon = (HICON)LoadImage(app->instance, MAKEINTRESOURCE(IDI_APP), IMAGE_ICON, 16, 16, 0);
            app->accelerator = App_CreateAccelerators();
            app->ui_font = (HFONT)GetStockObject(SYSTEM_FONT);
            app->mono_font = App_CreateMonospaceFont();
            app->menu = LoadMenu(app->instance, MAKEINTRESOURCE(IDR_MAIN_MENU));

            if (app->icon) {
                SendMessage(window, WM_SETICON, ICON_BIG, (LPARAM)app->icon);
            }
            if (app->small_icon) {
                SendMessage(window, WM_SETICON, ICON_SMALL, (LPARAM)app->small_icon);
            }

            app->command_bar = CommandBar_Create(app->instance, app->window, 1);
            if (app->command_bar) {
                CommandBar_InsertMenubar(app->command_bar, app->instance, IDR_MAIN_MENU, 0);
                CommandBar_DrawMenuBar(app->command_bar, 0);
                app->menu = CommandBar_GetMenu(app->command_bar, 0);
            }

            app->code_label = CreateWindow(TEXT("STATIC"), TEXT("Code:"), WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, (HMENU)IDC_CODE_LABEL, app->instance, NULL);
            app->code_edit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, 0, 0, window, (HMENU)IDC_CODE_EDIT, app->instance, NULL);
            app->input_label = CreateWindow(TEXT("STATIC"), TEXT("Standard input:"), WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, (HMENU)IDC_INPUT_LABEL, app->instance, NULL);
            app->input_edit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, 0, 0, window, (HMENU)IDC_INPUT_EDIT, app->instance, NULL);
            app->output_label = CreateWindow(TEXT("STATIC"), TEXT("Standard output:"), WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, (HMENU)IDC_OUTPUT_LABEL, app->instance, NULL);
            app->output_edit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 0, 0, 0, 0, window, (HMENU)IDC_OUTPUT_EDIT, app->instance, NULL);

            App_ApplyFonts(app);
            SetWindowText(app->code_edit, TEXT(",[>,]<[.<]"));
            SetWindowText(app->input_edit, TEXT("This is the default stdin-text"));
            App_Layout(app);
            SetFocus(app->code_edit);
            return 0;

        case WM_SIZE:
            App_Layout(app);
            return 0;

        case WM_COMMAND:
            App_HandleCommand(app, LOWORD(w_param));
            return 0;

        case WM_INITMENUPOPUP:
            App_UpdateMenuState(app);
            return 0;

        case WM_CTLCOLORSTATIC:
            SetBkMode((HDC)w_param, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);

        case WM_APP_RUN_COMPLETE:
            App_HandleRunResult(app, (RUN_RESULT*)l_param);
            return 0;

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            InterlockedExchange(&app->cancel_requested, 1);
            app->window = NULL;
            if (app->icon) {
                DestroyIcon(app->icon);
                app->icon = NULL;
            }
            if (app->small_icon) {
                DestroyIcon(app->small_icon);
                app->small_icon = NULL;
            }
            if (app->accelerator) {
                DestroyAcceleratorTable(app->accelerator);
                app->accelerator = NULL;
            }
            if (app->mono_font && app->mono_font != GetStockObject(SYSTEM_FIXED_FONT)) {
                DeleteObject(app->mono_font);
                app->mono_font = NULL;
            }
            if (app->command_bar) {
                CommandBar_Destroy(app->command_bar);
                app->command_bar = NULL;
            }
            if (app->menu) {
                DestroyMenu(app->menu);
                app->menu = NULL;
            }
            App_DestroyThreadHandle(app);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(window, message, w_param, l_param);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, LPWSTR command_line, int show_command) {
    INITCOMMONCONTROLSEX controls;
    HWND window;
    MSG message;

    (void)previous_instance;
    (void)command_line;

    memset(&g_app, 0, sizeof(g_app));
    g_app.instance = instance;

    memset(&controls, 0, sizeof(controls));
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&controls);

    if (!App_RegisterClass(instance)) {
        MessageBox(NULL, TEXT("Window class registration failed."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }

    window = CreateWindow(
        APP_CLASS_NAME,
        APP_TITLE,
        WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        620,
        350,
        NULL,
        NULL,
        instance,
        NULL
    );

    if (!window) {
        MessageBox(NULL, TEXT("Main window creation failed."), APP_TITLE, MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(window, show_command);
    UpdateWindow(window);

    while (GetMessage(&message, NULL, 0, 0)) {
        if (!g_app.accelerator || !TranslateAccelerator(window, g_app.accelerator, &message)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }

    return (int)message.wParam;
}
