#include "LuaPluginManager.h"
#include <Logging.h>
#include <cstdlib>

// Define the memory limit for a single plugin execution (e.g., 20KB).
// This is crucial for the ESP32-C3 with limited RAM.
static const size_t PLUGIN_MEMORY_LIMIT = 20 * 1024;

LuaPluginManager& LuaPluginManager::getInstance() {
    static LuaPluginManager instance;
    return instance;
}

void LuaPluginManager::init() {
    LOG_INF("LUA", "Initializing Plugin Manager...");
    // TODO: Implement SD card scanning logic here.
    // It should iterate over /sdcard/.crosspoint/plugins/, read manifest.json,
    // and populate installedPlugins.
}

void* LuaPluginManager::restrictedAlloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    size_t* used_mem = static_cast<size_t*>(ud);

    if (nsize == 0) { // Free
        if (*used_mem >= osize) {
            *used_mem -= osize;
        } else {
            // This should not happen if accounting is correct
            *used_mem = 0; 
        }
        free(ptr);
        return nullptr;
    } else { // Realloc / Malloc
        size_t needed = nsize - (ptr ? osize : 0);
        
        // Check if allocation would exceed the limit
        if (*used_mem + needed > PLUGIN_MEMORY_LIMIT) {
            LOG_ERR("LUA", "Plugin exceeded memory limit! Used: %d, Requested: %d", *used_mem, needed);
            return nullptr; 
        }
        
        void* new_ptr = realloc(ptr, nsize);
        if (new_ptr) {
            *used_mem += needed;
        }
        return new_ptr;
    }
}

lua_State* LuaPluginManager::createLuaState(size_t* memoryCounter) {
    *memoryCounter = 0;
    // Create a new Lua state with our custom allocator
    lua_State* L = lua_newstate(restrictedAlloc, memoryCounter);
    
    if (!L) {
        LOG_ERR("LUA", "Failed to create Lua state (OOM)");
        return nullptr;
    }

    // Load standard libraries (restricted set)
    // We intentionally avoid loading 'io' and 'os' fully for security/sandbox reasons.
    // We only load what is strictly necessary.
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "table", luaopen_table, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "string", luaopen_string, 1);
    lua_pop(L, 1);
    luaL_requiref(L, "math", luaopen_math, 1);
    lua_pop(L, 1);

    registerNativeBindings(L);

    return L;
}

void LuaPluginManager::destroyLuaState(lua_State* L) {
    if (L) {
        lua_close(L);
    }
}

void LuaPluginManager::registerNativeBindings(lua_State* L) {
    // TODO: Register 'cp' namespace and functions here.
    // Example: cp.log(), cp.ui.toast(), etc.
}

bool LuaPluginManager::fireEvent(PluginEvent eventType, void* contextData) {
    bool handled = false;
    
    for (const auto& plugin : installedPlugins) {
        if (!plugin.enabled) continue;
        
        // Check if the plugin subscribes to this event (directly or implicitly)
        bool isSubscribed = false;
        switch (eventType) {
            case PluginEvent::ON_BOOT: isSubscribed = plugin.capabilities & PluginCapability::ON_BOOT; break;
            case PluginEvent::ON_SLEEP: isSubscribed = plugin.capabilities & PluginCapability::ON_SLEEP; break;
            case PluginEvent::ON_MAIN_MENU_BUILD: isSubscribed = plugin.capabilities & PluginCapability::ON_MAIN_MENU_BUILD; break;
            case PluginEvent::ON_READER_MENU_BUILD: isSubscribed = plugin.capabilities & PluginCapability::ON_READER_MENU_BUILD; break;
            case PluginEvent::ON_CHAPTER_INDEX: isSubscribed = plugin.capabilities & PluginCapability::ON_CHAPTER_INDEX; break;
            case PluginEvent::ON_SETTINGS_UI: isSubscribed = plugin.capabilities & PluginCapability::ON_SETTINGS_UI; break;
            
            case PluginEvent::ON_MENU_ACTION:
                // Implied: If you build a menu, you handle its actions
                isSubscribed = plugin.capabilities & (PluginCapability::ON_MAIN_MENU_BUILD | PluginCapability::ON_READER_MENU_BUILD);
                break;
                
            case PluginEvent::ON_SETTINGS_CHANGE:
                // Implied: If you show settings, you handle their changes
                isSubscribed = plugin.capabilities & PluginCapability::ON_SETTINGS_UI;
                break;
        }

        if (isSubscribed) {
            
            size_t memoryUsed = 0;
            lua_State* L = createLuaState(&memoryUsed);
            if (!L) continue;

            // Determine hook name based on eventType
            const char* hookName = nullptr;
            if (eventType == PluginEvent::ON_MAIN_MENU_BUILD) hookName = "on_build_main_menu";
            else if (eventType == PluginEvent::ON_READER_MENU_BUILD) hookName = "on_build_reader_menu";
            else if (eventType == PluginEvent::ON_BOOT) hookName = "on_boot";
            else if (eventType == PluginEvent::ON_MENU_ACTION) hookName = "on_menu_action";
            else if (eventType == PluginEvent::ON_SETTINGS_CHANGE) hookName = "on_settings_change";
            else if (eventType == PluginEvent::ON_SLEEP) hookName = "on_sleep"; // Assuming hook name
            else if (eventType == PluginEvent::ON_CHAPTER_INDEX) hookName = "on_index_chapter"; // Assuming hook name
            else if (eventType == PluginEvent::ON_SETTINGS_UI) hookName = "on_get_settings_schema"; // Assuming hook name

            if (hookName) {
                executePluginHook(L, plugin, hookName, contextData);
                handled = true; 
            }

            destroyLuaState(L);
        }
    }
    return handled;
}

void LuaPluginManager::executePluginHook(lua_State* L, const PluginMetadata& plugin, const char* hookName, void* contextData) {
    // TODO: Load the specific plugin script from SD card
    // std::string scriptPath = "/sdcard/.crosspoint/plugins/" + plugin.id + "/main.lua";
    // if (luaL_dofile(L, scriptPath.c_str()) != LUA_OK) { ... error handling ... }
    
    // TODO: Call the function 'hookName' passing 'contextData' converted to Lua table/userdata
}