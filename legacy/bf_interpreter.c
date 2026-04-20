#include <windows.h>

#define IDC_CODE_LABEL 1001
#define IDC_CODE_EDIT 1002
#define IDC_INPUT_LABEL 1003
#define IDC_INPUT_EDIT 1004
#define IDC_OUTPUT_LABEL 1005
#define IDC_OUTPUT_EDIT 1006

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_CREATE: {
            // Set monospaced font (Courier New)
            HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                    FIXED_PITCH | FF_MODERN, TEXT("Courier New"));

            // Code Label
            HWND hwndCodeLabel = CreateWindow(
                TEXT("STATIC"), TEXT("Code"),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 10, 50, 20,
                hwnd, (HMENU)IDC_CODE_LABEL, NULL, NULL);
            SendMessage(hwndCodeLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Code Input (Edit control)
            HWND hwndCodeEdit = CreateWindow(
                TEXT("EDIT"),
                TEXT("], >[<->"),
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                10, 40, 400, 100,
                hwnd, (HMENU)IDC_CODE_EDIT, NULL, NULL);
            SendMessage(hwndCodeEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Input Label
            HWND hwndInputLabel = CreateWindow(
                TEXT("STATIC"), TEXT("Standard input"),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 150, 100, 20,
                hwnd, (HMENU)IDC_INPUT_LABEL, NULL, NULL);
            SendMessage(hwndInputLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Input Edit control
            HWND hwndInputEdit = CreateWindow(
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                10, 180, 400, 100,
                hwnd, (HMENU)IDC_INPUT_EDIT, NULL, NULL);
            SendMessage(hwndInputEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Output Label
            HWND hwndOutputLabel = CreateWindow(
                TEXT("STATIC"), TEXT("This is the output"),
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                10, 290, 150, 20,
                hwnd, (HMENU)IDC_OUTPUT_LABEL, NULL, NULL);
            SendMessage(hwndOutputLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Output Edit control (read-only)
            HWND hwndOutputEdit = CreateWindow(
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
                10, 320, 400, 100,
                hwnd, (HMENU)IDC_OUTPUT_EDIT, NULL, NULL);
            SendMessage(hwndOutputEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"BF Interpreter Window Class";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        CLASS_NAME,
        L"BF Interpreter",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 500,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}