#include "entrypoint.h"
#include "driver/DBDriver.h"
#include <swiftly-ext/files.h>
#include "metamod_oslink.h"

//////////////////////////////////////////////////////////////
/////////////////        Core Variables        //////////////
////////////////////////////////////////////////////////////

CREATE_GLOBALVARS();

SH_DECL_HOOK1_void(ISource2Server, PreWorldUpdate, SH_NOATTRIB, 0, bool);

ISource2Server* server = nullptr;

MySQLExtension g_Ext;
DBDriver g_dbDriver;

typedef void (*RegisterDriver)(void*);

//////////////////////////////////////////////////////////////
/////////////////          Core Class          //////////////
////////////////////////////////////////////////////////////

#ifdef _WIN32
FILE _ioccc[] = { *stdin, *stdout, *stderr };
extern "C" FILE* __cdecl __iob_func(void)
{
    return _ioccc;
}
#endif

EXT_EXPOSE(g_Ext);
bool MySQLExtension::Load(std::string& error, SourceHook::ISourceHook* SHPtr, ISmmAPI* ismm, bool late)
{
    SAVE_GLOBALVARS();

    GET_IFACE_ANY(GetServerFactory, server, ISource2Server, INTERFACEVERSION_SERVERGAMEDLL);

    HINSTANCE m_hModule;
#ifdef _WIN32
    m_hModule = dlmount(GeneratePath("addons/swiftly/bin/win64/swiftly.dll"));
#else
    m_hModule = dlopen(GeneratePath("addons/swiftly/bin/linuxsteamrt64/swiftly.so"), RTLD_NOW);
    if (!m_hModule) {
        error = "Could not open swiftly.so as a module.";
        return false;
    }
#endif

    void* regPtr = reinterpret_cast<void*>(dlsym(m_hModule, "swiftly_RegisterDBDriver"));
    if (!regPtr) {
        error = "Could not find the following exported function: 'swiftly_RegisterDBDriver'.";
        dlclose(m_hModule);
        return false;
    }

    dlclose(m_hModule);

    reinterpret_cast<RegisterDriver>(regPtr)((void*)&g_dbDriver);

    SH_ADD_HOOK_MEMFUNC(ISource2Server, PreWorldUpdate, server, this, &MySQLExtension::PreWorldUpdate, true);

    return true;
}

void MySQLExtension::PreWorldUpdate(bool bSimulating)
{
    while (!m_nextFrame.empty())
    {
        auto pair = m_nextFrame.front();
        pair.first(pair.second);
        m_nextFrame.pop_front();
    }
}

void MySQLExtension::NextFrame(std::function<void(std::vector<std::any>)> fn, std::vector<std::any> param)
{
    m_nextFrame.push_back({ fn, param });
}

bool MySQLExtension::Unload(std::string& error)
{
    SH_REMOVE_HOOK_MEMFUNC(ISource2Server, PreWorldUpdate, server, this, &MySQLExtension::PreWorldUpdate, true);

    return true;
}

void MySQLExtension::AllExtensionsLoaded()
{

}

void MySQLExtension::AllPluginsLoaded()
{

}

bool MySQLExtension::OnPluginLoad(std::string pluginName, void* pluginState, PluginKind_t kind, std::string& error)
{
    return true;
}

bool MySQLExtension::OnPluginUnload(std::string pluginName, void* pluginState, PluginKind_t kind, std::string& error)
{
    return true;
}

const char* MySQLExtension::GetAuthor()
{
    return "Swiftly Development Team";
}

const char* MySQLExtension::GetName()
{
    return "MySQL Database Extension";
}

const char* MySQLExtension::GetVersion()
{
#ifndef VERSION
    return "Local";
#else
    return VERSION;
#endif
}

const char* MySQLExtension::GetWebsite()
{
    return "https://swiftlycs2.net/";
}
