#include "ShaderLab/Core/PackageManager.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include "ShaderLab/Core/ShaderLabData.h" // For nlohmann or similar if needed? No, binary format.
#include <vector>

namespace ShaderLab {

// Magic Footer: SHADERLAB_PACK
// Followed by uint64 table_offset
// Followed by magic again? 
// Let's use a simple format:
// [ ... EXE DATA ... ]
// [ PACK DATA ... ]
// [ DIRECTORY JSON STRING SIZE (uint32) ]
// [ DIRECTORY JSON STRING ]
// [ "SHADERLAB_PACK" (14 chars) ]

static const char MAGIC[] = "SHADERLAB_PACK";
static const size_t MAGIC_LEN = 14;

PackageManager& PackageManager::Get() {
    static PackageManager instance;
    return instance;
}

bool PackageManager::Initialize() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    m_exePath = exePath;

    std::ifstream file(m_exePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    auto fileSize = file.tellg();
    if (fileSize < (std::streampos)MAGIC_LEN) return false;

    // Check Magic
    file.seekg(-((int)MAGIC_LEN), std::ios::end);
    char buf[MAGIC_LEN + 1];
    file.read(buf, MAGIC_LEN);
    buf[MAGIC_LEN] = 0;

    if (std::string(buf) != MAGIC) {
        m_isPacked = false;
        return false;
    }

    // Read Footer Info
    // Before magic, we have Directory Offset (uint64)
    uint64_t footerSize = MAGIC_LEN + sizeof(uint64_t);
    if ((uint64_t)fileSize < footerSize) return false;

    file.seekg(-((int)footerSize), std::ios::end);
    uint64_t dirOffset = 0;
    file.read(reinterpret_cast<char*>(&dirOffset), sizeof(uint64_t));

    // Validate offset
    if (dirOffset >= (uint64_t)fileSize) return false;

    m_isPacked = true;
    m_packRequestOffset = 0; // Not strictly needed if absolute offsets used.
    // Actually, let's assume offsets in directory are relative to dirOffset? 
    // No, file format: 
    // [Packed File 1] [Packed File 2] ... [Directory Table] [Footer Info] [Magic]
    // So dirOffset points to Directory Table.
    // The offsets in Directory Table will be absolute file offsets or relative to 0? usually absolute is easiest.

    // Read Directory
    file.seekg(dirOffset, std::ios::beg);
    
    // Format: Count(uint32), then entries
    uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));

    for(uint32_t i=0; i<count; ++i) {
        PackedEntry entry;
        uint32_t pathLen = 0;
        file.read(reinterpret_cast<char*>(&pathLen), sizeof(uint32_t));
        if (pathLen > 1024) break; // Sanity
        std::vector<char> pathBuf(pathLen + 1);
        file.read(pathBuf.data(), pathLen);
        pathBuf[pathLen] = 0;
        entry.path = pathBuf.data();
        
        file.read(reinterpret_cast<char*>(&entry.offset), sizeof(uint64_t));
        file.read(reinterpret_cast<char*>(&entry.size), sizeof(uint64_t));
        
        m_directory[entry.path] = entry;
    }

    return true;
}

bool PackageManager::HasFile(const std::string& path) const {
    // Normalize path separators if needed?
    // Start simple.
    std::string safePath = path; 
    std::replace(safePath.begin(), safePath.end(), '\\', '/');
    return m_directory.find(safePath) != m_directory.end();
}

std::vector<uint8_t> PackageManager::GetFile(const std::string& path) {
    if (!m_isPacked) return {};
    
    std::string safePath = path; 
    std::replace(safePath.begin(), safePath.end(), '\\', '/');

    auto it = m_directory.find(safePath);
    if (it == m_directory.end()) return {};

    std::vector<uint8_t> data(it->second.size);
    std::ifstream file(m_exePath, std::ios::binary);
    file.seekg(it->second.offset, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.data()), it->second.size);
    
    return data;
}

}
