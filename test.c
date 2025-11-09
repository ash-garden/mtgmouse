#include <windows.h>
#include <magnification.h>
#include <tchar.h>
#include <stdio.h>

#pragma comment(lib, "Magnification.lib")

HWND hwndHost = NULL;
HWND hwndMag = NULL;
HINSTANCE hInst = NULL;

#define WIDTH  800
#define HEIGHT 600

// BMP保存ヘルパー
BOOL SaveHBITMAPToFile(HBITMAP hBitmap, LPCWSTR filename)
{
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPFILEHEADER bmfHeader = {0};
    BITMAPINFOHEADER bi = {0};

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = -bmp.bmHeight;  // 上下反転防止
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;

    DWORD dwBmpSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;

    HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
    BYTE* lpbitmap = (BYTE*)GlobalLock(hDIB);

    GetDIBits(GetDC(NULL), hBitmap, 0, (UINT)bmp.bmHeight, lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfHeader.bfSize = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfHeader.bfType = 0x4D42; // BM

    DWORD dwBytesWritten;
    WriteFile(hFile, (LPSTR)&bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
    WriteFile(hFile, (LPSTR)&bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
    WriteFile(hFile, (LPSTR)lpbitmap, dwBmpSize, &dwBytesWritten, NULL);

    GlobalUnlock(hDIB);
    GlobalFree(hDIB);
    CloseHandle(hFile);

    return TRUE;
}

// Magnifier初期化
BOOL InitMagnifier(HWND hwndParent)
{
    if (!MagInitialize())
        return FALSE;

    hwndMag = CreateWindow(WC_MAGNIFIER, TEXT("MagnifierWindow"),
                           WS_CHILD | WS_VISIBLE,
                           0, 0, WIDTH, HEIGHT,
                           hwndParent, NULL, hInst, NULL);

    if (!hwndMag)
        return FALSE;

    // 自ウィンドウを除外
    HWND hwndSelf = hwndParent;
    HWND excludeList[] = { hwndSelf };
    MagSetWindowFilterList(hwndMag, MW_FILTERMODE_EXCLUDE, 1, excludeList);

    // 拡大率1.0設定
    MAGTRANSFORM matrix;
    MagIdentityMatrix(&matrix);
    matrix.v[0][0] = 1.0f;
    matrix.v[1][1] = 1.0f;
    matrix.v[2][2] = 1.0f;
    MagSetWindowTransform(hwndMag, &matrix);

    // ソース範囲を画面全体に
    RECT rcSource = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    MagSetWindowSource(hwndMag, rcSource);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        if (!InitMagnifier(hwnd))
            MessageBox(hwnd, L"Magnifier 初期化失敗", L"Error", MB_OK);
        SetTimer(hwnd, 1, 2000, NULL); // 2秒後にキャプチャ
        break;

    case WM_TIMER:
        if (hwndMag)
        {
            HBITMAP hBitmap = NULL;
            if (MagGetWindowBitmap(hwndMag, &hBitmap))
            {
                SaveHBITMAPToFile(hBitmap, L"capture.bmp");
                DeleteObject(hBitmap);
                MessageBox(hwnd, L"capture.bmp に保存しました", L"完了", MB_OK);
                KillTimer(hwnd, 1);
            }
        }
        break;

    case WM_DESTROY:
        MagUninitialize();
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    hInst = hInstance;

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MagDemo";
    RegisterClass(&wc);

    hwndHost = CreateWindow(L"MagDemo", L"Magnifier Capture",
                            WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, 0, WIDTH, HEIGHT,
                            NULL, NULL, hInstance, NULL);

    ShowWindow(hwndHost, nCmdShow);
    UpdateWindow(hwndHost);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
