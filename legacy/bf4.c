#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IDC_CODE_EDIT    1001
#define IDC_INPUT_EDIT   1002
#define IDC_OUTPUT_EDIT  1003
#define IDM_RUN          2001
#define IDM_COPY         2004

typedef struct {
    unsigned char *data;
    int position;
    int size;
} Tape;

typedef struct {
    Tape tape;
    char *code;
    char *input;
    int input_pos;
    char *output;
    int output_len;
    int output_capacity;
} Interpreter;

void tape_init(Tape *tape, int size) {
    tape->size = size;
    tape->data = (unsigned char *)calloc(size, sizeof(unsigned char));
    tape->position = 0;
}

void tape_free(Tape *tape) {
    free(tape->data);
}

void tape_forward(Tape *tape) {
    if (tape->position >= tape->size - 1)
        tape->position = 0;
    else
        tape->position++;
}

void tape_reverse(Tape *tape) {
    if (tape->position <= 0)
        tape->position = tape->size - 1;
    else
        tape->position--;
}

void tape_inc(Tape *tape) {
    tape->data[tape->position]++;
}

void tape_dec(Tape *tape) {
    tape->data[tape->position]--;
}

unsigned char tape_get(Tape *tape) {
    return tape->data[tape->position];
}

void tape_set(Tape *tape, unsigned char value) {
    tape->data[tape->position] = value;
}

void interpreter_init(Interpreter *interpreter) {
    tape_init(&interpreter->tape, 0x10000);
    interpreter->code = NULL;
    interpreter->input = NULL;
    interpreter->input_pos = 0;
    interpreter->output = NULL;
    interpreter->output_len = 0;
    interpreter->output_capacity = 0;
}

void interpreter_free(Interpreter *interpreter) {
    tape_free(&interpreter->tape);
    free(interpreter->code);
    free(interpreter->input);
    free(interpreter->output);
}

void interpreter_append_output(Interpreter *interpreter, char c) {
    if (interpreter->output_len + 1 >= interpreter->output_capacity) {
        int new_cap = (interpreter->output_capacity == 0) ? 256 : interpreter->output_capacity * 2;
        char *new_output = (char *)realloc(interpreter->output, new_cap);
        if (!new_output) return;
        interpreter->output = new_output;
        interpreter->output_capacity = new_cap;
    }
    interpreter->output[interpreter->output_len++] = c;
    interpreter->output[interpreter->output_len] = '\0';
}

char *optimize_code(const char *code) {
    int len = strlen(code);
    char *optimized = (char *)malloc(len + 1);
    int j = 0;
    for (int i = 0; i < len; i++) {
        switch (code[i]) {
            case '>': case '<': case '+': case '-':
            case ',': case '.': case '[': case ']':
                optimized[j++] = code[i];
                break;
            default: break;
        }
    }
    optimized[j] = '\0';
    return optimized;
}

void interpreter_run(Interpreter *interpreter, const char *code, const char *input) {
    free(interpreter->code);
    free(interpreter->input);
    free(interpreter->output);
    interpreter->code = optimize_code(code);
    interpreter->input = _strdup(input);
    interpreter->input_pos = 0;
    interpreter->output = NULL;
    interpreter->output_len = 0;
    interpreter->output_capacity = 0;

    int code_len = strlen(interpreter->code);
    for (int pc = 0; pc < code_len; pc++) {
        char c = interpreter->code[pc];
        switch (c) {
            case '>': tape_forward(&interpreter->tape); break;
            case '<': tape_reverse(&interpreter->tape); break;
            case '+': tape_inc(&interpreter->tape); break;
            case '-': tape_dec(&interpreter->tape); break;
            case ',': {
                char ch = (interpreter->input_pos < strlen(interpreter->input)) ? interpreter->input[interpreter->input_pos++] : 0;
                tape_set(&interpreter->tape, ch);
                break;
            }
            case '.': {
                char ch = tape_get(&interpreter->tape);
                interpreter_append_output(interpreter, ch);
                break;
            }
            case '[':
                if (tape_get(&interpreter->tape) == 0) {
                    int count = 1;
                    while (count > 0 && ++pc < code_len) {
                        if (interpreter->code[pc] == '[') count++;
                        else if (interpreter->code[pc] == ']') count--;
                    }
                }
                break;
            case ']':
                if (tape_get(&interpreter->tape) != 0) {
                    int count = 1;
                    while (count > 0 && --pc >= 0) {
                        if (interpreter->code[pc] == ']') count++;
                        else if (interpreter->code[pc] == '[') count--;
                    }
                }
                break;
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HFONT hMonoFont = NULL;
    static Interpreter interpreter;

    switch (msg) {
        case WM_CREATE: {
            hMonoFont = CreateFont(0, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
            if (!hMonoFont)
                hMonoFont = CreateFont(0, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");

            int y = 10, labelHeight = 20, editHeight = 100, width = 600, x = 10;

            CreateWindow("STATIC", "Code:", WS_CHILD | WS_VISIBLE, x, y, 50, labelHeight, hwnd, NULL, NULL, NULL);
            HWND hCodeEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                x, y + labelHeight, width, editHeight, hwnd, (HMENU)IDC_CODE_EDIT, NULL, NULL);
            SendMessage(hCodeEdit, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
            y += labelHeight + editHeight + 10;

            CreateWindow("STATIC", "Input:", WS_CHILD | WS_VISIBLE, x, y, 50, labelHeight, hwnd, NULL, NULL, NULL);
            HWND hInputEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                x, y + labelHeight, width, editHeight, hwnd, (HMENU)IDC_INPUT_EDIT, NULL, NULL);
            SendMessage(hInputEdit, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
            y += labelHeight + editHeight + 10;

            CreateWindow("STATIC", "Output:", WS_CHILD | WS_VISIBLE, x, y, 50, labelHeight, hwnd, NULL, NULL, NULL);
            HWND hOutputEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
                x, y + labelHeight, width, editHeight, hwnd, (HMENU)IDC_OUTPUT_EDIT, NULL, NULL);
            SendMessage(hOutputEdit, WM_SETFONT, (WPARAM)hMonoFont, TRUE);

            HMENU hMenu = CreateMenu();
            HMENU hActions = CreatePopupMenu();
            AppendMenu(hActions, MF_STRING, IDM_RUN, "&Run");
            AppendMenu(hActions, MF_STRING, IDM_COPY, "&Copy");
            AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hActions, "&Actions");
            SetMenu(hwnd, hMenu);

            interpreter_init(&interpreter);
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDM_RUN: {
                    HWND hCodeEdit = GetDlgItem(hwnd, IDC_CODE_EDIT);
                    int codeLen = GetWindowTextLength(hCodeEdit) + 1;
                    char *code = (char *)malloc(codeLen);
                    GetWindowText(hCodeEdit, code, codeLen);

                    HWND hInputEdit = GetDlgItem(hwnd, IDC_INPUT_EDIT);
                    int inputLen = GetWindowTextLength(hInputEdit) + 1;
                    char *input = (char *)malloc(inputLen);
                    GetWindowText(hInputEdit, input, inputLen);

                    interpreter_run(&interpreter, code, input);
                    SetWindowText(GetDlgItem(hwnd, IDC_OUTPUT_EDIT), interpreter.output);

                    free(code);
                    free(input);
                    break;
                }
                case IDM_COPY: {
                    HWND hOutput = GetDlgItem(hwnd, IDC_OUTPUT_EDIT);
                    int len = GetWindowTextLength(hOutput) + 1;
                    char *text = (char *)malloc(len);
                    GetWindowText(hOutput, text, len);
                    if (OpenClipboard(hwnd)) {
                        EmptyClipboard();
                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
                        if (hMem) {
                            char *clip = (char *)GlobalLock(hMem);
                            strcpy(clip, text);
                            GlobalUnlock(hMem);
                            SetClipboardData(CF_TEXT, hMem);
                        }
                        CloseClipboard();
                    }
                    free(text);
                    break;
                }
            }
            return 0;
        }

        case WM_DESTROY: {
            if (hMonoFont) DeleteObject(hMonoFont);
            interpreter_free(&interpreter);
            PostQuitMessage(0);
            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0,
                      hInstance, NULL, LoadCursor(NULL, IDC_ARROW), (HBRUSH)(COLOR_WINDOW+1),
                      NULL, "BFInterpreterClass", NULL };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow("BFInterpreterClass", "BF Interpreter", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}