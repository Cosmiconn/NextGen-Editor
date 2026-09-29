#include "KfmPanel.hpp"
#include "mapeditor/app/Localization.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>

using namespace theseed::mapeditor;
// Opt-in real GPU smoke/benchmark for the actual KFM panel. No client asset is
// bundled in this test; pass an existing KFM and an output directory explicitly.
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    const std::filesystem::path source=argv[1],out=argv[2];
    app::KfmPanel panel;if(!panel.Open(source))return 1;
    std::vector<double> loads,encodes;
    for(int i=0;i<31;++i) {
        const auto begin=std::chrono::steady_clock::now();auto f=core::LoadKfmFile(source);
        const auto middle=std::chrono::steady_clock::now();if(!f)return 1;auto encoded=core::EncodeKfm(*f);
        const auto end=std::chrono::steady_clock::now();if(!encoded)return 1;
        if(i){loads.push_back(std::chrono::duration<double,std::milli>(middle-begin).count());encodes.push_back(std::chrono::duration<double,std::milli>(end-middle).count());}
    }
    std::sort(loads.begin(),loads.end());std::sort(encodes.begin(),encodes.end());
    std::cout<<"warm_load_median_ms="<<(loads[14]+loads[15])/2<<", warm_encode_median_ms="<<(encodes[14]+encodes[15])/2<<'\n';
    if(!glfwInit())return 1;
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    auto* window=glfwCreateWindow(1280,900,"KFM runtime verification",nullptr,nullptr);
    if(!window){glfwTerminate();return 1;}glfwMakeContextCurrent(window);glfwSwapInterval(0);
    if(!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))){glfwDestroyWindow(window);glfwTerminate();return 1;}
    std::cout<<"GL_RENDERER="<<glGetString(GL_RENDERER)<<"\nGL_VERSION="<<glGetString(GL_VERSION)<<'\n';
    ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330 core");
    std::filesystem::create_directories(out);int failures=0;
    const auto draw=[&] {
        glfwPollEvents();ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
        ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1280,900});
        ImGui::Begin("KFM",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
        panel.Draw({});ImGui::End();ImGui::Render();
        glViewport(0,0,1280,900);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    };
    for(int language=0;language<2;++language) {
        app::SetLanguage(language?app::Language::English:app::Language::German);
        for(int i=0;i<3;++i)draw();glFinish();
        std::vector<unsigned char> pixels(1280*900*3);glReadPixels(0,0,1280,900,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
        std::size_t bright=0;for(std::size_t i=0;i<pixels.size();i+=3)if(pixels[i]>150 && pixels[i+1]>150 && pixels[i+2]>150)++bright;
        const auto error=glGetError();if(error!=GL_NO_ERROR || bright<1000 || ImGui::GetDrawData()->TotalVtxCount<1000)++failures;
        std::cout<<(language?"EN":"DE")<<": bright_pixels="<<bright<<", vertices="<<ImGui::GetDrawData()->TotalVtxCount<<", GL_error="<<error<<'\n';
        std::ofstream image(out/(language?"kfm-en.ppm":"kfm-de.ppm"),std::ios::binary);image<<"P6\n1280 900\n255\n";
        for(int y=899;y>=0;--y)image.write(reinterpret_cast<const char*>(pixels.data()+y*1280*3),1280*3);
        const auto begin=std::chrono::steady_clock::now();for(int i=0;i<60;++i)draw();glFinish();
        std::cout<<(language?"EN":"DE")<<": warm_panel_frame_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/60<<'\n';
    }
    ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return failures?1:0;
}
