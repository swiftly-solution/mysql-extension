#ifndef _entrypoint_h
#define _entrypoint_h

#include <string>
#include <any>
#include <vector>
#include <functional>
#include <deque>

#include <swiftly-ext/core.h>
#include <swiftly-ext/extension.h>
#include <swiftly-ext/hooks/NativeHooks.h>

class MySQLExtension : public SwiftlyExt
{
public:
    bool Load(std::string& error, SourceHook::ISourceHook *SHPtr, ISmmAPI* ismm, bool late);
    bool Unload(std::string& error);
    
    void AllExtensionsLoaded();
    void AllPluginsLoaded();

    bool OnPluginLoad(std::string pluginName, void* pluginState, PluginKind_t kind, std::string& error);
    bool OnPluginUnload(std::string pluginName, void* pluginState, PluginKind_t kind, std::string& error);

    void Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick);
    void Hook_ServerHibernationUpdate(bool bHibernation);

public:
    const char* GetAuthor();
    const char* GetName();
    const char* GetVersion();
    const char* GetWebsite();

    void NextFrame(std::function<void(std::vector<std::any>)> fn, std::vector<std::any> param);

private:
    std::deque<std::pair<std::function<void(std::vector<std::any>)>, std::vector<std::any>>> m_nextFrame;
};

extern MySQLExtension g_Ext;
extern ISource2Server* server;
DECLARE_GLOBALVARS();

#endif