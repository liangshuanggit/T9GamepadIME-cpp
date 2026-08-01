# 修复 tests/test_main.cpp 中 ACSE 测试路径的反斜杠数量
# 目标 C++ 源码：
#   fs::create_directories(dir + "\\Profiles");          (1 反斜杠 -> 2 反斜杠)
#   WriteTestFile(dir + "\\Profiles\\_sys_desktop.json") (当前 4 反斜杠 -> 2+Profiles+2)
p = 'tests/test_main.cpp'
data = open(p, 'rb').read()

# 1) create_directories: 当前 1 反斜杠 "\Profiles" -> 2 反斜杠 "\\Profiles"
old1 = b'fs::create_directories(dir + "\\Profiles");'
new1 = b'fs::create_directories(dir + "\\\\Profiles");'
print('fix1 count:', data.count(old1))
data = data.replace(old1, new1)

# 2) profile 路径: 当前 4 反斜杠 + _sys_ -> 2 反斜杠 + Profiles + 2 反斜杠 + _sys_
old2 = b'dir + "\\\\_sys_'
new2 = b'dir + "\\\\Profiles\\\\_sys_'
print('fix2 count:', data.count(old2))
data = data.replace(old2, new2)

open(p, 'wb').write(data)

# 验证
d = open(p, 'rb').read()
i = d.find(b'create_directories')
print('verify create_directories:', d[i:i+42])
i = d.find(b'WriteTestFile(dir + "')
print('verify profile path:', d[i:i+58])
