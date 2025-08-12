#pragma once
#include<d3d11.h>


ID3D11ShaderResourceView* LoadTextureFromFile(ID3D11Device* device, const wchar_t* filename);
void ReleaseTexture(ID3D11ShaderResourceView* texture);