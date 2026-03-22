#include <windows.h>
#include <commctrl.h>
#include <strsafe.h>

#include "..\KrEverythingShared.h"

#pragma comment(lib, "Comctl32.lib")

#define IDC_PROCESS_LIST 1001
#define IDC_REFRESH_BUTTON 1002
#define IDC_STATUS_TEXT 1003

static HINSTANCE g_Instance;
static HWND g_MainWindow;
static HWND g_ListView;
static HWND g_StatusText;
static HWND g_RefreshButton;

static void SetStatusText(const wchar_t *text)
{
    SetWindowTextW(g_StatusText, text);
}

static void FormatUInt64(wchar_t *buffer, size_t bufferCount, ULONGLONG value)
{
    StringCchPrintfW(buffer, bufferCount, L"%llu", value);
}

static void FormatSizeKb(wchar_t *buffer, size_t bufferCount, ULONGLONG value)
{
    StringCchPrintfW(buffer, bufferCount, L"%llu KB", value / 1024ULL);
}

static void FormatCpuTimeMs(wchar_t *buffer, size_t bufferCount, ULONGLONG value)
{
    StringCchPrintfW(buffer, bufferCount, L"%llu ms", value / 10000ULL);
}

static void FormatCreateTime(wchar_t *buffer, size_t bufferCount, ULONGLONG value)
{
    FILETIME fileTime;
    SYSTEMTIME systemTime;

    if (value == 0) {
        buffer[0] = L'\0';
        return;
    }

    fileTime.dwLowDateTime = (DWORD)(value & 0xffffffffULL);
    fileTime.dwHighDateTime = (DWORD)(value >> 32);

    if (!FileTimeToSystemTime(&fileTime, &systemTime)) {
        buffer[0] = L'\0';
        return;
    }

    StringCchPrintfW(
        buffer,
        bufferCount,
        L"%04u-%02u-%02u %02u:%02u:%02u",
        systemTime.wYear,
        systemTime.wMonth,
        systemTime.wDay,
        systemTime.wHour,
        systemTime.wMinute,
        systemTime.wSecond);
}

static void InitListColumns(HWND listView)
{
    static const struct {
        int width;
        const wchar_t *title;
    } columns[] = {
        { 90, L"PID" },
        { 90, L"PPID" },
        { 220, L"Image Name" },
        { 80, L"Threads" },
        { 80, L"Handles" },
        { 70, L"Session" },
        { 70, L"BasePri" },
        { 170, L"Create Time" },
        { 100, L"Kernel Time" },
        { 100, L"User Time" },
        { 110, L"Working Set" },
        { 110, L"Private Mem" }
    };
    LVCOLUMNW column;
    int index;

    ZeroMemory(&column, sizeof(column));
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    for (index = 0; index < (int)(sizeof(columns) / sizeof(columns[0])); index++) {
        column.iSubItem = index;
        column.cx = columns[index].width;
        column.pszText = (LPWSTR)columns[index].title;
        ListView_InsertColumn(listView, index, &column);
    }
}

static HANDLE OpenDriverHandle(void)
{
    return CreateFileW(
        KREVERYTHING_WIN32_DEVICE_PATH,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
}

static BOOL QueryProcessList(BYTE **buffer, DWORD *bufferSize, DWORD *returnedBytes)
{
    HANDLE deviceHandle;
    BYTE *localBuffer;
    DWORD localBufferSize;
    DWORD bytesReturned;
    BOOL success;

    *buffer = NULL;
    *bufferSize = 0;
    *returnedBytes = 0;

    deviceHandle = OpenDriverHandle();
    if (deviceHandle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    localBufferSize = 256 * 1024;
    localBuffer = NULL;

    for (;;) {
        localBuffer = (BYTE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, localBufferSize);
        if (localBuffer == NULL) {
            CloseHandle(deviceHandle);
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }

        success = DeviceIoControl(
            deviceHandle,
            IOCTL_KREVERYTHING_GET_PROCESS_LIST,
            NULL,
            0,
            localBuffer,
            localBufferSize,
            &bytesReturned,
            NULL);
        if (success) {
            break;
        }

        HeapFree(GetProcessHeap(), 0, localBuffer);
        localBuffer = NULL;

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER &&
            GetLastError() != ERROR_MORE_DATA) {
            CloseHandle(deviceHandle);
            return FALSE;
        }

        if (localBufferSize >= 8 * 1024 * 1024) {
            CloseHandle(deviceHandle);
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return FALSE;
        }

        localBufferSize *= 2;
    }

    CloseHandle(deviceHandle);

    *buffer = localBuffer;
    *bufferSize = localBufferSize;
    *returnedBytes = bytesReturned;
    return TRUE;
}

static void PopulateListView(
    const KREVERYTHING_PROCESS_LIST_HEADER *header,
    const KREVERYTHING_PROCESS_ENTRY *entries
)
{
    LVITEMW item;
    wchar_t buffer[128];
    ULONG index;

    ListView_DeleteAllItems(g_ListView);

    for (index = 0; index < header->EntryCount; index++) {
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = (int)index;
        item.pszText = (LPWSTR)entries[index].ImageName;
        ListView_InsertItem(g_ListView, &item);

        FormatUInt64(buffer, ARRAYSIZE(buffer), entries[index].ProcessId);
        ListView_SetItemText(g_ListView, (int)index, 0, buffer);

        FormatUInt64(buffer, ARRAYSIZE(buffer), entries[index].ParentProcessId);
        ListView_SetItemText(g_ListView, (int)index, 1, buffer);

        ListView_SetItemText(g_ListView, (int)index, 2, (LPWSTR)entries[index].ImageName);

        StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%lu", entries[index].ThreadCount);
        ListView_SetItemText(g_ListView, (int)index, 3, buffer);

        StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%lu", entries[index].HandleCount);
        ListView_SetItemText(g_ListView, (int)index, 4, buffer);

        StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%lu", entries[index].SessionId);
        ListView_SetItemText(g_ListView, (int)index, 5, buffer);

        StringCchPrintfW(buffer, ARRAYSIZE(buffer), L"%ld", entries[index].BasePriority);
        ListView_SetItemText(g_ListView, (int)index, 6, buffer);

        FormatCreateTime(buffer, ARRAYSIZE(buffer), entries[index].CreateTime);
        ListView_SetItemText(g_ListView, (int)index, 7, buffer);

        FormatCpuTimeMs(buffer, ARRAYSIZE(buffer), entries[index].KernelTime);
        ListView_SetItemText(g_ListView, (int)index, 8, buffer);

        FormatCpuTimeMs(buffer, ARRAYSIZE(buffer), entries[index].UserTime);
        ListView_SetItemText(g_ListView, (int)index, 9, buffer);

        FormatSizeKb(buffer, ARRAYSIZE(buffer), entries[index].WorkingSetSize);
        ListView_SetItemText(g_ListView, (int)index, 10, buffer);

        FormatSizeKb(buffer, ARRAYSIZE(buffer), entries[index].PrivatePageCount);
        ListView_SetItemText(g_ListView, (int)index, 11, buffer);
    }
}

static void RefreshProcessList(void)
{
    BYTE *buffer;
    DWORD bufferSize;
    DWORD returnedBytes;
    const KREVERYTHING_PROCESS_LIST_HEADER *header;
    const KREVERYTHING_PROCESS_ENTRY *entries;
    wchar_t statusBuffer[128];

    SetStatusText(L"Refreshing...");

    if (!QueryProcessList(&buffer, &bufferSize, &returnedBytes)) {
        StringCchPrintfW(
            statusBuffer,
            ARRAYSIZE(statusBuffer),
            L"Open/query driver failed. Win32 error=%lu",
            GetLastError());
        SetStatusText(statusBuffer);
        return;
    }

    UNREFERENCED_PARAMETER(bufferSize);

    if (returnedBytes < sizeof(KREVERYTHING_PROCESS_LIST_HEADER)) {
        HeapFree(GetProcessHeap(), 0, buffer);
        SetStatusText(L"Driver returned an invalid buffer.");
        return;
    }

    header = (const KREVERYTHING_PROCESS_LIST_HEADER *)buffer;
    entries = (const KREVERYTHING_PROCESS_ENTRY *)(header + 1);

    PopulateListView(header, entries);

    StringCchPrintfW(
        statusBuffer,
        ARRAYSIZE(statusBuffer),
        L"%lu processes",
        header->EntryCount);
    SetStatusText(statusBuffer);

    HeapFree(GetProcessHeap(), 0, buffer);
}

static void ResizeChildren(HWND window)
{
    RECT clientRect;
    int topHeight;

    GetClientRect(window, &clientRect);
    topHeight = 32;

    MoveWindow(g_RefreshButton, 8, 6, 90, 24, TRUE);
    MoveWindow(g_StatusText, 108, 10, clientRect.right - 116, 18, TRUE);
    MoveWindow(
        g_ListView,
        8,
        topHeight,
        clientRect.right - 16,
        clientRect.bottom - topHeight - 8,
        TRUE);
}

static LRESULT CALLBACK MainWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
    {
        INITCOMMONCONTROLSEX initControls;

        ZeroMemory(&initControls, sizeof(initControls));
        initControls.dwSize = sizeof(initControls);
        initControls.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&initControls);

        g_RefreshButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Refresh",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0,
            0,
            0,
            0,
            window,
            (HMENU)IDC_REFRESH_BUTTON,
            g_Instance,
            NULL);

        g_StatusText = CreateWindowExW(
            0,
            L"STATIC",
            L"Ready",
            WS_CHILD | WS_VISIBLE,
            0,
            0,
            0,
            0,
            window,
            (HMENU)IDC_STATUS_TEXT,
            g_Instance,
            NULL);

        g_ListView = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_LISTVIEWW,
            L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0,
            0,
            0,
            0,
            window,
            (HMENU)IDC_PROCESS_LIST,
            g_Instance,
            NULL);

        ListView_SetExtendedListViewStyle(
            g_ListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        InitListColumns(g_ListView);
        ResizeChildren(window);
        RefreshProcessList();
        return 0;
    }

    case WM_SIZE:
        ResizeChildren(window);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_REFRESH_BUTTON) {
            RefreshProcessList();
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE previousInstance, LPWSTR commandLine, int showCommand)
{
    WNDCLASSEXW windowClass;
    HWND window;
    MSG message;

    UNREFERENCED_PARAMETER(previousInstance);
    UNREFERENCED_PARAMETER(commandLine);

    g_Instance = instance;

    ZeroMemory(&windowClass, sizeof(windowClass));
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"KrEverythingUiWindowClass";
    windowClass.lpfnWndProc = MainWindowProc;
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    windowClass.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    windowClass.hIconSm = windowClass.hIcon;

    if (RegisterClassExW(&windowClass) == 0) {
        return 0;
    }

    window = CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"KrEverything Process Viewer",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1240,
        720,
        NULL,
        NULL,
        instance,
        NULL);
    if (window == NULL) {
        return 0;
    }

    g_MainWindow = window;
    ShowWindow(window, showCommand);
    UpdateWindow(window);

    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return (int)message.wParam;
}

