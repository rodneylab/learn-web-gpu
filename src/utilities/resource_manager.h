#ifndef SRC_RESOURCE_MANAGER_H
#define SRC_RESOURCE_MANAGER_H

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <vector>

class ResourceManager
{
public:
    static bool load_geometry(const std::filesystem::path &path,
                              std::vector<float> &point_data,
                              std::vector<uint16_t> &index_data);

    static wgpu::ShaderModule load_shader_module(
        const std::filesystem::path &path,
        wgpu::Device device);
};

bool ResourceManager::load_geometry(const std::filesystem::path &path,
                                    std::vector<float> &point_data,
                                    std::vector<uint16_t> &index_data)
{
    std::ifstream file{path};
    if (!file.is_open())
    {
        spdlog::error("Was not able to open file `{}`", path.string());
        return false;
    }

    point_data.clear();
    index_data.clear();

    enum class Section
    {
        None,
        Points,
        Indices
    };
    Section current_section{Section::None};

    float value{};
    uint16_t index{};
    std::string line;
    while (!file.eof())
    {
        getline(file, line);
        spdlog::info("Got line: {}", line);

        // overcome the `CRLF` problem
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line == "[points]")
        {
            current_section = Section::Points;
        }
        else if (line == "[indices]")
        {
            current_section = Section::Indices;
        }
        else if (line[0] == '#' || line.empty())
        {
            // nothing to do
        }
        else if (current_section == Section::Points)
        {
            std::istringstream iss{line};
            for (int i{0}; i < 5; ++i)
            {
                iss >> value;
                point_data.push_back(value);
            }
        }
        else if (current_section == Section::Indices)
        {
            std::istringstream iss(line);
            for (int i{0}; i < 3; ++i)
            {
                iss >> index;
                index_data.push_back(index);
            }
        }
    }

    return true;
}

wgpu::ShaderModule ResourceManager::load_shader_module(
    const std::filesystem::path &path,
    wgpu::Device device)
{
    spdlog::info("Loading shader module from `{}`", path.string());
    std::ifstream file{path};
    if (!file.is_open())
    {
        spdlog::error("Was not able to open file `{}`", path.string());
        return nullptr;
    }
    std::string shader_source{(std::istreambuf_iterator<char>(file)),
                              (std::istreambuf_iterator<char>())};
    spdlog::trace("Source: \n{}", shader_source);

    wgpu::ShaderModuleWGSLDescriptor shader_code_descriptor{};
    shader_code_descriptor.chain.next = nullptr;
    shader_code_descriptor.chain.sType =
        wgpu::SType::ShaderModuleWGSLDescriptor;
    shader_code_descriptor.code = shader_source.c_str();

    wgpu::ShaderModuleDescriptor shader_descriptor{};
#ifdef WEBGPU_BACKEND_WGPU
    shader_descriptor.hintCount = 0;
    shader_descriptor.hints = nullptr;
#endif
    shader_descriptor.nextInChain = &shader_code_descriptor.chain;

    return device.createShaderModule(shader_descriptor);
}

#endif
