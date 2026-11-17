#pragma once

#include <string>
#include <complex>

// Plugin export macros (copied from SDR++ module.h)
#ifdef _WIN32
#include <Windows.h>
#define MOD_EXPORT           extern "C" __declspec(dllexport)
#else
#define MOD_EXPORT extern "C"
#endif

// Minimal ModuleManager interface (matches SDR++)
class ModuleManager {
public:
    struct ModuleInfo_t {
        const char* name;
        const char* description;
        const char* author;
        const int versionMajor;
        const int versionMinor;
        const int versionBuild;
        const int maxInstances;
    };

    class Instance {
    public:
        virtual ~Instance() {}
        virtual void postInit() = 0;
        virtual void enable() = 0;
        virtual void disable() = 0;
        virtual bool isEnabled() = 0;
    };

    // Module load/creation functions
    struct Module_t {
#ifdef _WIN32
        HMODULE handle;
#else
        void* handle;
#endif
        ModuleInfo_t* info;
        void (*init)();
        Instance* (*createInstance)(std::string name);
        void (*deleteInstance)(Instance* instance);
        void (*end)();
    };

    // Events
    template<typename T> class Event {
    public:
        void operator()(T arg) {}
    };
};

// Module info export macro
#define SDRPP_MOD_INFO MOD_EXPORT const ModuleManager::ModuleInfo_t _INFO_