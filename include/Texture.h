#pragma once
#include<d3d11.h>
#include<iostream>


ID3D11ShaderResourceView* LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename , bool forceReload = false);
void ReleaseTexture(ID3D11ShaderResourceView*& textureSRV);