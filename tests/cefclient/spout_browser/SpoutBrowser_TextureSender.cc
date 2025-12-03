#include "SpoutBrowser_TextureSender.h"
#include "SPOUTSDK/SpoutDirectX/SpoutDX/SpoutDX.h"


namespace client::spout {

    bool TextureSender::Initialize(ID3D11Device* pDevice) {
        if (!_sender) {
            _sender = new spoutDX();
            return _sender->OpenDirectX11(pDevice);
        }
        return false;
    }
    

    void TextureSender::Cleanup() {
        if (_sender) {
            _sender->ReleaseSender();
            _sender->CloseDirectX11();
            delete _sender;
            _sender = nullptr;
        }
    }

    bool TextureSender::SendTexture() {
        if (_sender) {
            return _sender->SendBackBuffer();
        }
        return false;
    }

}
