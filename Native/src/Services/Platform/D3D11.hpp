#pragma once

#include <string>
#include <vector>

struct ID3D11DepthStencilView;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11DeviceChild;
struct ID3D11RenderTargetView;
struct ID3D11Resource;
struct ID3D11View;
struct IDXGIAdapter;

enum D3D11_DSV_DIMENSION;
enum D3D11_RTV_DIMENSION;
enum DXGI_FORMAT;

namespace D3D11 {

ID3D11Device* GetDevice();
ID3D11DeviceContext* GetImmediateContext();
IDXGIAdapter* GetAdapter();

std::string GetDebugName(ID3D11DeviceChild*);
std::string GetDebugName(D3D11_DSV_DIMENSION);
std::string GetDebugName(D3D11_RTV_DIMENSION);
std::string GetDebugName(DXGI_FORMAT);

ID3D11Resource* GetResource(ID3D11View*);

void Initialize(ID3D11Resource* TexturePtr);
}
