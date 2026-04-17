#include "gtest/gtest.h"
#include "Texture.h"

class MockShaderResourceView : public ID3D11ShaderResourceView {
	ULONG refCount = 1;

public:
	ULONG STDMETHODCALLTYPE AddRef() override { return ++refCount;  }
	ULONG STDMETHODCALLTYPE Release() override {
		return --refCount;
	}

    // These can be left unimplemented for this test
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    void STDMETHODCALLTYPE GetDevice(ID3D11Device** ppDevice) override {}
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT*, void*) override { return E_FAIL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void*) override { return E_FAIL; }
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown*) override { return E_FAIL; }
    void STDMETHODCALLTYPE GetResource(ID3D11Resource** ppResource) override {}
    void STDMETHODCALLTYPE GetDesc(D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc) override {}

};

// Test ReleaseTexture
TEST(TextureTests, ReleaseTextureSetsPointerToNull)
{
    auto* mockSRV = new MockShaderResourceView();
    ID3D11ShaderResourceView* texture = mockSRV;

    ReleaseTexture(texture);

    EXPECT_EQ(texture, nullptr);  // check if it set to null
}