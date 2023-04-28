//
// Created by loulfy on 29/03/2023.
//

#ifndef LER_RES_H
#define LER_RES_H

#include "ler_sys.hpp"
#include "ler_dev.hpp"

namespace ler
{
    // TODO: finish cache loader
    //template <typename Rc>
    class CacheLoader
    {
    public:

        //using Ptr = std::shared_ptr<Texture>;
        AsyncRes<TexturePtr> load(const std::string& path, LerDevicePtr& device)
        {
            if(saved.contains(path))
                co_return saved.at(path);
            co_await ReadFileAwaitable();
            auto t = device->createTexture(vk::Format::eR8G8B8A8Unorm, vk::Extent2D(200,200), vk::SampleCountFlagBits::e1);
            co_return t;
        }

    private:

        std::map<std::string, TexturePtr> saved;
    };
}

#endif //LER_RES_H
