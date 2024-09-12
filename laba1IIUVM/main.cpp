#include <windows.h>
#include <iostream>
#include <powrprof.h>
#include <setupapi.h>
#include <batclass.h>
#include <devguid.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "powrprof.lib")

DWORD GetBatteryState()
{
#define GBS_HASBATTERY 0x1
#define GBS_ONBATTERY  0x2
    // Returned value includes GBS_HASBATTERY if the system has a
    // non-UPS battery, and GBS_ONBATTERY if the system is running on
    // a battery.
    //
    // dwResult & GBS_ONBATTERY means we have not yet found AC power.
    // dwResult & GBS_HASBATTERY means we have found a non-UPS battery.

    DWORD dwResult = GBS_ONBATTERY;

    // IOCTL_BATTERY_QUERY_INFORMATION,
    // enumerate the batteries and ask each one for information.

    HDEVINFO hdev =
            SetupDiGetClassDevs(&GUID_DEVCLASS_BATTERY,
                                0,
                                0,
                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (INVALID_HANDLE_VALUE != hdev)
    {
        // Limit search to 100 batteries max
        for (int idev = 0; idev < 100; idev++)
        {
            SP_DEVICE_INTERFACE_DATA did = {0};
            did.cbSize = sizeof(did);

            if (SetupDiEnumDeviceInterfaces(hdev,
                                            0,
                                            &GUID_DEVCLASS_BATTERY,
                                            idev,
                                            &did))
            {
                DWORD cbRequired = 0;

                SetupDiGetDeviceInterfaceDetail(hdev,
                                                &did,
                                                0,
                                                0,
                                                &cbRequired,
                                                0);
                if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
                {
                    PSP_DEVICE_INTERFACE_DETAIL_DATA pdidd =
                            (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR,
                                                                         cbRequired);
                    if (pdidd)
                    {
                        pdidd->cbSize = sizeof(*pdidd);
                        if (SetupDiGetDeviceInterfaceDetail(hdev,
                                                            &did,
                                                            pdidd,
                                                            cbRequired,
                                                            &cbRequired,
                                                            0))
                        {
                            // Enumerated a battery.  Ask it for information.
                            HANDLE hBattery =
                                    CreateFile(pdidd->DevicePath,
                                               GENERIC_READ | GENERIC_WRITE,
                                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                                               NULL,
                                               OPEN_EXISTING,
                                               FILE_ATTRIBUTE_NORMAL,
                                               NULL);
                            if (INVALID_HANDLE_VALUE != hBattery)
                            {
                                // Ask the battery for its tag.
                                BATTERY_QUERY_INFORMATION bqi = {0};

                                DWORD dwWait = 0;
                                DWORD dwOut;

                                if (DeviceIoControl(hBattery,
                                                    IOCTL_BATTERY_QUERY_TAG,
                                                    &dwWait,
                                                    sizeof(dwWait),
                                                    &bqi.BatteryTag,
                                                    sizeof(bqi.BatteryTag),
                                                    &dwOut,
                                                    NULL)
                                    && bqi.BatteryTag)
                                {
                                    // With the tag, you can query the battery info.
                                    BATTERY_INFORMATION bi = {0};
                                    bqi.InformationLevel = BatteryInformation;

                                    if (DeviceIoControl(hBattery,
                                                        IOCTL_BATTERY_QUERY_INFORMATION,
                                                        &bqi,
                                                        sizeof(bqi),
                                                        &bi,
                                                        sizeof(bi),
                                                        &dwOut,
                                                        NULL))
                                    {
                                        // Only non-UPS system batteries count
                                        if (bi.Capabilities & BATTERY_SYSTEM_BATTERY)
                                        {
                                            if (!(bi.Capabilities & BATTERY_IS_SHORT_TERM))
                                            {
                                                dwResult |= GBS_HASBATTERY;
                                            }
                                            char Chemistry[5];
                                            memcpy(Chemistry, bi.Chemistry, 4);
                                            std :: cout << "\nBattery ion: " << Chemistry << "\n";

                                            // Query the battery status.
                                            BATTERY_WAIT_STATUS bws = {0};
                                            bws.BatteryTag = bqi.BatteryTag;

                                            BATTERY_STATUS bs;
                                            if (DeviceIoControl(hBattery,
                                                                IOCTL_BATTERY_QUERY_STATUS,
                                                                &bws,
                                                                sizeof(bws),
                                                                &bs,
                                                                sizeof(bs),
                                                                &dwOut,
                                                                NULL))
                                            {
                                                if (bs.PowerState & BATTERY_POWER_ON_LINE)
                                                {
                                                    dwResult &= ~GBS_ONBATTERY;
                                                }
                                            }
                                        }
                                    }
                                }
                                CloseHandle(hBattery);
                            }
                        }
                        LocalFree(pdidd);
                    }
                }
            }
            else  if (ERROR_NO_MORE_ITEMS == GetLastError())
            {
                break;  // Enumeration failed - perhaps we're out of items
            }
        }
        SetupDiDestroyDeviceInfoList(hdev);
    }

    //  Final cleanup:  If we didn't find a battery, then presume that we
    //  are on AC power.

    if (!(dwResult & GBS_HASBATTERY))
        dwResult &= ~GBS_ONBATTERY;

    return dwResult;
}
void printPowerInfo() {
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        GetBatteryState();

        std::cout << "Battery status: ";

        if (sps.BatteryFlag & 1)
            std::cout << "High ";
        if (sps.BatteryFlag & 2)
            std::cout << "Low ";
        if (sps.BatteryFlag & 4)
            std::cout << "Critical ";
        if (sps.BatteryFlag & 8)
            std::cout << "\nBattery AC status: Charging ";
        if (sps.BatteryFlag & 128)
            std::cout << "No system battery, ";
        std::cout << "\n";

        std::cout << "Battery life percentage: " << static_cast<int>(sps.BatteryLifePercent) << "%\n";
        std::cout << "Power save mode: ";
        if (static_cast<int>(sps.SystemStatusFlag)) {
             std::cout << "On\n";
        } else {
            std::cout << "Off\n";
        }
        int batteryFullLifeTime = 30000;
        int FullHours = batteryFullLifeTime / 3600;
        int FullMinutes = (batteryFullLifeTime % 3600) / 60;
        if (!(sps.BatteryFlag & 8)) {
            int batteryLifeTime = 30000*static_cast<int>(sps.BatteryLifePercent)/100;
            int hours = batteryLifeTime / 3600;
            int minutes = (batteryLifeTime % 3600) / 60;
            std::cout << "Battery life time: " << hours << " hours " << minutes << " minutes\n";
        }
        std::cout << "Battery full life time: " << FullHours << " hours " << FullMinutes << " minutes\n";
    } else {
        std::cout << "Failed to get power status\n";
    }
}

void goToSleep() {
    SetSuspendState(FALSE, FALSE, FALSE);
}

void goToHibernate() {
    SetSuspendState(TRUE, FALSE, FALSE);
}

int main() {
    int choice;
    do {
        std::cout << "1. Print power info\n";
        std::cout << "2. Go to sleep\n";
        std::cout << "3. Go to hibernate\n";
        std::cout << "0. Exit\n";
        std::cout << "Enter your choice: ";
        std::cin >> choice;

        switch (choice) {
            case 1:
                printPowerInfo();
                break;
            case 2:
                goToSleep();
                break;
            case 3:
                goToHibernate();
                break;
            case 0:
                std::cout << "Exiting...\n";
                break;
            default:
                std::cout << "Invalid choice\n";
                break;
        }
    } while (choice != 0);

    return 0;
}