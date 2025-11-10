// GpuCaptureSample.cpp
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

using namespace Microsoft::WRL;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

ComPtr<ID3D11Device>           g_d3dDevice;
ComPtr<ID3D11DeviceContext>    g_d3dContext;
ComPtr<IDXGISwapChain1>        g_swapChain;
ComPtr<ID3D11Texture2D>        g_captureTexture;
GraphicsCaptureItem            g_captureItem{ nullptr };
Direct3D11CaptureFramePool     g_framePool{ nullptr };
GraphicsCaptureSession         g_session{ nullptr };
std::atomic<bool>              g_frameArrived{ false };

// ヘルパー：DXGIデバイス → WinRT IDirect3DDevice
IDirect3DDevice CreateWinRtDeviceFromD3D11(ComPtr<ID3D11Device> device)
{
    ComPtr<IDXGIDevice> dxgiDevice;
    device.As(&dxgiDevice);
    winrt::com_ptr<::IInspectable> inspectable;
    winrt::check_hresult(
        CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put()));
    return inspectable.as<IDirect3DDevice>();
}

// フレーム到着ハンドラ
void OnFrameArrived(Direct3D11CaptureFramePool const& pool, IInspectable const&)
{
    auto frame = pool.TryGetNextFrame();
    if (!frame) return;

    // 取得されたフレームの表面（テクスチャ）を取得
    auto surface = frame.Surface();
    auto d3dDevice = CreateWinRtDeviceFromD3D11(g_d3dDevice);
    auto nativeDevice = reinterpret_cast<::ID3D11Device*>(winrt::get_abi(d3dDevice));

    // WinRT テクスチャから D3D11Texture2D を取得（省略: get IDXGISurface etc）
    ComPtr<ID3D11Texture2D> sourceTex;
    winrt::get_unknown(surface).as(&sourceTex);

    // 必要なら：コピー or 利用
    // For simplicity assume sourceTex is usable directly as g_captureTexture
    g_captureTexture = sourceTex;

    g_frameArrived.store(true, std::memory_order_release);
}

// 初期化
void InitGraphicsCapture(HWND hwndTarget, RECT captureRect)
{
    // 1) D3D11デバイス／コンテキスト作成
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL createdLevel;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                      D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                      featureLevels, _countof(featureLevels),
                      D3D11_SDK_VERSION,
                      &g_d3dDevice, &createdLevel, &g_d3dContext);

    // 2) キャプチャ対象取得（ウィンドウ or モニタ）
    auto interopFactory = winrt::get_activation_factory<GraphicsCaptureItem,
        IGraphicsCaptureItemInterop>();
    winrt::com_ptr<IGraphicsCaptureItemInterop> itemInterop{ interopFactory };
    winrt::com_ptr<GraphicsCaptureItem> item;
    // 例：ウィンドウキャプチャ
    itemInterop->CreateForWindow(hwndTarget,
        winrt::guid_of<GraphicsCaptureItem>(),
        reinterpret_cast<void**>(winrt::put_abi(item)));
    g_captureItem = *item;

    // 3) FramePool 作成
    auto winRtDevice = CreateWinRtDeviceFromD3D11(g_d3dDevice);
    auto size = winrt::Windows::Graphics::SizeInt32{ captureRect.right - captureRect.left,
                                                     captureRect.bottom - captureRect.top };
    g_framePool = Direct3D11CaptureFramePool::Create(
        winRtDevice,
        winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2, size);

    g_framePool.FrameArrived({ &OnFrameArrived });

    // 4) セッション作成と開始
    g_session = g_framePool.CreateCaptureSession(g_captureItem);
    g_session.IsCursorCaptureEnabled(true);
    g_session.StartCapture();
}

// 描画ループ（簡略版）
void RenderLoop()
{
    while (true) {
        if (!g_frameArrived.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        g_frameArrived.store(false, std::memory_order_release);

        // g_captureTexture に最新フレームが入っている
        // ここで GPU上で使用可能 → 拡大表示／円形マスク描画など実装
        // 略：レンダーターゲットに描画、SwapChain.Present() など

    }
}

// WinMain 等で呼び出し
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow)
{
    // …ウィンドウ作成コード略…

    // キャプチャ対象ウィンドウのハンドル取得
    HWND hwndTarget = /* 対象ウィンドウの HWND を取得 */;
    RECT rect;
    GetClientRect(hwndTarget, &rect);

    InitGraphicsCapture(hwndTarget, rect);

    std::thread renderThread(RenderLoop);
    renderThread.detach();

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
