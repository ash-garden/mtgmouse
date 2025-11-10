// CaptureExcludeSelf.cpp
// Build: Visual Studio 2022 (x86/x64) - Win32 C++ project
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <cstdio>
#include <chrono>
#include <thread>
#include <winrt/base.h> // for init_apartment (optional)

using SetWindowDisplayAffinity_t = BOOL(WINAPI*)(HWND, DWORD);

// WDA_EXCLUDEFROMCAPTURE がヘッダに無い環境向けの定義（ランタイム呼び出しで使います）
constexpr DWORD WDA_EXCLUDEFROMCAPTURE_FALLBACK = 0x00000011; // 実行時に使う値（OSによって意味が変わる場合あり）
constexpr DWORD WDA_NONE = 0;

// 前方宣言
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void SaveBitmapToFile(HBITMAP hBitmap, HDC hDC, const wchar_t* filename);

// Globals
HWND g_hwnd = nullptr;
const wchar_t g_szClassName[] = L"CaptureExcludeSelfClass";

// ボタンID
constexpr int ID_BUTTON_CAPTURE = 1001;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    winrt::init_apartment(); // optional, harmless

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = g_szClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    // シンプルなウィンドウ（ボタンを置く）
    g_hwnd = CreateWindowEx(
        0, g_szClassName, L"Capture Exclude Self Demo",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME, // 固定サイズでも可
        CW_USEDEFAULT, CW_USEDEFAULT, 360, 140,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(g_hwnd, nCmdShow);

    // メッセージループ
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

void SetThisWindowExcludedFromCapture(HWND hwnd, bool exclude)
{
    // ランタイムで SetWindowDisplayAffinity を取得して呼ぶ（存在しない環境でも安全）
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    auto fn = reinterpret_cast<SetWindowDisplayAffinity_t>(
        GetProcAddress(user32, "SetWindowDisplayAffinity"));
    if (!fn) {
        // API が存在しない環境
        MessageBox(hwnd, L"SetWindowDisplayAffinity API not found on this OS.", L"Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    DWORD mode = exclude ? WDA_EXCLUDEFROMCAPTURE_FALLBACK : WDA_NONE;
    BOOL ok = fn(hwnd, mode);
    if (!ok) {
        // 呼び出しに失敗した場合はメッセージを出すが続行
        MessageBox(hwnd, L"SetWindowDisplayAffinity failed (API returned FALSE). The OS may not support exclusion.", L"Warning", MB_OK | MB_ICONWARNING);
    }
}

// ボタン処理： 自ウィンドウを除外してキャプチャ→ファイル保存
void DoExcludeAndCapture(HWND hwnd)
{
    // 1) 自ウィンドウをキャプチャ除外に設定
    SetThisWindowExcludedFromCapture(hwnd, true);

    // 2) 少し待つ：DWM 等が反映するまで短時間待機（実機で調整）
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    // 3) 仮想スクリーン全体をキャプチャ（マルチモニタ対応）
    int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hScreen = GetDC(nullptr);
    HDC hMemDC = CreateCompatibleDC(hScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, w, h);
    if (!hBmp) {
        MessageBox(hwnd, L"Failed to create bitmap for capture.", L"Error", MB_OK | MB_ICONERROR);
        DeleteDC(hMemDC);
        ReleaseDC(nullptr, hScreen);
        SetThisWindowExcludedFromCapture(hwnd, false);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(hMemDC, hBmp);

    // CAPTUREBLT を使うと layered ウィンドウ等も含めて取得できる場合があります
    BOOL b = BitBlt(hMemDC, 0, 0, w, h, hScreen, x, y, SRCCOPY | CAPTUREBLT);
    if (!b) {
        MessageBox(hwnd, L"BitBlt failed.", L"Error", MB_OK | MB_ICONERROR);
    }

    // 4) BMP ファイルに保存
    const wchar_t* filename = L"screenshot_exclude_self.bmp";
    SaveBitmapToFile(hBmp, hMemDC, filename);

    // 5) 後始末
    SelectObject(hMemDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreen);

    // 6) 除外フラグを解除（必要なら）
    SetThisWindowExcludedFromCapture(hwnd, false);

    // 完了通知
    std::wstring msg = L"Saved: ";
    msg += filename;
    MessageBox(hwnd, msg.c_str(), L"Done", MB_OK);
}

// シンプルな BMP 書き出し（24/32bit 自動対応）
void SaveBitmapToFile(HBITMAP hBitmap, HDC hDC, const wchar_t* filename)
{
    BITMAP bm;
    if (!GetObject(hBitmap, sizeof(bm), &bm)) {
        MessageBox(nullptr, L"GetObject failed", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = bm.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32; // 出力は 32bit BGRA に揃える
    bi.biCompression = BI_RGB;
    bi.biSizeImage = ((bm.bmWidth * 32 + 31) / 32) * 4 * bm.bmHeight;

    std::vector<BYTE> pixels(bi.biSizeImage);
    BITMAPINFO biInfo{};
    biInfo.bmiHeader = bi;

    // GetDIBits で 32bit ピクセルを取得
    HDC hdc = hDC;
    if (!GetDIBits(hdc, hBitmap, 0, bm.bmHeight, pixels.data(), &biInfo, DIB_RGB_COLORS)) {
        MessageBox(nullptr, L"GetDIBits failed", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    // BMP ファイルヘッダ作成
    BITMAPFILEHEADER bfh{};
    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + static_cast<DWORD>(pixels.size());

    // ファイルに書き出し
    FILE* fp = nullptr;
    _wfopen_s(&fp, filename, L"wb");
    if (!fp) {
        MessageBox(nullptr, L"Failed to open output file for writing.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    fwrite(&bfh, sizeof(bfh), 1, fp);
    fwrite(&bi, sizeof(bi), 1, fp);
    fwrite(pixels.data(), 1, pixels.size(), fp);
    fclose(fp);
}

// ウィンドウプロシージャ： ボタンを作って押されたら DoExcludeAndCapture を呼ぶ
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // ボタン設置
        CreateWindowEx(0, L"BUTTON", L"Capture (exclude self)",
                       WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                       20, 20, 300, 40,
                       hwnd, (HMENU)ID_BUTTON_CAPTURE,
                       (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), nullptr);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BUTTON_CAPTURE) {
            // ボタン押下時、別スレッドで実行して UI ブロックを避ける
            std::thread([hwnd]() {
                DoExcludeAndCapture(hwnd);
            }).detach();
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}
