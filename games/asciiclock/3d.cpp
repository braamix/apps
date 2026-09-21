#include "clock.h"
#include "math/math.h"

float izx = 100;
float izy = 100;

static constexpr float rad = 3.1415926535f / 180.0f;

s_2dcoord c3dto2d(s_3dcoord c3d)
{
    float zzx        = c3d.z / izx;
    float zzy        = c3d.z / izy;
    s_2dcoord coords = { 0, 0 };
    coords.x         = int(round(c3d.x + (zzx * c3d.x)));
    coords.y         = int(round(c3d.y + (zzy * c3d.y)) / SCREEN_CHAR_ASPECT_RATIO);
    return coords;
}

s_3dcoord c3drotate(int axis, int angle, s_3dcoord c3d)
{
    s_3dcoord coords = { 0, 0, 0 };
    switch (axis) {
    case 1:
        coords.x = c3d.x;
        coords.y = int(round(c3d.y * cos(angle * rad) + c3d.z * sin(angle * rad)));
        coords.z = int(round(-c3d.y * sin(angle * rad) + c3d.z * cos(angle * rad)));
        break;
    case 2:
        coords.x = int(round(-c3d.z * sin(angle * rad) + c3d.x * cos(angle * rad)));
        coords.y = c3d.y;
        coords.z = int(round(c3d.z * cos(angle * rad) + c3d.x * sin(angle * rad)));
        break;
    case 3:
        coords.x = int(round(-c3d.x * sin(angle * rad) + c3d.y * cos(angle * rad)));
        coords.y = int(round(c3d.x * cos(angle * rad) + c3d.y * sin(angle * rad)));
        coords.z = c3d.z;
        break;
    }
    return coords;
}
