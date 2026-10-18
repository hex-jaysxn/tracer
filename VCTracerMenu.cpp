// VCTracerMenu.cpp  -  ALL-IN-ONE AML mod for the VC bullet tracer (HEXED Studio)
// Replaces BOTH libVCTracerMenu.so and libVCTracerSolid.so.
// Still requires libVC_Tracer_Nezuko.so (the renderer, closed binary) next to it.
//
// What it does (only for draws that come from libVC_Tracer_Nezuko.so):
//   * Enabled   : off  -> the tracer draw is skipped
//   * Thickness : scales the ribbon width around its own axis
//   * Color     : optional RGB override (alpha is kept)
//   * Style     : 0 = textured (VC look), 1 = solid line (texture unbound)
//
// Config block (exported float[8], same layout the Lua menu already used):
//   [0] enabled  [1] thickness  [2] use custom color  [3] r  [4] g  [5] b
//   [6] status   bit0 = Transform hook ok, bit1 = Draw hook ok, bit2 = tracer draw seen
//   [7] style    0 textured, 1 solid
//
// NOTE: thickness/color are a re-implementation (the original source wasn't available),
// so the look can differ slightly from the old libVCTracerMenu.so.

#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define EXPORT extern "C" __attribute__((visibility("default")))

// ---- AML mod info (176-byte layout, copied from the old libVCTracerMenu.so) --
struct ModInfo {
    char     id[0x30];
    char     name[0x30];
    char     version[0x18];
    char     author[0x30];
    uint32_t flags;
    uint32_t pad;
};
static_assert(sizeof(ModInfo) == 176, "ModInfo layout must be 176 bytes");

static ModInfo g_info = { "vc_tracer_menu", "VC Tracer Menu", "2.0.0", "HEXED Studio", 1, 0 };

EXPORT ModInfo* modinfo = &g_info;
EXPORT ModInfo* __GetModInfo() { return &g_info; }

// ---- shared config ----------------------------------------------------------
EXPORT volatile float VCTracerCfg[8] = { 1, 1, 0, 1, 1, 1, 0, 0 };
static volatile float* const cfg = VCTracerCfg;
static unsigned g_status = 0;

static void SetStatus(unsigned bit) {
    if (!(g_status & bit)) { g_status |= bit; cfg[6] = (float)g_status; }
}

// ---- RenderWare types -------------------------------------------------------
struct RxVert {                       // RxObjSpace3DVertex, 36 bytes
    float   x, y, z;
    float   nx, ny, nz;
    uint8_t rgba[4];                  // r,g,b,a in memory order
    float   u, v;
};
static_assert(sizeof(RxVert) == 36, "RxObjSpace3DVertex must be 36 bytes");

static const int rwRENDERSTATETEXTURERASTER = 1;
static const unsigned MAX_VERTS = 128;

typedef void* (*Transform_t)(RxVert* verts, unsigned num, void* ltm, unsigned flags);
typedef int   (*RenderIndexed_t)(int prim, uint16_t* idx, int n);
typedef int   (*StateSet_t)(int state, void* value);
typedef int   (*StateGet_t)(int state, void* value);
typedef int   (*DobbyHook_t)(void* addr, void* replace, void** origin);

static Transform_t     g_origTransform = nullptr;
static RenderIndexed_t g_origRender    = nullptr;
static StateSet_t      g_rwSet         = nullptr;
static StateGet_t      g_rwGet         = nullptr;

// ---- is the caller inside libVC_Tracer_Nezuko.so? (small cache) -------------
static bool CallerIsTracer(void* ra) {
    struct E { void* a; bool r; };
    static E cache[8];
    static unsigned next = 0;
    for (unsigned i = 0; i < 8; i++)
        if (cache[i].a == ra) return cache[i].r;

    bool res = false;
    Dl_info di;
    if (dladdr(ra, &di) && di.dli_fname)
        res = strstr(di.dli_fname, "VC_Tracer_Nezuko") != nullptr;
    cache[next].a = ra; cache[next].r = res;
    next = (next + 1) & 7;
    return res;
}

// ---- thickness: scale the ribbon around its own axis ------------------------
static void ScaleThickness(RxVert* v, unsigned n, float s) {
    if (n < 3 || fabsf(s - 1.0f) < 0.001f) return;

    // axis estimate: farthest vertex from v[0]
    unsigned b = 0; float best = -1.0f;
    for (unsigned i = 1; i < n; i++) {
        float dx = v[i].x - v[0].x, dy = v[i].y - v[0].y, dz = v[i].z - v[0].z;
        float d2 = dx*dx + dy*dy + dz*dz;
        if (d2 > best) { best = d2; b = i; }
    }
    if (best < 1e-8f) return;
    float ax = v[b].x - v[0].x, ay = v[b].y - v[0].y, az = v[b].z - v[0].z;

    // split vertices into near / far half along that axis, axis = mean(far) - mean(near)
    float tmin = 1e30f, tmax = -1e30f, t[MAX_VERTS];
    for (unsigned i = 0; i < n; i++) {
        t[i] = (v[i].x - v[0].x)*ax + (v[i].y - v[0].y)*ay + (v[i].z - v[0].z)*az;
        if (t[i] < tmin) tmin = t[i];
        if (t[i] > tmax) tmax = t[i];
    }
    float mid = 0.5f * (tmin + tmax);
    float nx = 0, ny = 0, nz = 0, fx = 0, fy = 0, fz = 0; unsigned nn = 0, nf = 0;
    float cx = 0, cy = 0, cz = 0;
    for (unsigned i = 0; i < n; i++) {
        cx += v[i].x; cy += v[i].y; cz += v[i].z;
        if (t[i] < mid) { nx += v[i].x; ny += v[i].y; nz += v[i].z; nn++; }
        else            { fx += v[i].x; fy += v[i].y; fz += v[i].z; nf++; }
    }
    if (!nn || !nf) return;
    cx /= n; cy /= n; cz /= n;
    float dx = fx/nf - nx/nn, dy = fy/nf - ny/nn, dz = fz/nf - nz/nn;
    float len = __builtin_sqrtf(dx*dx + dy*dy + dz*dz);
    if (len < 1e-5f) return;
    dx /= len; dy /= len; dz /= len;

    for (unsigned i = 0; i < n; i++) {
        float rx = v[i].x - cx, ry = v[i].y - cy, rz = v[i].z - cz;
        float al = rx*dx + ry*dy + rz*dz;
        float px = rx - al*dx, py = ry - al*dy, pz = rz - al*dz;
        v[i].x = cx + al*dx + px*s;
        v[i].y = cy + al*dy + py*s;
        v[i].z = cz + al*dz + pz*s;
    }
}

// ---- hooks ------------------------------------------------------------------
__attribute__((noinline))
static void* Hook_Transform(RxVert* verts, unsigned num, void* ltm, unsigned flags) {
    if (verts && num && num <= MAX_VERTS && CallerIsTracer(__builtin_return_address(0))) {
        SetStatus(4);
        float thick = cfg[1];
        bool  col   = cfg[2] >= 0.5f;
        if (cfg[0] >= 0.5f && (col || fabsf(thick - 1.0f) >= 0.001f)) {
            RxVert tmp[MAX_VERTS];                 // work on a copy: never compounds
            memcpy(tmp, verts, num * sizeof(RxVert));
            ScaleThickness(tmp, num, thick);
            if (col) {
                uint8_t r = (uint8_t)(cfg[3] < 0 ? 0 : cfg[3] > 1 ? 255 : cfg[3] * 255.0f);
                uint8_t g = (uint8_t)(cfg[4] < 0 ? 0 : cfg[4] > 1 ? 255 : cfg[4] * 255.0f);
                uint8_t b = (uint8_t)(cfg[5] < 0 ? 0 : cfg[5] > 1 ? 255 : cfg[5] * 255.0f);
                for (unsigned i = 0; i < num; i++) {
                    tmp[i].rgba[0] = r; tmp[i].rgba[1] = g; tmp[i].rgba[2] = b;
                }
            }
            return g_origTransform(tmp, num, ltm, flags);
        }
    }
    return g_origTransform(verts, num, ltm, flags);
}

__attribute__((noinline))
static int Hook_RenderIndexed(int prim, uint16_t* idx, int n) {
    if (CallerIsTracer(__builtin_return_address(0))) {
        SetStatus(4);
        if (cfg[0] < 0.5f) return 1;               // disabled: skip the draw
        if (cfg[7] >= 0.5f) {                      // solid style: no texture
            void* oldRaster = nullptr;
            g_rwGet(rwRENDERSTATETEXTURERASTER, &oldRaster);
            g_rwSet(rwRENDERSTATETEXTURERASTER, nullptr);
            int r = g_origRender(prim, idx, n);
            g_rwSet(rwRENDERSTATETEXTURERASTER, oldRaster);
            return r;
        }
    }
    return g_origRender(prim, idx, n);
}

// ---- install ----------------------------------------------------------------
static void Install() {
    void* gta = dlopen("libGTASA.so", RTLD_NOW);
    void* dob = dlopen("libdobby.so", RTLD_NOW);
    if (!gta || !dob) return;

    DobbyHook_t dobbyHook = (DobbyHook_t)dlsym(dob, "DobbyHook");
    void* tTransform = dlsym(gta, "_Z15RwIm3DTransformP18RxObjSpace3DVertexjP11RwMatrixTagj");
    void* tRender    = dlsym(gta, "_Z28RwIm3DRenderIndexedPrimitive15RwPrimitiveTypePti");
    g_rwSet = (StateSet_t)dlsym(gta, "_Z16RwRenderStateSet13RwRenderStatePv");
    g_rwGet = (StateGet_t)dlsym(gta, "_Z16RwRenderStateGet13RwRenderStatePv");
    if (!dobbyHook || !tTransform || !tRender || !g_rwSet || !g_rwGet) return;

    if (dobbyHook(tTransform, (void*)Hook_Transform, (void**)&g_origTransform) == 0 && g_origTransform)
        SetStatus(1);
    if (dobbyHook(tRender, (void*)Hook_RenderIndexed, (void**)&g_origRender) == 0 && g_origRender)
        SetStatus(2);
}

EXPORT void OnModLoad() { Install(); }
