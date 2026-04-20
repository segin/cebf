// Removed: #define UNICODE
// Removed: #define _UNICODE
#include <windows.h>
#include <string.h> // For strcpy_s

// --- Control IDs ---
#define IDC_LABEL_INPUT1 101
#define IDC_EDIT_INPUT1  102
#define IDC_LABEL_INPUT2 103
#define IDC_EDIT_INPUT2  104
#define IDC_LABEL_OUTPUT 105
#define IDC_EDIT_OUTPUT  106

// --- Global Variables ---
HWND hEditInput1, hEditInput2, hEditOutput;
HFONT hMonoFont = NULL; // Handle for the monospaced font

// --- Window Procedure ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // --- Create Monospaced Font ---
            LOGFONT lf = {0};
            // Common monospaced fonts: "Consolas", "Courier New", "Lucida Console"
            // Using Courier New for broad compatibility.
            strcpy_s(lf.lfFaceName, LF_FACESIZE, "Courier New"); // Use strcpy_s for char[]
            lf.lfHeight = -MulDiv(10, GetDeviceCaps(GetDC(hwnd), LOGPIXELSY), 72); // 10 points
            lf.lfWeight = FW_NORMAL;
            lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN; // Crucial for monospaced
            hMonoFont = CreateFontIndirect(&lf); // Resolves to CreateFontIndirectA

            if (hMonoFont == NULL) {
                 // Use ANSI strings for MessageBox
                 MessageBox(hwnd, "Failed to create monospaced font.", "Error", MB_OK | MB_ICONERROR);
                 // Handle error - perhaps fall back to system font or exit
            }

            HINSTANCE hInstance = GetModuleHandle(NULL); // Get instance handle

            // --- Create Controls ---
            // Label 1
            // Use ANSI strings for CreateWindowEx class and text
            HWND hLabel1 = CreateWindowEx(0, "STATIC", "Input 1:",
                           WS_CHILD | WS_VISIBLE | SS_LEFT,
                           10, 10, 80, 20, // x, y, width, height
                           hwnd, (HMENU)IDC_LABEL_INPUT1, hInstance, NULL);

            // Edit Box 1
            hEditInput1 = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
                           WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                           100, 10, 250, 20,
                           hwnd, (HMENU)IDC_EDIT_INPUT1, hInstance, NULL);

            // Label 2
            HWND hLabel2 = CreateWindowEx(0, "STATIC", "Input 2:",
                           WS_CHILD | WS_VISIBLE | SS_LEFT,
                           10, 40, 80, 20,
                           hwnd, (HMENU)IDC_LABEL_INPUT2, hInstance, NULL);

            // Edit Box 2
            hEditInput2 = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
                           WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                           100, 40, 250, 20,
                           hwnd, (HMENU)IDC_EDIT_INPUT2, hInstance, NULL);

            // Label Output
            HWND hLabelOut = CreateWindowEx(0, "STATIC", "Output:",
                             WS_CHILD | WS_VISIBLE | SS_LEFT,
                             10, 70, 80, 20,
                             hwnd, (HMENU)IDC_LABEL_OUTPUT, hInstance, NULL);

            // Edit Box Output (Read-only, Multiline)
            hEditOutput = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
                           WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE |
                           ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
                           10, 95, 340, 150, // Made wider and taller
                           hwnd, (HMENU)IDC_EDIT_OUTPUT, hInstance, NULL);

             // --- Apply Font to Controls ---
             // SendMessage does not have A/W variants, it handles ANSI/Unicode based on the message
            if (hMonoFont) {
                SendMessage(hLabel1, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
                SendMessage(hEditInput1, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
                SendMessage(hLabel2, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
                SendMessage(hEditInput2, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
                SendMessage(hLabelOut, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
                SendMessage(hEditOutput, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
            }

            // Example: Put some initial text in the output box
            // Use ANSI string for SetWindowText
            SetWindowText(hEditOutput, "Ready.\r\nWaiting for input...\r\n");

            // You could add a button here and handle WM_COMMAND to process input
            // and update the output box.

            return 0; // Handled WM_CREATE
        }

        // Example: Add text to output (you would call this from event handlers)
        // case WM_COMMAND: {
        //     if (LOWORD(wParam) == ID_OF_SOME_BUTTON) { // If a button was clicked
        //         char buffer1[256]; // Use char instead of wchar_t
        //         char buffer2[256];
        //         GetWindowText(hEditInput1, buffer1, 256); // Resolves to GetWindowTextA
        //         GetWindowText(hEditInput2, buffer2, 256);
        //
        //         // Append to output box
        //         int len = GetWindowTextLength(hEditOutput); // Resolves to GetWindowTextLengthA
        //         SendMessage(hEditOutput, EM_SETSEL, (WPARAM)len, (LPARAM)len); // Move cursor to end
        //         // Use ANSI strings for EM_REPLACESEL
        //         SendMessage(hEditOutput, EM_REPLACESEL, FALSE, (LPARAM)"Input 1: ");
        //         SendMessage(hEditOutput, EM_REPLACESEL, FALSE, (LPARAM)buffer1);
        //         SendMessage(hEditOutput, EM_REPLACESEL, FALSE, (LPARAM)"\r\nInput 2: ");
        //         SendMessage(hEditOutput, EM_REPLACESEL, FALSE, (LPARAM)buffer2);
        //         SendMessage(hEditOutput, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
        //     }
        //     return 0;
        // }

        case WM_DESTROY:
            if (hMonoFont) {
                DeleteObject(hMonoFont); // Clean up the font object
            }
            PostQuitMessage(0);
            return 0;

        case WM_CLOSE:
             DestroyWindow(hwnd); // No A/W variant needed
             return 0;

        default:
            // Let Windows handle messages we don't process
            return DefWindowProc(hwnd, uMsg, wParam, lParam); // Resolves to DefWindowProcA
    }
    return 0; // Should technically not be reached due to DefWindowProc
}

// --- Entry Point ---
// Use WinMain instead of wWinMain, LPSTR instead of PWSTR
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    // --- Register the window class ---
    // Use ANSI string for class name
    const char CLASS_NAME[] = "MonospaceInputOutputWindowClass";

    WNDCLASS wc = {0}; // Use WNDCLASS
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME; // Assign char* directly
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Default window color
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    // wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Optional: Add an icon

    // RegisterClass resolves to RegisterClassA
    if (!RegisterClass(&wc)) {
        // Use ANSI strings for MessageBox
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // --- Create the window ---
    // CreateWindowEx resolves to CreateWindowExA
    // Use ANSI strings for window class and title
    HWND hwnd = CreateWindowEx(
        0,                         // Optional window styles.
        CLASS_NAME,                // Window class (ANSI)
        "Monospace Input/Output Example", // Window title (ANSI)
        WS_OVERLAPPEDWINDOW,       // Window style (standard)

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, 380, 300, // x, y, width, height

        NULL,       // Parent window
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL) {
        // Use ANSI strings for MessageBox
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow); // No A/W variant needed
    UpdateWindow(hwnd);         // No A/W variant needed

    // --- Run the message loop ---
    MSG msg = {0};
    // GetMessage resolves to GetMessageA
    while (GetMessage(&msg, NULL, 0, 0) > 0) { // Use > 0 to handle WM_QUIT correctly
        TranslateMessage(&msg); // No A/W variant needed
        DispatchMessage(&msg);  // Resolves to DispatchMessageA
    }

    return (int)msg.wParam; // Return the exit code from PostQuitMessage
}