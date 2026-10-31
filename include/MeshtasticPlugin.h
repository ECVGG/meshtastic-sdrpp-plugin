#pragma once

#include <module.h>
#include <string>

class MeshtasticPlugin : public ModuleManager::Instance {
public:
    MeshtasticPlugin(std::string name);
    ~MeshtasticPlugin();

    void postInit() override;
    void enable() override;
    void disable() override;
    bool isEnabled() override;

private:
    std::string name;
    bool enabled = true;

    // Friend to allow C-style exports
    friend MeshtasticPlugin* createMeshtasticPlugin(std::string name);
    friend void destroyMeshtasticPlugin(MeshtasticPlugin* plugin);
};

// C-style factory functions for SDR++ module system
extern "C" {
    MOD_EXPORT ModuleManager::Instance* createMeshtasticPlugin(std::string name);
    MOD_EXPORT void destroyMeshtasticPlugin(ModuleManager::Instance* instance);
}