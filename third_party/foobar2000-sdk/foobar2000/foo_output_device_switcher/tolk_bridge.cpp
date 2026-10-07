#include "stdafx.h"
#include "tolk_bridge.h"
#include <memory>

namespace {
using Tolk_Load_t = void(__cdecl*)();
using Tolk_Unload_t = void(__cdecl*)();
using Tolk_TrySAPI_t = void(__cdecl*)(bool);
using Tolk_IsLoaded_t = bool(__cdecl*)();
using Tolk_Output_t = bool(__cdecl*)(const wchar_t*, bool);
using Tolk_Speak_t = bool(__cdecl*)(const wchar_t*, bool);
using Tolk_Silence_t = bool(__cdecl*)();

enum task_kind { task_none, task_speak, task_silence, task_quit };

HMODULE g_tolk = nullptr;
Tolk_Load_t pLoad = nullptr;
Tolk_Unload_t pUnload = nullptr;
Tolk_TrySAPI_t pTrySAPI = nullptr;
Tolk_IsLoaded_t pIsLoaded = nullptr;
Tolk_Output_t pOutput = nullptr;
Tolk_Speak_t pSpeak = nullptr;
Tolk_Silence_t pSilence = nullptr;
bool g_loaded = false;
bool g_load_failed = false;

constexpr wchar_t k_tolk_file_name[] = L"foo_output_device_switcher_tolk.dll";

CRITICAL_SECTION g_tolk_lock;
CRITICAL_SECTION g_task_lock;
bool g_locks_ready = false;

enum class worker_lifecycle_state {
    stopped,
    running,
    stopping,
};

struct worker_context {
    HANDLE event = nullptr;
    HANDLE stopped_event = nullptr;

    worker_context() = default;
    worker_context(const worker_context&) = delete;
    worker_context& operator=(const worker_context&) = delete;

    ~worker_context() {
        if (event) CloseHandle(event);
        if (stopped_event) CloseHandle(stopped_event);
    }
};

using worker_context_ptr = std::shared_ptr<worker_context>;

worker_context_ptr g_worker_context;
worker_lifecycle_state g_worker_state = worker_lifecycle_state::stopped;
bool g_shutdown_requested = false;
bool g_worker_quit = false;
task_kind g_task = task_none;
std::wstring g_task_text;
bool g_task_interrupt = true;
std::wstring g_component_dir;

void init_locks_once() {
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    InitOnceExecuteOnce(&once, [](PINIT_ONCE, PVOID, PVOID*) -> BOOL {
        InitializeCriticalSection(&g_tolk_lock);
        InitializeCriticalSection(&g_task_lock);
        g_locks_ready = true;
        return TRUE;
    }, nullptr, nullptr);
}

bool safe_try_sapi(Tolk_TrySAPI_t fn, bool enable) {
    if (!fn) return true;
    __try { fn(enable); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool safe_load(Tolk_Load_t fn) {
    if (!fn) return false;
    __try { fn(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool safe_unload(Tolk_Unload_t fn) {
    if (!fn) return true;
    __try { fn(); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool safe_is_loaded(Tolk_IsLoaded_t fn) {
    if (!fn) return true;
    __try { return fn(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

template<typename Fn>
bool safe_text_call(Fn fn, const wchar_t* text, bool interrupt) {
    if (!fn) return false;
    __try { return fn(text, interrupt); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void safe_silence(Tolk_Silence_t fn) {
    if (!fn) return;
    __try { fn(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
}

void debug_log(const std::wstring& message) {
    std::wstring line = L"foo_output_device_switcher: ";
    line += message;
    line += L"\r\n";
    OutputDebugStringW(line.c_str());
}

std::wstring module_path(HMODULE module) {
    if (!module) return L"";

    std::vector<wchar_t> buffer(512);
    for (;;) {
        SetLastError(ERROR_SUCCESS);
        const DWORD length = GetModuleFileNameW(
            module, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return L"";
        if (length < buffer.size() - 1) {
            return std::wstring(buffer.data(), length);
        }
        if (buffer.size() >= 32768) return L"";
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring directory_from_path(const std::wstring& path) {
    const std::size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return L"";
    return path.substr(0, separator);
}

std::wstring current_dll_dir() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&current_dll_dir),
            &module)) {
        debug_log(L"component module lookup failed; error=" +
            std::to_wstring(GetLastError()));
        return L"";
    }

    const std::wstring path = module_path(module);
    const std::wstring directory = directory_from_path(path);
    if (directory.empty()) {
        debug_log(L"component module path lookup failed");
    }
    return directory;
}

std::wstring join_path(const std::wstring& directory, const wchar_t* name) {
    if (directory.empty() || !name || !*name) return L"";
    std::wstring path = directory;
    if (path.back() != L'\\' && path.back() != L'/') path += L'\\';
    path += name;
    return path;
}

void log_runtime_driver_paths(const std::wstring& tolk_dir) {
#ifdef _WIN64
    const std::pair<const wchar_t*, const wchar_t*> drivers[] = {
        {L"NVDA driver", L"ods-nvda-client-64bits.dll"},
        {L"Boy driver", L"ods-br-x64.dll"},
        {L"ZDSR driver", L"ods-zsr_x64.dll"},
        {L"System Access driver", L"odssa64.dll"},
    };
#else
    const std::pair<const wchar_t*, const wchar_t*> drivers[] = {
        {L"NVDA driver", L"ods-nvda-client-32bits.dll"},
        {L"Boy driver", L"ods-br.dll"},
        {L"ZDSR driver", L"ods-zsr.dll"},
        {L"System Access driver", L"odssa32.dll"},
        {L"SuperNova driver", L"odsdol32.dll"},
    };
#endif

    for (const auto& driver : drivers) {
        const std::wstring path = join_path(tolk_dir, driver.second);
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            debug_log(std::wstring(driver.first) + L" file missing: " + path +
                L"; error=" + std::to_wstring(GetLastError()));
        } else {
            debug_log(std::wstring(driver.first) + L" file: " + path);
        }
    }
}

void log_loaded_runtime_driver(const wchar_t* role, const wchar_t* name) {
    if (!name || !*name) return;
    HMODULE module = GetModuleHandleW(name);
    if (!module) {
        debug_log(std::wstring(role ? role : L"runtime") +
            L" was not loaded by Tolk: " + name);
        return;
    }

    const std::wstring actual_path = module_path(module);
    std::wstring message = role ? role : L"runtime";
    message += L" loaded by Tolk: ";
    message += actual_path.empty() ? name : actual_path;
    debug_log(message);
}

void reset_tolk_symbols() {
    pLoad = nullptr;
    pUnload = nullptr;
    pTrySAPI = nullptr;
    pIsLoaded = nullptr;
    pOutput = nullptr;
    pSpeak = nullptr;
    pSilence = nullptr;
}

bool ensure_loaded() {
    if (g_loaded && (pSpeak || pOutput)) return true;
    if (g_load_failed) return false;

    if (g_component_dir.empty()) g_component_dir = current_dll_dir();
    if (g_component_dir.empty()) {
        g_load_failed = true;
        return false;
    }

    const std::wstring tolk_dir = join_path(g_component_dir, L"tolk");
    const wchar_t* architecture = sizeof(void*) == 8 ? L"x64" : L"x86";
    debug_log(std::wstring(L"loading Tolk runtime; architecture=") +
        architecture + L"; component_dir=" + g_component_dir);
    log_runtime_driver_paths(tolk_dir);

    const std::wstring path = join_path(tolk_dir, k_tolk_file_name);
    SetLastError(ERROR_SUCCESS);
    g_tolk = LoadLibraryExW(
        path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!g_tolk) {
        g_load_failed = true;
        debug_log(L"Tolk runtime failed to load: " + path + L"; error=" +
            std::to_wstring(GetLastError()));
        return false;
    }

    const std::wstring actual_tolk_path = module_path(g_tolk);
    std::wstring loaded_message = L"Tolk runtime loaded: ";
    loaded_message += path;
    if (!actual_tolk_path.empty()) loaded_message += L"; actual=" + actual_tolk_path;
    debug_log(loaded_message);

    pLoad = reinterpret_cast<Tolk_Load_t>(GetProcAddress(g_tolk, "Tolk_Load"));
    pUnload = reinterpret_cast<Tolk_Unload_t>(GetProcAddress(g_tolk, "Tolk_Unload"));
    pTrySAPI = reinterpret_cast<Tolk_TrySAPI_t>(GetProcAddress(g_tolk, "Tolk_TrySAPI"));
    pIsLoaded = reinterpret_cast<Tolk_IsLoaded_t>(GetProcAddress(g_tolk, "Tolk_IsLoaded"));
    pOutput = reinterpret_cast<Tolk_Output_t>(GetProcAddress(g_tolk, "Tolk_Output"));
    pSpeak = reinterpret_cast<Tolk_Speak_t>(GetProcAddress(g_tolk, "Tolk_Speak"));
    pSilence = reinterpret_cast<Tolk_Silence_t>(GetProcAddress(g_tolk, "Tolk_Silence"));
    if (!pLoad || !pUnload || (!pSpeak && !pOutput)) {
        g_load_failed = true;
        debug_log(L"Tolk runtime is missing required exports");
        FreeLibrary(g_tolk);
        g_tolk = nullptr;
        reset_tolk_symbols();
        return false;
    }

    safe_try_sapi(pTrySAPI, false);
    if (!safe_load(pLoad)) {
        g_load_failed = true;
        debug_log(L"Tolk_Load raised an exception");
        // Tolk may have partially initialized a driver before the exception.
        // Keep the module mapped and stop using it rather than unloading code
        // whose internal state is no longer known to be consistent.
        reset_tolk_symbols();
        return false;
    }

    if (!safe_is_loaded(pIsLoaded)) {
        g_load_failed = true;
        debug_log(L"Tolk_Load returned but Tolk reports not loaded");
        // Keep the module mapped on an initialization failure. The isolated
        // runtime owns the driver handles and may have performed partial work.
        reset_tolk_symbols();
        return false;
    }

    g_loaded = true;
    debug_log(L"Tolk_Load succeeded");
#ifdef _WIN64
    log_loaded_runtime_driver(L"NVDA driver", L"ods-nvda-client-64bits.dll");
    log_loaded_runtime_driver(L"Boy driver", L"ods-br-x64.dll");
    log_loaded_runtime_driver(L"ZDSR driver", L"ods-zsr_x64.dll");
    log_loaded_runtime_driver(L"System Access driver", L"odssa64.dll");
#else
    log_loaded_runtime_driver(L"NVDA driver", L"ods-nvda-client-32bits.dll");
    log_loaded_runtime_driver(L"Boy driver", L"ods-br.dll");
    log_loaded_runtime_driver(L"ZDSR driver", L"ods-zsr.dll");
    log_loaded_runtime_driver(L"System Access driver", L"odssa32.dll");
    log_loaded_runtime_driver(L"SuperNova driver", L"odsdol32.dll");
#endif
    return true;
}

bool speak_direct(const wchar_t* text, bool interrupt) {
    init_locks_once();
    EnterCriticalSection(&g_tolk_lock);
    bool ok = true;
    if (text && *text) {
        ok = ensure_loaded();
        if (ok) {
            ok = safe_text_call(pSpeak, text, interrupt);
            if (!ok) ok = safe_text_call(pOutput, text, interrupt);
        }
    }
    LeaveCriticalSection(&g_tolk_lock);
    return ok;
}

void silence_direct() {
    init_locks_once();
    EnterCriticalSection(&g_tolk_lock);
    if (g_loaded && pSilence) safe_silence(pSilence);
    LeaveCriticalSection(&g_tolk_lock);
}

void unload_direct() {
    init_locks_once();
    EnterCriticalSection(&g_tolk_lock);
    if (g_loaded) {
        if (pSilence) safe_silence(pSilence);

        const bool unloaded = safe_unload(pUnload);
        g_loaded = false;
        reset_tolk_symbols();
        if (unloaded) {
            debug_log(L"Tolk_Unload succeeded");
            if (g_tolk) {
                if (FreeLibrary(g_tolk)) {
                    g_tolk = nullptr;
                } else {
                    g_load_failed = true;
                    debug_log(L"Tolk runtime FreeLibrary failed; error=" +
                        std::to_wstring(GetLastError()));
                }
            }
        } else {
            // Keep the module mapped if a driver fails during Tolk_Unload.
            // This is safer than freeing code that may still be referenced
            // by a screen-reader callback or worker inside a driver.
            g_load_failed = true;
            debug_log(L"Tolk_Unload raised an exception; keeping runtime loaded");
        }
    }
    LeaveCriticalSection(&g_tolk_lock);
}

void mark_worker_finished(const worker_context_ptr& context) {
    EnterCriticalSection(&g_task_lock);
    if (g_worker_context.get() == context.get()) {
        g_worker_context.reset();
        g_worker_state = worker_lifecycle_state::stopped;
        g_worker_quit = false;
        g_task = task_none;
        g_task_text.clear();
    }
    LeaveCriticalSection(&g_task_lock);
}

DWORD WINAPI worker_proc(const worker_context_ptr& context) {
    const HANDLE workerEvent = context->event;
    for (;;) {
        WaitForSingleObject(workerEvent, INFINITE);

        task_kind kind = task_none;
        std::wstring text;
        bool interrupt = true;
        EnterCriticalSection(&g_task_lock);
        kind = g_task;
        text.swap(g_task_text);
        interrupt = g_task_interrupt;
        g_task = task_none;
        bool quit = g_worker_quit || kind == task_quit;
        LeaveCriticalSection(&g_task_lock);

        if (quit) break;
        if (kind == task_silence) silence_direct();
        else if (kind == task_speak) speak_direct(text.c_str(), interrupt);
    }

    silence_direct();
    unload_direct();
    return 0;
}

bool ensure_worker() {
    init_locks_once();
    EnterCriticalSection(&g_task_lock);
    if (g_shutdown_requested || g_worker_state == worker_lifecycle_state::stopping) {
        LeaveCriticalSection(&g_task_lock);
        return false;
    }
    if (g_worker_state == worker_lifecycle_state::running) {
        const bool ready = g_worker_context &&
            g_worker_context->event != nullptr &&
            g_worker_context->stopped_event != nullptr;
        LeaveCriticalSection(&g_task_lock);
        return ready;
    }

    worker_context_ptr context;
    try {
        context = std::make_shared<worker_context>();
    } catch (...) {
        LeaveCriticalSection(&g_task_lock);
        return false;
    }

    context->event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!context->event) {
        LeaveCriticalSection(&g_task_lock);
        return false;
    }
    context->stopped_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!context->stopped_event) {
        LeaveCriticalSection(&g_task_lock);
        return false;
    }

    g_worker_quit = false;
    g_task = task_none;
    g_worker_context = context;
    g_worker_state = worker_lifecycle_state::running;
    try {
        // SDK-managed worker lifetime keeps the component loaded until the
        // worker has finished all Tolk calls and handle cleanup.
        fb2k::splitTask([context]() {
            try {
                worker_proc(context);
            } catch (...) {
                // Always signal the stopped event below so shutdown can
                // release its reference without closing a live worker handle.
            }
            mark_worker_finished(context);
            SetEvent(context->stopped_event);
        });
    } catch (...) {
        g_worker_context.reset();
        g_worker_state = worker_lifecycle_state::stopped;
        LeaveCriticalSection(&g_task_lock);
        return false;
    }
    LeaveCriticalSection(&g_task_lock);
    return true;
}

void post_task(task_kind kind, const wchar_t* text, bool interrupt) {
    if (!ensure_worker()) return;
    HANDLE event = nullptr;
    EnterCriticalSection(&g_task_lock);
    if (g_shutdown_requested || g_worker_state != worker_lifecycle_state::running || !g_worker_context) {
        LeaveCriticalSection(&g_task_lock);
        return;
    }
    g_task = kind;
    g_task_text = text ? text : L"";
    g_task_interrupt = interrupt;
    event = g_worker_context->event;
    LeaveCriticalSection(&g_task_lock);
    if (event) SetEvent(event);
}

}

void tolk_queue_speak(const wchar_t* text, bool interrupt) {
    if (!text || !*text) return;
    post_task(task_speak, text, interrupt);
}

void tolk_queue_silence() {
    post_task(task_silence, L"", true);
}

void tolk_shutdown() {
    init_locks_once();
    worker_context_ptr context;
    HANDLE event = nullptr;
    EnterCriticalSection(&g_task_lock);
    g_shutdown_requested = true;
    if (g_worker_state == worker_lifecycle_state::running && g_worker_context) {
        g_worker_state = worker_lifecycle_state::stopping;
        context = g_worker_context;
    } else if (g_worker_state == worker_lifecycle_state::stopping) {
        // A previous shutdown may still be waiting for a blocked Tolk call.
        // Reuse the same context and wait again instead of closing handles.
        context = g_worker_context;
    }
    if (context) {
        g_worker_quit = true;
        g_task = task_quit;
        event = context->event;
    }
    LeaveCriticalSection(&g_task_lock);

    if (!context) {
        unload_direct();
        return;
    }
    if (event) SetEvent(event);

    const DWORD waitResult = context->stopped_event
        ? WaitForSingleObject(context->stopped_event, 5000) : WAIT_FAILED;
    if (waitResult != WAIT_OBJECT_0) {
        // Do not close either event while the SDK-managed worker may still be
        // executing a Tolk call. The worker owns final cleanup and signals
        // stopped_event only after its last Tolk call has returned.
        return;
    }
}
