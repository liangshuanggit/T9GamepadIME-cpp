import winreg

paths = [
    r"HKLM\SOFTWARE\ASUS\ASUS System Control Interface\AsusOptimization\ASUS Keyboard Hotkeys",
    r"HKCU\SOFTWARE\ASUS\ASUS System Control Interface\AsusOptimization\ASUS Keyboard Hotkeys",
    r"HKLM\SOFTWARE\ASUS\ASUS System Control Interface\AsusOptimization",
    r"HKLM\SOFTWARE\ASUS\ASUS System Control Interface",
]
for p in paths:
    hive_name, _, sub = p.split('\\', 2)
    h = winreg.HKEY_LOCAL_MACHINE if hive_name == 'HKLM' else winreg.HKEY_CURRENT_USER
    try:
        k = winreg.OpenKey(h, sub)
        print("OPEN:", p)
        i = 0
        while True:
            try:
                name, val, typ = winreg.EnumValue(k, i)
                print(f"   {name} = {val!r} (type {typ})")
                i += 1
            except OSError:
                break
        i = 0
        while True:
            try:
                print("   subkey:", winreg.EnumKey(k, i))
                i += 1
            except OSError:
                break
        winreg.CloseKey(k)
    except OSError as e:
        print("NO:", p)
