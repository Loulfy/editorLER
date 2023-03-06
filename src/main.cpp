#include <iostream>
#include "ler.hpp"

int main()
{
    std::cout << ler::getHomeDir() << std::endl;
    ler::LerApp app;
    app.show([](){
        ImGui::Begin("Hello Gui");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    });
    app.run();

    return EXIT_SUCCESS;
}
