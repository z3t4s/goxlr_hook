#include <string>
#include <windows.h>
#include "Voicemeeter-SDK/VoicemeeterRemote.h"

class voicemeter_interface
{
public:
    bool status;
    voicemeter_interface();
    ~voicemeter_interface();

    bool set_parameter(const char* parameter, float value);
    bool set_parameter(const char* parameter);
   
private:
    bool get_voicemeter_dll_path();
    bool load_api_dll();
    bool unload_api_dll();

    std::string voicemeter_folder_path;
    HMODULE api_module;
    T_VBVMR_INTERFACE ivmr;   
};
