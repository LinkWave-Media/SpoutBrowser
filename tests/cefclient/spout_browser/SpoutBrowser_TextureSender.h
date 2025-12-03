#pragma once

#include <string>

struct ID3D11Device;
class spoutDX;

namespace client::spout {

    class TextureSender {

    public:
        TextureSender() {}
        virtual ~TextureSender() { Cleanup(); }

        bool Initialize(ID3D11Device* pDevice);
        void Cleanup();

        bool SendTexture();

    protected:
        spoutDX* _sender = nullptr;
    };

}
