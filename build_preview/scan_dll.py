import re

b = open(r"C:/Program Files/ASUS/ARMOURY CRATE SE Service/SystemFeatures/SystemFeatures.dll", 'rb').read()
s16 = b.decode('utf-16-le', errors='ignore')
strs = set(re.findall(r'[\x20-\x7e]{4,}', s16))
kws = ['software', 'asus', 'armoury', '.ini', 'config', 'hotkey',
       'commandcenter', 'localstate', 'programdata', 'appdata',
       'keys', 'systemfeature', 'create']
print("== 相关字符串 ==")
for x in sorted(strs):
    low = x.lower()
    if any(k in low for k in kws):
        print(repr(x))
