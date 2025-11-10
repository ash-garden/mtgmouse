#include "pch.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <iostream>
#include <vector>
#include <string>
#include <wrl.h>

using namespace winrt;
using namespace Windows::Graphics::Capture;
using namespace Windows::Graphics::DirectX;
using namespace Windows::Graphics::DirectX::Direct3D11;
using namespace Microsoft::WRL;

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowscodecs.lib")

// --- ヘルパー：Direct3Dデバイスを作成 ---
ID3D11Device* CreateD3DDevice()
{
    ComPtr<ID3D11Device> device;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        0,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,
        ARRAYSIZE(levels),
        D3D11_SDK_VERSION,
        &device,
        nullptr,
        nullptr);
    return device.Detach();
}

// --- ヘルパー：Direct3D11 → WinRT オブジェクト変換 ---
IDirect3DDevice CreateDirect3DDevice(ID3D11Device* const device)
{
    ComPtr<IDXGIDevice> dxgiDevice;
    device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    com_ptr<IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put()));
    return inspectable.as<IDirect3DDevice>();
}

// --- WICを使って保存 ---
void SaveTextureToFile(ID3D11DeviceContext* ctx, ID3D11Texture2D* texture, const std::wstring& filename)
{
    ComPtr<IDXGISurface> surface;
    texture->QueryInterface(IID_PPV_ARGS(&surface));

    ComPtr<IWICImagingFactory> wicFactory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));

    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICStream> stream;
    wicFactory->CreateStream(&stream);
    stream->InitializeFromFilename(filename.c_str(), GENERIC_WRITE);
    wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);

    ComPtr<IWICBitmapFrameEncode> frame;
    encoder->CreateNewFrame(&frame, nullptr);
    frame->Initialize(nullptr);

    DXGI_SURFACE_DESC desc;
    DXGI_SURFACE_DESC surfaceDesc;
    D3D11_TEXTURE2D_DESC texDesc;
    texture->GetDesc(&texDesc);

    frame->SetSize(texDesc.Width, texDesc.Height);
    frame->SetPixelFormat(&GUID_WICPixelFormat32bppBGRA);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    ctx->Map(texture, 0, D3D11_MAP_READ, 0, &mapped);
    frame->WritePixels(texDesc.Height, mapped.RowPitch, texDesc.Height * mapped.RowPitch, reinterpret_cast<BYTE*>(mapped.pData));
    ctx->Unmap(texture, 0);

    frame->Commit();
    encoder->Commit();
}

// --- ディスプレイ単位でキャプチャして保存 ---
void CaptureAllDisplays()
{
    winrt::init_apartment();

    ComPtr<ID3D11Device> device{ CreateD3DDevice() };
    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);

    auto d3dDevice = CreateDirect3DDevice(device.Get());

    // 全ディスプレイ列挙
    ComPtr<IDXGIFactory1> factory;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));

    UINT index = 0;
    ComPtr<IDXGIAdapter1> adapter;
    while (factory->EnumAdapters1(index++, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        UINT monitorIndex = 0;
        ComPtr<IDXGIOutput> output;
        while (adapter->EnumOutputs(monitorIndex++, &output) != DXGI_ERROR_NOT_FOUND)
        {
            DXGI_OUTPUT_DESC desc;
            output->GetDesc(&desc);

            auto item = GraphicsCaptureItem::CreateFromVisual(nullptr);
            // 通常はモニタハンドルからCaptureItemを作成（省略可）

            // Windows::Graphics::Capture APIを利用してFramePool作成
            auto framePool = Direct3D11CaptureFramePool::Create(
                d3dDevice,
                DirectXPixelFormat::B8G8R8A8UIntNormalized,
                1,
                item.Size());

            auto session = framePool.CreateCaptureSession(item);
            session.StartCapture();

            auto frame = framePool.TryGetNextFrame();
            if (frame)
            {
                auto surface = frame.Surface();
                ComPtr<ID3D11Texture2D> tex;
                winrt::check_hresult(GetDXGIInterfaceFromObject(surface, IID_PPV_ARGS(&tex)));

                std::wstring filename = L"Captured_Display_" + std::to_wstring(index) + L".png";
                SaveTextureToFile(context.Get(), tex.Get(), filename);
                std::wcout << L"Saved: " << filename << std::endl;
            }
        }
    }
}

int main()
{
    try
    {
        CaptureAllDisplays();
        std::wcout << L"All displays captured successfully." << std::endl;
    }
    catch (std::exception const& e)
    {
        std::cerr << e.what() << std::endl;
    }
}
