#include "MeshtasticPlugin.h"
#include "mesh.pb.h"
#include <fmt/format.h>
#include <iostream>

// Global instance pointer
static class MeshtasticPluginInstance* g_instance = nullptr;

// Minimal plugin instance class - only implements required virtual methods
class MeshtasticPluginInstance : public ModuleManager::Instance {
public:
    MeshtasticPluginInstance() : enabled_(false) {
        fmt::print("MeshtasticPlugin: Created instance\n");
    }

    ~MeshtasticPluginInstance() override {
        fmt::print("MeshtasticPlugin: Destroyed instance\n");
    }

    void postInit() override {
        fmt::print("MeshtasticPlugin: postInit\n");
    }

    void enable() override {
        try {
            fmt::print("MeshtasticPlugin: enable() called\n");
            enabled_ = true;
            fmt::print("MeshtasticPlugin: Enabled\n");
            // TODO: Inicializar LoRa demodulator, decryptor, etc.
        } catch (const std::exception& e) {
            fmt::print("MeshtasticPlugin: Enable failed: {}\n", e.what());
            enabled_ = false;
            throw;
        } catch (...) {
            fmt::print("MeshtasticPlugin: Enable failed: unknown exception\n");
            enabled_ = false;
            throw;
        }
    }

    void disable() override {
        try {
            fmt::print("MeshtasticPlugin: disable() called\n");
            enabled_ = false;
            fmt::print("MeshtasticPlugin: Disabled\n");
            // TODO: Limpiar recursos LoRa, decryptor, etc.
        } catch (const std::exception& e) {
            fmt::print("MeshtasticPlugin: Disable failed: {}\n", e.what());
        } catch (...) {
            fmt::print("MeshtasticPlugin: Disable failed: unknown exception\n");
        }
    }

    bool isEnabled() override {
        return enabled_;
    }

private:
    bool enabled_ = false;
};

// Module info export - MUST be exported for SDR++ to find the plugin
MOD_EXPORT const ModuleManager::ModuleInfo_t _INFO_ = {
    "meshtastic_decoder",
    "Decodes and decrypts Meshtastic LoRa packets",
    "Victor",
    1,  // versionMajor
    0,  // versionMinor
    0,  // versionBuild
    1   // maxInstances
};

// C API exports
extern "C" {
    __declspec(dllexport) void _INIT_() {
        fmt::print("MeshtasticPlugin: _INIT_ called\n");
    }

    __declspec(dllexport) ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
        fmt::print("MeshtasticPlugin: _CREATE_INSTANCE_ '{}'\n", name);
        if (g_instance) {
            delete g_instance;
        }
        g_instance = new MeshtasticPluginInstance();
        return g_instance;
    }

    __declspec(dllexport) void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
        fmt::print("MeshtasticPlugin: _DELETE_INSTANCE_\n");
        if (instance == g_instance) {
            delete g_instance;
            g_instance = nullptr;
        }
    }

    __declspec(dllexport) void _END_() {
        fmt::print("MeshtasticPlugin: _END_\n");
        if (g_instance) {
            delete g_instance;
            g_instance = nullptr;
        }
    }
}