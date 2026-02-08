#include "ShaderLab/Core/Serializer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace ShaderLab {

    // Helper conversions - Moved to ShaderLab namespace for ADL
    NLOHMANN_JSON_SERIALIZE_ENUM(TextureType, {
        {TextureType::Texture2D, "Texture2D"},
        {TextureType::TextureCube, "TextureCube"},
        {TextureType::Texture3D, "Texture3D"}
    })

    NLOHMANN_JSON_SERIALIZE_ENUM(BindingType, {
        {BindingType::Scene, "Scene"},
        {BindingType::File, "File"}
    })
    
    NLOHMANN_JSON_SERIALIZE_ENUM(TransitionType, {
        {TransitionType::None, "None"},
        {TransitionType::Crossfade, "Crossfade"},
        {TransitionType::DipToBlack, "DipToBlack"},
        {TransitionType::FadeOut, "FadeOut"},
        {TransitionType::FadeIn, "FadeIn"},
        {TransitionType::Glitch, "Glitch"},
        {TransitionType::Pixelate, "Pixelate"}
    })

    void to_json(json& j, const TextureBinding& b);
    void from_json(const json& j, TextureBinding& b);
    void to_json(json& j, const Scene::PostFXEffect& e);
    void from_json(const json& j, Scene::PostFXEffect& e);
    void to_json(json& j, const Scene& s);
    void from_json(const json& j, Scene& s);
    void to_json(json& j, const AudioClip& a);
    void from_json(const json& j, AudioClip& a);
    void to_json(json& j, const TrackerRow& r);
    void from_json(const json& j, TrackerRow& r);
    void to_json(json& j, const DemoTrack& t);
    void from_json(const json& j, DemoTrack& t);
    void to_json(json& j, const ProjectData& p);
    void from_json(const json& j, ProjectData& p);


    void to_json(json& j, const TextureBinding& b) {
        j = json{
            {"channel", b.channelIndex},
            {"enabled", b.enabled},
            {"bindType", b.bindingType},
            {"sourceIndex", b.sourceSceneIndex},
            {"path", b.filePath},
            {"type", b.type}
        };
    }

    void from_json(const json& j, TextureBinding& b) {
        j.at("channel").get_to(b.channelIndex);
        j.at("enabled").get_to(b.enabled);
        j.at("bindType").get_to(b.bindingType);
        j.at("sourceIndex").get_to(b.sourceSceneIndex);
        j.at("path").get_to(b.filePath);
        j.at("type").get_to(b.type);
    }

    void to_json(json& j, const Scene::PostFXEffect& e) {
        j = json{
            {"name", e.name},
            {"code", e.shaderCode},
            {"enabled", e.enabled}
        };
        if (!e.precompiledPath.empty()) j["precompiled"] = e.precompiledPath;
    }

    void from_json(const json& j, Scene::PostFXEffect& e) {
        j.at("name").get_to(e.name);
        j.at("code").get_to(e.shaderCode);
        if (j.contains("enabled")) j.at("enabled").get_to(e.enabled);
        if (j.contains("precompiled")) j.at("precompiled").get_to(e.precompiledPath);
        e.isDirty = true;
        e.pipelineState = nullptr;
        e.historyIndex = 0;
        e.historyInitialized = false;
        e.historyTextures.clear();
    }

    void to_json(json& j, const Scene& s) {
        j = json{
            {"name", s.name},
            {"code", s.shaderCode},
            {"bindings", s.bindings},
            {"outputType", s.outputType}
        };
        if (!s.postFxChain.empty()) j["postfx"] = s.postFxChain;
        if (!s.precompiledPath.empty()) j["precompiled"] = s.precompiledPath;
    }

    void from_json(const json& j, Scene& s) {
        j.at("name").get_to(s.name);
        j.at("code").get_to(s.shaderCode);
        j.at("bindings").get_to(s.bindings);
        if(j.contains("outputType")) j.at("outputType").get_to(s.outputType);
        if(j.contains("postfx")) j.at("postfx").get_to(s.postFxChain);
        if(j.contains("precompiled")) j.at("precompiled").get_to(s.precompiledPath);
    }

    void to_json(json& j, const AudioClip& a) {
        j = json{
            {"name", a.name},
            {"path", a.path},
            {"bpm", a.bpm},
            {"type", (int)a.type}
        };
    }

    void from_json(const json& j, AudioClip& a) {
        j.at("name").get_to(a.name);
        j.at("path").get_to(a.path);
        j.at("bpm").get_to(a.bpm);
        int t; j.at("type").get_to(t); a.type = (AudioType)t;
    }

    void to_json(json& j, const TrackerRow& r) {
        j = json{
            {"id", r.rowId},
            {"scene", r.sceneIndex},
            {"trans", r.transition},
            {"dur", r.transitionDuration},
            {"offset", r.timeOffset},
            {"music", r.musicIndex},
            {"oneshot", r.oneShotIndex},
            {"stop", r.stop}
        };
    }

    void from_json(const json& j, TrackerRow& r) {
        j.at("id").get_to(r.rowId);
        j.at("scene").get_to(r.sceneIndex);
        j.at("trans").get_to(r.transition);
        j.at("dur").get_to(r.transitionDuration);
        if(j.contains("offset")) j.at("offset").get_to(r.timeOffset);
        j.at("music").get_to(r.musicIndex);
        j.at("oneshot").get_to(r.oneShotIndex);
        if(j.contains("stop")) j.at("stop").get_to(r.stop);
    }

    void to_json(json& j, const DemoTrack& t) {
        j = json{
            {"name", t.name},
            {"bpm", t.bpm},
            {"len", t.lengthBeats},
            {"rows", t.rows}
        };
    }

    void from_json(const json& j, DemoTrack& t) {
        j.at("name").get_to(t.name);
        j.at("bpm").get_to(t.bpm);
        j.at("len").get_to(t.lengthBeats);
        j.at("rows").get_to(t.rows);
    }

    void to_json(json& j, const ProjectData& p) {
        j = json{
            {"scenes", p.scenes},
            {"audio", p.audioLibrary},
            {"track", p.track},
            {"bpm", p.transport.bpm}
        };
    }
    
    void from_json(const json& j, ProjectData& p) {
        j.at("scenes").get_to(p.scenes);
        j.at("audio").get_to(p.audioLibrary);
        j.at("track").get_to(p.track);
        if(j.contains("bpm")) j.at("bpm").get_to(p.transport.bpm);
    }

namespace Serializer {

    bool SaveProject(const ProjectData& project, const std::string& filepath) {
        json j = project;
        std::ofstream o(filepath);
        if (!o.is_open()) return false;
        o << j.dump(4);
        return true;
    }

    bool LoadProjectFromJson(const std::string& jsonContent, ProjectData& outProject) {
        try {
            json j = json::parse(jsonContent);
            outProject = j.get<ProjectData>();
            return true;
        } catch (...) {
            return false;
        }
    }

    bool LoadProject(const std::string& filepath, ProjectData& outProject) {
        std::ifstream i(filepath);
        if (!i.is_open()) return false;
        json j;
        i >> j;
        outProject = j.get<ProjectData>();
        return true;
    }

    bool ExportProject(const ProjectData& inputProject, const std::string& outputFile) {
        ProjectData p = inputProject; // Copy to modify paths
        
        fs::path outPath(outputFile);
        fs::path baseDir = outPath.parent_path();
        fs::path assetsDir = baseDir / "assets";

        if (!fs::exists(assetsDir)) {
            fs::create_directories(assetsDir);
        }

        // 1. Copy Audio
        for (auto& clip : p.audioLibrary) {
            if (fs::exists(clip.path)) {
                fs::path originalPath(clip.path);
                fs::path fileName = originalPath.filename();
                fs::path newPath = assetsDir / fileName;
                
                // Copy
                if (fs::exists(newPath)) fs::remove(newPath); // Overwrite
                fs::copy_file(clip.path, newPath);

                // Rebase
                clip.path = "assets/" + fileName.string();
            }
        }

        // 2. Copy Texture Bindings & Precompiled Shaders
        for (auto& scene : p.scenes) {
            // Textures
            for (auto& bind : scene.bindings) {
                if (bind.bindingType == BindingType::File && !bind.filePath.empty()) {
                    if (fs::exists(bind.filePath)) {
                         fs::path originalPath(bind.filePath);
                         fs::path fileName = originalPath.filename();
                         fs::path newPath = assetsDir / fileName;
                         
                         if(fs::exists(newPath)) fs::remove(newPath);
                         fs::copy_file(bind.filePath, newPath);
                         
                         bind.filePath = "assets/" + fileName.string();
                    }
                }
            }
            // Precompiled Shader
            if (!scene.precompiledPath.empty()) {
                if (fs::exists(scene.precompiledPath)) {
                    fs::path originalPath(scene.precompiledPath);
                    fs::path fileName = originalPath.filename();
                    fs::path newPath = assetsDir / fileName;
                    
                    if(fs::exists(newPath)) fs::remove(newPath);
                    fs::copy_file(scene.precompiledPath, newPath);
                    
                    scene.precompiledPath = "assets/" + fileName.string();
                }
            }

            // Precompiled Post FX
            for (auto& fx : scene.postFxChain) {
                if (!fx.precompiledPath.empty() && fs::exists(fx.precompiledPath)) {
                    fs::path originalPath(fx.precompiledPath);
                    fs::path fileName = originalPath.filename();
                    fs::path newPath = assetsDir / fileName;

                    if (fs::exists(newPath)) fs::remove(newPath);
                    fs::copy_file(fx.precompiledPath, newPath);

                    fx.precompiledPath = "assets/" + fileName.string();
                }
            }
        }

        return SaveProject(p, outputFile);
    }

    bool ConsolidateProject(ProjectData& project, const std::string& rootPath) {
        fs::path baseDir(rootPath);
        fs::path assetsDir = baseDir / "assets";

        if (!fs::exists(assetsDir)) {
            // Create assets folder if it doesn't exist
            // Also suggest creating subfolders for organization?
            fs::create_directories(assetsDir);
            fs::create_directories(assetsDir / "audio");
            fs::create_directories(assetsDir / "textures");
            fs::create_directories(assetsDir / "shaders"); // For precompiled
        } else {
             if(!fs::exists(assetsDir / "audio")) fs::create_directories(assetsDir / "audio");
             if(!fs::exists(assetsDir / "textures")) fs::create_directories(assetsDir / "textures");
             if(!fs::exists(assetsDir / "shaders")) fs::create_directories(assetsDir / "shaders");
        }

        auto normalizePath = [](const std::string& p) -> std::string {
             std::string s = p;
             std::replace(s.begin(), s.end(), '\\', '/');
             return s;
        };

        auto isInside = [&](const fs::path& p, const fs::path& root) {
            std::string pStr = normalizePath(fs::absolute(p).string());
            std::string rStr = normalizePath(fs::absolute(root).string());
            return pStr.find(rStr) == 0;
        };

        // 1. Audio
        for (auto& clip : project.audioLibrary) {
            fs::path p(clip.path);
            if (!p.is_absolute() && !fs::exists(p)) {
                // Try resolving relative to CWD if not found? 
                // Or assume assumes CWD is current run location
            }

            // Check if absolute and inside?
            // If absolute and NOT inside root, copy.
            // If relative, assume it's already inside (or we can't find it easily unless we know where it came from, 
            // but we assume current session has valid paths).
            
            // Logic: If path exists at absolute location and is NOT inside root -> Copy.
            if (fs::exists(p) && p.is_absolute()) {
                if (!isInside(p, baseDir)) {
                    fs::path fileName = p.filename();
                    fs::path newPath = assetsDir / "audio" / fileName;
                    
                    // Avoid self-overwrite if somehow resolved same
                    if (fs::absolute(p) != fs::absolute(newPath)) {
                        fs::copy_file(p, newPath, fs::copy_options::overwrite_existing);
                    }
                    clip.path = "assets/audio/" + fileName.string();
                } else {
                     // Inside root, ensure relative path stored
                     // fs::relative available in C++17
                     clip.path = fs::relative(p, baseDir).string();
                }
            }
             clip.path = normalizePath(clip.path);
        }

        // 2. Textures & Shaders
        for (auto& scene : project.scenes) {
            for (auto& bind : scene.bindings) {
                if (bind.bindingType == BindingType::File && !bind.filePath.empty()) {
                    fs::path p(bind.filePath);
                    if (fs::exists(p) && p.is_absolute()) {
                         if (!isInside(p, baseDir)) {
                             fs::path fileName = p.filename();
                             fs::path newPath = assetsDir / "textures" / fileName;
                             if (fs::absolute(p) != fs::absolute(newPath)) {
                                 fs::copy_file(p, newPath, fs::copy_options::overwrite_existing);
                             }
                             bind.filePath = "assets/textures/" + fileName.string();
                         } else {
                             bind.filePath = fs::relative(p, baseDir).string();
                         }
                    }
                    bind.filePath = normalizePath(bind.filePath);
                }
            }

            if (!scene.precompiledPath.empty()) {
                fs::path p(scene.precompiledPath);
                if (fs::exists(p) && p.is_absolute()) {
                     if (!isInside(p, baseDir)) {
                         fs::path fileName = p.filename();
                         fs::path newPath = assetsDir / "shaders" / fileName;
                          if (fs::absolute(p) != fs::absolute(newPath)) {
                                 fs::copy_file(p, newPath, fs::copy_options::overwrite_existing);
                          }
                         scene.precompiledPath = "assets/shaders/" + fileName.string();
                     } else {
                         scene.precompiledPath = fs::relative(p, baseDir).string();
                     }
                }
                scene.precompiledPath = normalizePath(scene.precompiledPath);
            }

            for (auto& fx : scene.postFxChain) {
                if (!fx.precompiledPath.empty()) {
                    fs::path p(fx.precompiledPath);
                    if (fs::exists(p) && p.is_absolute()) {
                        if (!isInside(p, baseDir)) {
                            fs::path fileName = p.filename();
                            fs::path newPath = assetsDir / "shaders" / fileName;
                            if (fs::absolute(p) != fs::absolute(newPath)) {
                                fs::copy_file(p, newPath, fs::copy_options::overwrite_existing);
                            }
                            fx.precompiledPath = "assets/shaders/" + fileName.string();
                        } else {
                            fx.precompiledPath = fs::relative(p, baseDir).string();
                        }
                    }
                    fx.precompiledPath = normalizePath(fx.precompiledPath);
                }
            }
        }
        
        return true;
    }
    
    struct PackedEntryInfo {
        std::string path;
        uint64_t offset;
        uint64_t size;
    };

    bool PackExecutable(const std::string& sourceExe, const std::string& outputExe, const std::string& projectJsonPath, const std::vector<PackedExtraFile>& extraFiles) {
        // 1. Read Source EXE
        std::ifstream src(sourceExe, std::ios::binary);
        if (!src.is_open()) return false;
        std::vector<uint8_t> exeData((std::istreambuf_iterator<char>(src)), std::istreambuf_iterator<char>());
        src.close();

        // 2. Load Project to find assets
        ProjectData project;
        if (!LoadProject(projectJsonPath, project)) return false;

        // 3. Prepare Pack Data
        std::vector<uint8_t> packBlob;
        std::vector<PackedEntryInfo> entries;
        
        // Add project.json
        {
            std::ifstream f(projectJsonPath, std::ios::binary);
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            PackedEntryInfo info;
            info.path = "project.json";
            info.offset = packBlob.size(); // Relative to start of PACK section
            info.size = data.size();
            entries.push_back(info);
            packBlob.insert(packBlob.end(), data.begin(), data.end());
        }

        // Add Assets (Audio)
        for(const auto& clip : project.audioLibrary) {
            fs::path p = fs::path(projectJsonPath).parent_path() / clip.path;
            if (fs::exists(p)) {
                 std::ifstream f(p, std::ios::binary);
                 std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                 PackedEntryInfo info;
                 info.path = clip.path; // Store relative path as used in JSON
                 std::replace(info.path.begin(), info.path.end(), '\\', '/');
                 info.offset = packBlob.size();
                 info.size = data.size();
                 entries.push_back(info);
                 packBlob.insert(packBlob.end(), data.begin(), data.end());
            }
        }
        
        // Add Assets (Texture Bindings & Precompiled Shaders)
        for(const auto& scene : project.scenes) {
            // Textures
            for(const auto& bind : scene.bindings) {
                if(bind.bindingType == BindingType::File && !bind.filePath.empty()) {
                     fs::path p = fs::path(projectJsonPath).parent_path() / bind.filePath;
                     // Simple duplicate check:
                     bool found = false;
                     std::string normPath = bind.filePath;
                     std::replace(normPath.begin(), normPath.end(), '\\', '/');
                     for(const auto& e : entries) if(e.path == normPath) found = true;
                     
                     if (!found && fs::exists(p)) {
                         std::ifstream f(p, std::ios::binary);
                         std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                         PackedEntryInfo info;
                         info.path = normPath;
                         info.offset = packBlob.size();
                         info.size = data.size();
                         entries.push_back(info);
                         packBlob.insert(packBlob.end(), data.begin(), data.end());
                     }
                }
            }
            // Precompiled Shader
            if (!scene.precompiledPath.empty()) {
                fs::path p = fs::path(projectJsonPath).parent_path() / scene.precompiledPath;
                bool found = false;
                std::string normPath = scene.precompiledPath;
                std::replace(normPath.begin(), normPath.end(), '\\', '/');
                for(const auto& e : entries) if(e.path == normPath) found = true;

                if (!found && fs::exists(p)) {
                    std::ifstream f(p, std::ios::binary);
                    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                    PackedEntryInfo info;
                    info.path = normPath;
                    info.offset = packBlob.size();
                    info.size = data.size();
                    entries.push_back(info);
                    packBlob.insert(packBlob.end(), data.begin(), data.end());
                }
            }

            for (const auto& fx : scene.postFxChain) {
                if (!fx.precompiledPath.empty()) {
                    fs::path p = fs::path(projectJsonPath).parent_path() / fx.precompiledPath;
                    bool found = false;
                    std::string normPath = fx.precompiledPath;
                    std::replace(normPath.begin(), normPath.end(), '\\', '/');
                    for (const auto& e : entries) if (e.path == normPath) found = true;

                    if (!found && fs::exists(p)) {
                        std::ifstream f(p, std::ios::binary);
                        std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                        PackedEntryInfo info;
                        info.path = normPath;
                        info.offset = packBlob.size();
                        info.size = data.size();
                        entries.push_back(info);
                        packBlob.insert(packBlob.end(), data.begin(), data.end());
                    }
                }
            }
        }

        for (const auto& extra : extraFiles) {
            fs::path p = extra.sourcePath;
            if (!fs::exists(p)) continue;

            bool found = false;
            std::string normPath = extra.packedPath;
            std::replace(normPath.begin(), normPath.end(), '\\', '/');
            for (const auto& e : entries) if (e.path == normPath) found = true;

            if (!found) {
                std::ifstream f(p, std::ios::binary);
                std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                PackedEntryInfo info;
                info.path = normPath;
                info.offset = packBlob.size();
                info.size = data.size();
                entries.push_back(info);
                packBlob.insert(packBlob.end(), data.begin(), data.end());
            }
        }

        // 4. Create Directory Blob
        // Format: Count(uint32) + [Len(u32)+Path+Offset(u64)+Size(u64)]...
        std::vector<uint8_t> dirBlob;
        uint32_t count = (uint32_t)entries.size();
        auto append = [&](const void* d, size_t s) { 
            const uint8_t* p = (const uint8_t*)d;
            dirBlob.insert(dirBlob.end(), p, p+s); 
        };
        append(&count, sizeof(uint32_t));
        
        for(const auto& e : entries) {
            uint32_t len = (uint32_t)e.path.length();
            append(&len, sizeof(uint32_t));
            append(e.path.data(), len);
            // Offset stored in EXE will be: exeSize + blobOffset + packedOffset
            // But wait, the Reader reads 'dirOffset' to find the directory table.
            // The directory table entries contain offsets into the file.
            // So we need absolute file offsets here!
            
            // packBlob stores the file content.
            // packBlob starts at 'exeData.size()'.
            // e.offset is relative to packBlob start.
            // So absolute offset = exeData.size() + e.offset.
            uint64_t absOffset = exeData.size() + e.offset;
            append(&absOffset, sizeof(uint64_t));
            append(&e.size, sizeof(uint64_t));
        }

        // 5. Write Output EXE
        // Layout: [EXE][PACK_DATA][DIRECTORY_BLOB][DIR_OFFSET_U64][MAGIC]
        std::ofstream out(outputExe, std::ios::binary);
        if (!out.is_open()) return false;
        
        out.write((const char*)exeData.data(), exeData.size());
        out.write((const char*)packBlob.data(), packBlob.size());
        
        // Directory location
        uint64_t dirStartOffset = exeData.size() + packBlob.size(); 
        out.write((const char*)dirBlob.data(), dirBlob.size());
        
        // Footer: DirOffset(u64) + Magic
        out.write((const char*)&dirStartOffset, sizeof(uint64_t));
        const char MAGIC[] = "SHADERLAB_PACK";
        out.write(MAGIC, 14); // 14 bytes
        
        return true;
    }

    bool PackExecutable(const std::string& sourceExe, const std::string& outputExe, const std::string& projectJsonPath) {
        return PackExecutable(sourceExe, outputExe, projectJsonPath, {});
    }


}
}
