#ifndef LUA_PLUGIN_MANAGER_H
#define LUA_PLUGIN_MANAGER_H

#include <string>
#include <vector>
#include <cstdint>

// Lua includes - wrapped in extern "C" for C++ compatibility
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

// Enum bitmask to quickly check if a plugin subscribes to an event
enum class PluginCapability : uint32_t {
    NONE              = 0,
    ON_BOOT           = 1 << 0,
    ON_SLEEP          = 1 << 1,
    ON_MAIN_MENU_BUILD = 1 << 2,  // For the main home screen menu
    ON_READER_MENU_BUILD = 1 << 3, // For the in-reader context menu
    ON_CHAPTER_INDEX  = 1 << 4,
    ON_SETTINGS_UI    = 1 << 5,
    // Add more hooks here as needed
};

// Bitwise operators for PluginCapability
inline PluginCapability operator|(PluginCapability a, PluginCapability b) {
    return static_cast<PluginCapability>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(PluginCapability a, PluginCapability b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// Events that can be fired by the system.
// Some map 1:1 to capabilities, others are implied by them.
enum class PluginEvent {
    ON_BOOT,
    ON_SLEEP,
    ON_MAIN_MENU_BUILD,
    ON_READER_MENU_BUILD,
    ON_CHAPTER_INDEX,
    ON_SETTINGS_UI,
    ON_MENU_ACTION,      // Implied by MENU_BUILD capabilities
    ON_SETTINGS_CHANGE   // Implied by SETTINGS_UI capability
};

struct PluginMetadata {
    std::string id;           // Folder name, e.g., "battery-logger"
    std::string name;         // Human readable name from manifest
    PluginCapability capabilities;    // Bitmask of subscribed events
    bool enabled;             // User toggle
};

// Context structures for passing data to events
// This allows passing complex C++ data to Lua without exposing internal classes directly
struct MenuContext {
    std::string menuName; // e.g., "HOME" or "READER"
    struct Entry {
        std::string label;
        std::string actionId;
    };
    std::vector<Entry> newEntries; // Plugins will append to this vector
};

struct MenuActionContext {
    std::string actionId;
};

struct SettingsChangeContext {
    std::string key;
    std::string newValue;
};

class LuaPluginManager {
public:
    static LuaPluginManager& getInstance();

    // Discovery: Scans /sdcard/.crosspoint/plugins/ for manifest.json files
    // and populates the installedPlugins list.
    void init();

    // The Trigger: Fires an event to all subscribed plugins.
    // Returns true if at least one plugin handled the event.
    // contextData is a pointer to a specific context struct (e.g., MenuContext*) based on eventType.
    bool fireEvent(PluginEvent eventType, void* contextData = nullptr);

    const std::vector<PluginMetadata>& getPlugins() const { return installedPlugins; }

private:
    LuaPluginManager() = default;
    ~LuaPluginManager() = default;
    LuaPluginManager(const LuaPluginManager&) = delete;
    LuaPluginManager& operator=(const LuaPluginManager&) = delete;

    std::vector<PluginMetadata> installedPlugins;

    // Custom allocator to enforce RAM limits on Lua states.
    // This prevents a plugin from consuming all available system RAM.
    static void* restrictedAlloc(void* ud, void* ptr, size_t osize, size_t nsize);

    // Helpers to manage the ephemeral Lua state
    lua_State* createLuaState(size_t* memoryCounter);
    void destroyLuaState(lua_State* L);

    // Registers C++ functions (cp.log, cp.ui...) into the Lua state
    void registerNativeBindings(lua_State* L);
    
    // Helper to execute the specific hook function in a plugin
    void executePluginHook(lua_State* L, const PluginMetadata& plugin, const char* hookName, void* contextData);
};

#endif // LUA_PLUGIN_MANAGER_H