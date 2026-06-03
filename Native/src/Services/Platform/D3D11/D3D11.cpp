#include <Services.hpp>
#include <Services/Platform/D3D11/_D3D11.hpp>
#include <Services/Platform/D3D11.hpp>
#include <Util.hpp>

#include <d3d11on12.h>
#include <wrl/client.h>

#include "Interop.h"
using Microsoft::WRL::ComPtr;

typedef void (*UnityRenderingEventAndData)(int, void*);

D3D11::Internal::State D3D11::Internal::_d3d11State;

ID3D11Device* D3D11::GetDevice()
{
    using namespace D3D11::Internal;
    return _d3d11State.Device;
}

ID3D11DeviceContext* D3D11::GetImmediateContext()
{
    using namespace D3D11::Internal;
    return _d3d11State.DeviceContext;
}

IDXGIAdapter* D3D11::GetAdapter()
{
    using namespace D3D11::Internal;
    return _d3d11State.Adapter;
}

ID3D11Resource* D3D11::GetResource(ID3D11View* view)
{
    ID3D11Resource* res;
    view->GetResource(&res);
    return res;
}

void D3D11::Initialize(ID3D11Resource* TexturePtr)
{
    using namespace D3D11::Internal;
    
    ID3D11Device* dev = nullptr;
    TexturePtr->GetDevice(&dev);
    assert(dev);
    
    ID3D11DeviceContext* ctx = nullptr;
    dev->GetImmediateContext(&ctx);
    assert(ctx);
    
    IDXGIDevice* dxgiDev = nullptr;
    HRESULT res = dev->QueryInterface(IID_PPV_ARGS(&dxgiDev));
    assert(SUCCEEDED(res));
    assert(dxgiDev);
    
    IDXGIAdapter* dxgiAdp = nullptr;
    dxgiDev->GetAdapter(&dxgiAdp);
    assert(dxgiAdp);
    
    _d3d11State.Device = dev;
    _d3d11State.DeviceContext = ctx;
    _d3d11State.Adapter = dxgiAdp;
}
