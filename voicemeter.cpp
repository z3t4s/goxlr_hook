#include "voicemeter.hpp"

#ifndef KEY_WOW64_32KEY
#define KEY_WOW64_32KEY 0x0200
#endif

voicemeter_interface::voicemeter_interface()
{
    this->status = get_voicemeter_dll_path();
    if (!this->status)
        return;

    this->status = load_api_dll();   
}

voicemeter_interface::~voicemeter_interface()
{
    unload_api_dll();
}

bool voicemeter_interface::set_parameter(const char* parameter, float value)
{
    long result = this->ivmr.VBVMR_SetParameterFloat(const_cast<char*>(parameter), value);
    if (result == 0)
        return true;

    else if (result == -1)
        printf("[VBVMR_SetParameterFloat] unknown error\n");
    else if (result == -2)
        printf("[VBVMR_SetParameterFloat] voicemeter disconnected\n");
    else if (result == -3)
        printf("[VBVMR_SetParameterFloat] unknown parameter\n");

    return false;
}

bool voicemeter_interface::set_parameter(const char* parameter)
{
    long result = this->ivmr.VBVMR_SetParameters(const_cast<char*>(parameter));
    if (result == 0)
        return true;

    else  if (result > 0)
        printf("[VBVMR_SetParameters] script error in line %d\n", result);
    else if (result == -1 || result == -3 || result == -4)
        printf("[VBVMR_SetParameters] unknown error\n");
    else if (result == -2)
        printf("[VBVMR_SetParameters] voicemeter disconnected\n");

    return false;
}

bool voicemeter_interface::get_voicemeter_dll_path()
{
    HKEY key;
    LSTATUS result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VB:Voicemeeter {17359A74-1236-5467}", 0, KEY_READ, &key);
    if (result != ERROR_SUCCESS)
    {
        result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VB:Voicemeeter {17359A74-1236-5467}", 0, KEY_READ | KEY_WOW64_32KEY, &key);
        if (result != ERROR_SUCCESS)
        {
            printf("Couldn't locate the UninstallPath for Voicemeter\n");
            return false;
        }
    }

    DWORD key_required_size = 0;
    DWORD key_type_string = REG_SZ;
    result = RegQueryValueExA(key, "UninstallString", 0, &key_type_string, nullptr, &key_required_size);
    if (result != ERROR_SUCCESS)
    {
        printf("Unexpected Error while determining UninstallString key size\n");
        RegCloseKey(key);
        return false;
    }

    // Resize our destination buffer to the now known key size and actually read the data
    //
    this->voicemeter_folder_path.resize(key_required_size);
    result = RegQueryValueExA(key, "UninstallString", 0, &key_type_string, (unsigned char*)&this->voicemeter_folder_path[0], &key_required_size);

    // Close the key at all time, no matter the result of the query call
    //
    RegCloseKey(key);

    // If the query call failed though we need to handle it
    //
    if (result != ERROR_SUCCESS)
    {
        printf("Unexpected Error while reading UninstallString key with size %d\n", key_required_size);
        return false;
    }

    // Clean the path from file extensions
    //  
    this->voicemeter_folder_path = this->voicemeter_folder_path.substr(0, this->voicemeter_folder_path.find_last_of("\\"));

    // Append the 64bit or 32bit DLL extension depending on the current platform
    //
    if (sizeof(uintptr_t) == 8)
    {
        this->voicemeter_folder_path += "\\VoicemeeterRemote64.dll";
    }
    else
    {
        this->voicemeter_folder_path += "\\VoicemeeterRemote.dll";
    }

    return true;
}

bool voicemeter_interface::load_api_dll()
{
    this->api_module = LoadLibraryA(this->voicemeter_folder_path.c_str());
    if (!this->api_module)
    {
        printf("LoadLibrary on %s failed\n", this->voicemeter_folder_path.c_str());
        return false;
    }

    this->ivmr.VBVMR_Login = reinterpret_cast<T_VBVMR_Login>(GetProcAddress(this->api_module, "VBVMR_Login"));
    this->ivmr.VBVMR_Logout = reinterpret_cast<T_VBVMR_Login>(GetProcAddress(this->api_module, "VBVMR_Login"));
    this->ivmr.VBVMR_RunVoicemeeter = reinterpret_cast<T_VBVMR_RunVoicemeeter>(GetProcAddress(this->api_module, "VBVMR_RunVoicemeeter"));
    this->ivmr.VBVMR_GetVoicemeeterType = reinterpret_cast<T_VBVMR_GetVoicemeeterType>(GetProcAddress(this->api_module, "VBVMR_GetVoicemeeterType"));
    this->ivmr.VBVMR_GetVoicemeeterVersion = reinterpret_cast<T_VBVMR_GetVoicemeeterVersion>(GetProcAddress(this->api_module, "VBVMR_GetVoicemeeterVersion"));

    this->ivmr.VBVMR_IsParametersDirty = reinterpret_cast<T_VBVMR_IsParametersDirty>(GetProcAddress(this->api_module, "VBVMR_IsParametersDirty"));
    this->ivmr.VBVMR_SetParameterFloat = reinterpret_cast<T_VBVMR_SetParameterFloat>(GetProcAddress(this->api_module, "VBVMR_SetParameterFloat"));
    this->ivmr.VBVMR_SetParameters = reinterpret_cast<T_VBVMR_SetParameters>(GetProcAddress(this->api_module, "VBVMR_SetParameters"));

    long result = this->ivmr.VBVMR_Login();
    if (result < 0)
    {
        printf("Failed to login at Voicemeter. Error code: %d\n", result);
        return false;
    }

    // Check if Voicemeter is installed but not currently running
    //
    if (result == 1)
    {
        printf("Voicemeter is not yet running. Starting it now...");

        // We cannot query which version is installed, so we try to start Voicemeter, Voicemeter Bana, Voicemeter Potato in that order
        //
        for (int i = 1; i <= 3; i++)
        {
            // If this version is not installed, try the next one
            //
            result = this->ivmr.VBVMR_RunVoicemeeter(i);
            if (result == -1)
                continue;

            break;
        }

        if (result != 0)
        {
            printf("Failed to auto launch Voicemeter. Try to start it manually\n");
            return false;
        }


        // This seems like a race condition waiting to happen, but the SDK example does it like that...
        //
        Sleep(1000);
    }

    // Query the installed Voicemeter type and version
    //
    long type = 0;
    result = this->ivmr.VBVMR_GetVoicemeeterType(&type);
    if (result == 0)
    {
        printf("Detected type \"");

        if (type == 1)
            printf("Voicemeeter\"");
        else if (type == 2)
            printf("Voicemeeter Banana\"");
        else if (type == 3)
            printf("Voicemeeter Potato\"");

        long version;
        this->ivmr.VBVMR_GetVoicemeeterVersion(&version);
        printf(" version: %d.%d.%d.%d\n", (version & 0xFF000000) >> 24, (version & 0x00FF0000) >> 16, (version & 0x0000FF00) >> 8, version & 0x000000FF);
    }

    // Poll all parameters once
    //
    this->ivmr.VBVMR_IsParametersDirty();

    return true;
}

bool voicemeter_interface::unload_api_dll()
{
    if (this->ivmr.VBVMR_Logout)
        this->ivmr.VBVMR_Logout();

    if (this->api_module)
        FreeLibrary(this->api_module);

    return true;
}