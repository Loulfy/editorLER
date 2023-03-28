#include <iostream>
#include "ler.hpp"
#include "imfilebrowser.hpp"

int main()
{
    ler::log::set_level(ler::log::level::level_enum::debug);
    ler::GlslangInitializer initme;
    ler::shaderAutoCompile();

    ler::LerApp app;
    auto dev = app.getDevice();

    ler::BatchedMesh batch;
    batch.allocate(dev);
    batch.appendMeshFromFile(dev, ler::ASSETS_DIR / "Bolt.fbx");
    batch.appendMeshFromFile(dev, ler::ASSETS_DIR / "Lantern.glb");
    batch.appendMeshFromFile(dev, ler::ASSETS_DIR / "Duck.glb");

    ler::MeshViewer viewer;
    viewer.init(dev);
    viewer.switchMesh(batch, 0);

    ImGui::FileBrowser fileDialog;
    fileDialog.SetTitle("Open Model");
    fileDialog.SetTypeFilters({".off", ".obj", ".fbx", ".glb", ".gltf"});

    app.show([&](){
        viewer.display(dev, batch);

        ImGui::Begin("Mesh Importer");
        if (ImGui::Button("Load Mesh"))
            fileDialog.Open();
        ImGui::End();
        fileDialog.Display();
        if (fileDialog.HasSelected())
        {
            batch.appendMeshFromFile(dev, fileDialog.GetSelected());
            fileDialog.ClearSelected();
        }
    });

    app.run();

    return EXIT_SUCCESS;
}
