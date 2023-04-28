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

    // TODO: finish async cache
    bool toto;
    ler::CacheLoader cache;
    auto co = cache.load("test.png", dev);
    toto = co.loaded();
    ler::BatchedMesh batch;
    batch.allocate(dev);
    batch.appendMeshFromFile(dev, "Bolt.fbx");
    batch.appendMeshFromFile(dev, "Lantern.glb");
    batch.appendMeshFromFile(dev, "Duck.glb"); // ler::ASSETS_DIR /

    ler::MeshViewer viewer;
    viewer.init(dev);
    viewer.switchMesh(batch, 0);

    ImGui::FileBrowser fileDialog;
    fileDialog.SetTitle("Open Model");
    fileDialog.SetTypeFilters({".off", ".obj", ".fbx", ".glb", ".gltf"});

    bool p_open = true;
    app.show([&](){

        /*ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", &p_open, window_flags);
        // Submit the DockSpace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleVar(2);
        ImGui::End();*/

        viewer.display(dev, batch);

        /*ImGui::Begin("Mesh Importer");
        if (ImGui::Button("Load Mesh"))
            fileDialog.Open();
        ImGui::End();
        fileDialog.Display();
        if (fileDialog.HasSelected())
        {
            batch.appendMeshFromFile(dev, fileDialog.GetSelected());
            fileDialog.ClearSelected();
        }*/
    });

    app.run();

    return EXIT_SUCCESS;
}
