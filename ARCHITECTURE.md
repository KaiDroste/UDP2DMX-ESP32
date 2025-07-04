# 🌐 UDP2DMX Gateway (ESP32) - Refactored Architecture

## 📁 Project Structure

```
main/
├── include/                     # Public header files
│   ├── dmx_manager.h           # DMX hardware abstraction
│   ├── udp_protocol.h          # UDP protocol handling
│   ├── udp_server.h            # UDP server implementation
│   └── system_config.h         # System configuration
├── src/                        # Source files
│   ├── main.c                  # Application entry point
│   ├── dmx_manager.c           # DMX management & fade engine
│   ├── udp_protocol.c          # Protocol parsing & execution
│   ├── udp_server.c            # UDP server & packet handling
│   └── system_config.c         # Configuration management
└── CMakeLists.txt              # Build configuration
```

## 🔧 Architecture Improvements

### 1. **Modular Design**
- **Separation of Concerns**: Each module has a specific responsibility
- **Clean Interfaces**: Well-defined APIs between modules
- **Testability**: Individual modules can be unit tested

### 2. **DMX Manager (`dmx_manager.h/c`)**
- Thread-safe DMX operations with mutex protection
- Fade engine with precise timing
- Hardware abstraction for DMX512 communication
- Bounds checking and validation

### 3. **UDP Protocol (`udp_protocol.h/c`)**
- Command parsing and validation
- Protocol-specific logic (Loxone compatibility)
- Error handling and result codes

### 4. **UDP Server (`udp_server.h/c`)**
- Network packet handling
- Statistics collection
- Robust error handling
- Clean start/stop mechanisms

### 5. **System Configuration (`system_config.h/c`)**
- Centralized configuration management
- NVS storage support
- Configuration validation
- Default value handling

## 🚀 Key Benefits

### **Thread Safety**
- All DMX operations are mutex-protected
- No race conditions between fade engine and command handling
- Safe concurrent access to shared data

### **Error Handling**
- Comprehensive error codes and logging
- Graceful degradation on failures
- Input validation at all levels

### **Maintainability**
- Clear module boundaries
- Self-documenting code structure
- Easy to extend and modify

### **Performance**
- Optimized fade calculations
- Efficient memory usage
- Minimal blocking operations

## 📊 API Examples

### DMX Manager
```c
// Initialize DMX system
esp_err_t err = dmx_manager_init(17, 16, 21);

// Set single channel with fade
dmx_set_channel(1, 255, 1000);  // Channel 1 to 255 over 1 second

// Set RGB with fade
dmx_set_rgb(10, 255, 128, 64, 500);  // RGB on channels 10-12

// Set tunable white
dmx_set_tunable_white(5, 200, 100, 0);  // Immediate change
```

### UDP Protocol
```c
// Parse and execute command
dmx_command_result_t result = udp_handle_raw_command("DMXR1#255128064#255");

// Check result
if (result == DMX_CMD_SUCCESS) {
    ESP_LOGI(TAG, "Command executed successfully");
}
```

### UDP Server
```c
// Initialize and start server
udp_server_init(6454);
udp_server_start();

// Get statistics
udp_server_stats_t stats = udp_server_get_stats();
ESP_LOGI(TAG, "Packets processed: %d", stats.packets_processed);
```

## 🔄 Migration from Old Code

The refactored code maintains **100% functional compatibility** with the original implementation:

- ✅ All UDP commands work identically
- ✅ Loxone integration unchanged  
- ✅ DMX output behavior identical
- ✅ Configuration format compatible
- ✅ Network protocol unchanged

### What Changed:
- **Internal structure**: Better organized modules
- **Thread safety**: Added mutex protection
- **Error handling**: Comprehensive error codes
- **Validation**: Input bounds checking
- **Logging**: More detailed debug information

### What Stayed the Same:
- **UDP Protocol**: Identical command format
- **DMX Behavior**: Same fade algorithms and timing
- **Network**: Same port and packet handling
- **Configuration**: JSON format unchanged

## 🧪 Testing

The refactored code can be tested using the same methods:

```bash
# RGB Test
echo "DMXR1#255128064#255" | nc -u [ESP32_IP] 6454

# Tunable White Test  
echo "DMXW1#200100#255" | nc -u [ESP32_IP] 6454

# Light Command Test
echo "DMXL1#200504000#255" | nc -u [ESP32_IP] 6454
```

The existing `testudp.ipynb` Jupyter Notebook will work without any modifications.

## 📈 Performance Improvements

- **Memory**: More efficient memory usage
- **CPU**: Optimized critical paths
- **Network**: Better UDP packet handling
- **DMX**: Precise fade timing

## 🔮 Future Extensions

The new architecture makes it easy to add:

- **MQTT Support**: New protocol module
- **Web Interface**: Enhanced REST API
- **DMX Input**: RDM support
- **Monitoring**: Advanced statistics
- **OTA Updates**: Firmware update capability

This refactored architecture provides a solid foundation for future development while maintaining complete backward compatibility.
