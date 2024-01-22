#!/usr/bin/python3

import math

def cubeCoordToVector(x, y, w, n):
    v = [float(w), float(w), float(w)]
    if n == 0:
        v[1] = float((w - 1) - (y + y))
        v[2] = float((w - 1) - (x + x))
    elif n == 1:
        v[0] = float(-w)
        v[1] = float((w - 1) - (y + y))
        v[2] = float((x + x) - (w - 1))
    elif n == 2:
        v[0] = float((x + x) - (w - 1))
        v[2] = float((y + y) - (w - 1))
    elif n == 3:
        v[0] = float((x + x) - (w - 1))
        v[1] = float(-w)
        v[2] = float((w - 1) - (y + y))
    elif n == 4:
        v[0] = float((x + x) - (w - 1))
        v[1] = float((w - 1) - (y + y))
    else:
        v[0] = float((w - 1) - (x + x))
        v[1] = float((w - 1) - (y + y))
        v[2] = float(-w)
    tmp = math.sqrt((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]))
    v[0] = v[0] / tmp
    v[1] = v[1] / tmp
    v[2] = v[2] / tmp
    return v[0], v[1], v[2]

def vectorToCubeCoord(x, y, z, w):
    if abs(x) > abs(y) and abs(x) > abs(z):
        if x > 0.0:                     # face 0: E
            u = -z / abs(x)
            v = -y / abs(x)
            n = 0
        else:                           # face 1: W
            u = z / abs(x)
            v = -y / abs(x)
            n = 1
    elif abs(y) > abs(x) and abs(y) > abs(z):
        if y > 0.0:                     # face 2: N
            u = x / abs(y)
            v = z / abs(y)
            n = 2
        else:                           # face 3: S
            u = x / abs(y)
            v = -z / abs(y)
            n = 3
    else:
        if z > 0.0:                     # face 4: top
            u = x / abs(z)
            v = -y / abs(z)
            n = 4
        else:                           # face 5: bottom
            u = -x / abs(z)
            v = -y / abs(z)
            n = 5
    return (u + 1.0) * float(w) * 0.5 - 0.5, (v + 1.0) * float(w) * 0.5 - 0.5, n

cubeWrapTable = []

for i in range(54):
    cubeWrapTable += [0]

for n in range(6):
    for yi in range(3):
        for xi in range(3):
            if xi != 0 and yi != 0:
                continue
            x = 64.0
            y = 64.0
            if xi == 1:
                x = 257.0
            elif xi == 2:
                x = -1.5
            elif yi == 1:
                y = 257.0
            elif yi == 2:
                y = -1.5
            vx, vy, vz = cubeCoordToVector(x, y, 256, n)
            xc, yc, nn = vectorToCubeCoord(vx, vy, vz, 256)
            x = math.fmod(x + 256.5, 256.0) - 0.5
            y = math.fmod(y + 256.5, 256.0) - 0.5
            # print("%f, %f, %d -> %f, %f, %d" % (x, y, n, xc, yc, nn))
            if (xc >= 32.0 and xc < 224.0) != (x >= 32.0 and x < 224.0):
                nn = nn | 0x40
                tmp = xc
                xc = yc
                yc = tmp
            if (xc < 128.0) != (x < 128.0):
                nn = nn | 0x10
            if (yc < 128.0) != (y < 128.0):
                nn = nn | 0x20
            cubeWrapTable[(n * 9) + (yi * 3) + xi] = nn | (nn << 8)

for n in range(6):
    for yi in range(3):
        for xi in range(3):
            if xi == 0 or yi == 0:
                continue
            n1 = cubeWrapTable[(n * 9) + xi]
            xi1 = 0
            yi1 = yi
            if (n1 & 0x20) != 0:
                yi1 = 3 - yi1
            if (n1 & 0x40) != 0:
                xi1 = yi1
                yi1 = 0
            n2a = cubeWrapTable[((n1 & 7) * 9) + (yi1 * 3) + xi1] & 0xFF
            if (n1 & 0x40) != 0:
                n2a = (n2a & 0x47) | ((n2a & 0x10) << 1) | ((n2a & 0x20) >> 1)
            n2a = n2a ^ (n1 & 0x70)
            n1 = cubeWrapTable[(n * 9) + (yi * 3)]
            xi1 = xi
            yi1 = 0
            if (n1 & 0x10) != 0:
                xi1 = 3 - xi1
            if (n1 & 0x40) != 0:
                yi1 = xi1
                xi1 = 0
            n2b = cubeWrapTable[((n1 & 7) * 9) + (yi1 * 3) + xi1] & 0xFF
            if (n1 & 0x40) != 0:
                n2b = (n2b & 0x47) | ((n2b & 0x10) << 1) | ((n2b & 0x20) >> 1)
            n2b = n2b ^ (n1 & 0x70)
            if (((n * 9) + (yi * 3) + xi) & 1) == 0:
                n2 = n2a | (n2b << 8)
            else:
                n2 = n2b | (n2a << 8)
            cubeWrapTable[(n * 9) + (yi * 3) + xi] = n2

s = "\nconst unsigned char DDSTexture::cubeWrapTable[108] =\n{\n  "
s += "// index = face * 9 + V_wrap (0: none, 1: +, 2: -) * 3 + U_wrap\n  "
s += "// value = new_face + mirror_U * 0x10 + mirror_V * 0x20 + swap_UV * 0x40"
s += "\n"
for f in range(2):
    if f == 1:
        s += "  // index + 54 = alternate face for corners\n"
    for n in range(6):
        for yi in range(3):
            s += " "
            for xi in range(3):
                nn = (n * 9) + (yi * 3) + xi
                s += " 0x%02X" % ((cubeWrapTable[nn] >> (f << 3)) & 0xFF)
                if not (f == 1 and n == 5 and yi == 2 and xi == 2):
                    s += ","
        s += "\n"
s += "};\n"
print(s)

