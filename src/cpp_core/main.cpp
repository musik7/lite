// Chromium DevTools C++ Core WASM Main Entrypoint
// Pure C++17 compilation without GoogleTest / Testing dependencies

#include <iostream>
#include "core.h"
#include "wasm_exports.h"

int main() {
    std::cout << "[Chromium DevTools C++ Core Engine]" << std::endl;
    std::cout << "Version: v1.8.4 (Chromium M124 Standards Compatible)" << std::endl;
    
    // Initialize Arena Allocator
    ArenaAllocator arena(1024 * 1024); // 1 MB Linear Heap
    JsonParser parser(&arena);

    // Initialize CDP Bridge & WebSocket Agent
    CdpBridge cdp_bridge(&arena, &parser);
    WebSocketCDPAgent ws_agent(&arena, &cdp_bridge);

    std::cout << "✓ Arena Allocator initialized (1024 KB)" << std::endl;
    std::cout << "✓ CDP Bridge & WebSocket Agent initialized" << std::endl;
    std::cout << "✓ Ready for WebAssembly / Standalone Native compilation" << std::endl;

    return 0;
}
