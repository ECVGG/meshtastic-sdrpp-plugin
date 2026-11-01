#pragma once

// Forward declarations for SDR++ types - avoid including internal headers
// These are provided by SDRPlusPlus::SDRPlusPlus INTERFACE target
namespace sdrpp {
    class ModuleManager;
    class ConfigManager;
    class SignalPath;
    class IQStream;
    class IQFrontEnd;
    namespace gui { class Menu; }
    namespace flog { void info(const char* fmt, ...); void warn(const char* fmt, ...); }
    namespace core { struct Args { const char* s() const; }; extern Args args; }
    namespace sigpath { extern IQFrontEnd* iqFrontEnd; }
}

// Module macro - defined by SDR++ module system
#ifndef SDRPP_MOD_INFO
#define SDRPP_MOD_INFO struct SDRPP_ModuleInfo
#endif
#ifndef MOD_EXPORT
#define MOD_EXPORT __declspec(dllexport)
#endif

#include <string>
#include <memory>
#include "MeshtasticDecoder.h"

class MeshtasticPlugin : public sdrpp::ModuleManager::Instance {
public:
    MeshtasticPlugin(std::string name);
    ~MeshtasticPlugin();

    void postInit() override;
    void enable() override;
    void disable() override;
    bool isEnabled() override;

    // IQ stream processing
    void processIQStream();

private:
    std::string name;
    bool enabled = true;
    std::unique_ptr<MeshtasticDecoder> decoder;
    sdrpp::IQStream* iqStream = nullptr;
    sdrpp::IQFrontEnd* iqFrontEnd = nullptr;
    sdrpp::ConfigManager* config = nullptr;

    // GUI
    void drawMenu();
    static void menuHandler(void* ctx);

    // Configuration persistence
    void loadConfig();
    void saveConfig();

    int selectedChannel = -1;

    // Friend to allow C-style exports
    friend MeshtasticPlugin* createMeshtasticPlugin(std::string name);
    friend void destroyMeshtasticPlugin(MeshtasticPlugin* plugin);
};

// C-style factory functions for SDR++ module system
extern "C" {
    MOD_EXPORT sdrpp::ModuleManager::Instance* createMeshtasticPlugin(std::string name);
    MOD_EXPORT void destroyMeshtasticPlugin(sdrpp::ModuleManager::Instance* instance);
}