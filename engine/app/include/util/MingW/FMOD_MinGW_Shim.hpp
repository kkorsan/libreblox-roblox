#ifndef FMOD_MINGW_SHIM_HPP
#define FMOD_MINGW_SHIM_HPP

#include "fmod.h"

namespace FMOD {
    class System;
    class Sound;
    class ChannelControl;
    class Channel;
    class ChannelGroup;
    class DSP;

    // Global Factory Functions
    inline FMOD_RESULT System_Create(System **system, unsigned int headerversion = FMOD_VERSION) {
        return FMOD_System_Create(reinterpret_cast<FMOD_SYSTEM**>(system), headerversion);
    }

    // Global Memory Stats (Missing in your log)
    inline FMOD_RESULT Memory_GetStats(int *currentalloced, int *maxalloced, bool blocking = true) {
        return FMOD_Memory_GetStats(currentalloced, maxalloced, blocking);
    }

    class System {
    public:
        inline FMOD_RESULT init(int maxchannels, FMOD_INITFLAGS flags, void *extradata) { return FMOD_System_Init((FMOD_SYSTEM*)this, maxchannels, flags, extradata); }
        inline FMOD_RESULT update() { return FMOD_System_Update((FMOD_SYSTEM*)this); }
        inline FMOD_RESULT release() { return FMOD_System_Release((FMOD_SYSTEM*)this); }
        inline FMOD_RESULT getVersion(unsigned int *v, unsigned int *b = 0) { return FMOD_System_GetVersion((FMOD_SYSTEM*)this, v, b); }

        inline FMOD_RESULT createSound(const char *n, FMOD_MODE m, FMOD_CREATESOUNDEXINFO *e, Sound **s) { return FMOD_System_CreateSound((FMOD_SYSTEM*)this, n, m, e, (FMOD_SOUND**)s); }
        inline FMOD_RESULT createStream(const char *n, FMOD_MODE m, FMOD_CREATESOUNDEXINFO *e, Sound **s) { return FMOD_System_CreateStream((FMOD_SYSTEM*)this, n, m, e, (FMOD_SOUND**)s); }
        inline FMOD_RESULT createChannelGroup(const char *n, ChannelGroup **cg) { return FMOD_System_CreateChannelGroup((FMOD_SYSTEM*)this, n, (FMOD_CHANNELGROUP**)cg); }
        inline FMOD_RESULT createDSP(const FMOD_DSP_DESCRIPTION *d, DSP **dsp) { return FMOD_System_CreateDSP((FMOD_SYSTEM*)this, d, (FMOD_DSP**)dsp); }

        inline FMOD_RESULT getMasterChannelGroup(ChannelGroup **cg) { return FMOD_System_GetMasterChannelGroup((FMOD_SYSTEM*)this, (FMOD_CHANNELGROUP**)cg); }
        inline FMOD_RESULT playSound(Sound *s, ChannelGroup *cg, bool p, Channel **c) { return FMOD_System_PlaySound((FMOD_SYSTEM*)this, (FMOD_SOUND*)s, (FMOD_CHANNELGROUP*)cg, p, (FMOD_CHANNEL**)c); }

        inline FMOD_RESULT setSoftwareFormat(int r, FMOD_SPEAKERMODE m, int s) { return FMOD_System_SetSoftwareFormat((FMOD_SYSTEM*)this, r, m, s); }
        inline FMOD_RESULT getSoftwareFormat(int *r, FMOD_SPEAKERMODE *m, int *s) { return FMOD_System_GetSoftwareFormat((FMOD_SYSTEM*)this, r, m, s); }
        inline FMOD_RESULT getCPUUsage(FMOD_CPU_USAGE *u) { return FMOD_System_GetCPUUsage((FMOD_SYSTEM*)this, u); }
        inline FMOD_RESULT getChannelsPlaying(int *c, int *r = 0) { return FMOD_System_GetChannelsPlaying((FMOD_SYSTEM*)this, c, r); }

        inline FMOD_RESULT getNumDrivers(int *n) { return FMOD_System_GetNumDrivers((FMOD_SYSTEM*)this, n); }
        inline FMOD_RESULT setDriver(int d) { return FMOD_System_SetDriver((FMOD_SYSTEM*)this, d); }
        inline FMOD_RESULT getDriver(int *d) { return FMOD_System_GetDriver((FMOD_SYSTEM*)this, d); }
        inline FMOD_RESULT getDriverInfo(int id, char *n, int nl, FMOD_GUID *g, int *sr, FMOD_SPEAKERMODE *sm, int *smc) { return FMOD_System_GetDriverInfo((FMOD_SYSTEM*)this, id, n, nl, g, sr, sm, smc); }

        inline FMOD_RESULT set3DSettings(float d, float df, float r) { return FMOD_System_Set3DSettings((FMOD_SYSTEM*)this, d, df, r); }
        inline FMOD_RESULT set3DListenerAttributes(int l, const FMOD_VECTOR *p, const FMOD_VECTOR *v, const FMOD_VECTOR *f, const FMOD_VECTOR *u) { return FMOD_System_Set3DListenerAttributes((FMOD_SYSTEM*)this, l, p, v, f, u); }
        inline FMOD_RESULT setReverbProperties(int i, const FMOD_REVERB_PROPERTIES *p) { return FMOD_System_SetReverbProperties((FMOD_SYSTEM*)this, i, p); }
    };

    class Sound {
    public:
        inline FMOD_RESULT release() { return FMOD_Sound_Release((FMOD_SOUND*)this); }
        inline FMOD_RESULT getLength(unsigned int *l, unsigned int ty) { return FMOD_Sound_GetLength((FMOD_SOUND*)this, l, ty); }
        inline FMOD_RESULT getDefaults(float *f, int *p) { return FMOD_Sound_GetDefaults((FMOD_SOUND*)this, f, p); }
    };

    // NOTE: In the C API, Channel and ChannelGroup functions are distinct.
    // We map ChannelControl to FMOD_ChannelGroup functions because they share the generic 'Control' signature in the C++ API,
    // and FMOD_ChannelGroup_* functions often accept generic handles in ABI or exist as aliases.
    // If you get runtime errors, you may need to dynamic_cast or check type, but this satisfies the linker.
    class ChannelControl {
    public:
        inline FMOD_RESULT stop() { return FMOD_ChannelGroup_Stop((FMOD_CHANNELGROUP*)this); }
        inline FMOD_RESULT setPaused(bool p) { return FMOD_ChannelGroup_SetPaused((FMOD_CHANNELGROUP*)this, p); }
        inline FMOD_RESULT getPaused(bool *p) { return FMOD_ChannelGroup_GetPaused((FMOD_CHANNELGROUP*)this, (FMOD_BOOL*)p); }
        inline FMOD_RESULT setVolume(float v) { return FMOD_ChannelGroup_SetVolume((FMOD_CHANNELGROUP*)this, v); }
        inline FMOD_RESULT getVolume(float *v) { return FMOD_ChannelGroup_GetVolume((FMOD_CHANNELGROUP*)this, v); }
        inline FMOD_RESULT setMute(bool m) { return FMOD_ChannelGroup_SetMute((FMOD_CHANNELGROUP*)this, m); }
        inline FMOD_RESULT getMute(bool *m) { return FMOD_ChannelGroup_GetMute((FMOD_CHANNELGROUP*)this, (FMOD_BOOL*)m); }
        inline FMOD_RESULT setMode(FMOD_MODE m) { return FMOD_ChannelGroup_SetMode((FMOD_CHANNELGROUP*)this, m); }
        inline FMOD_RESULT getMode(FMOD_MODE *m) { return FMOD_ChannelGroup_GetMode((FMOD_CHANNELGROUP*)this, m); }
        inline FMOD_RESULT isPlaying(bool *ip) { return FMOD_ChannelGroup_IsPlaying((FMOD_CHANNELGROUP*)this, (FMOD_BOOL*)ip); }

        inline FMOD_RESULT setUserData(void *ud) { return FMOD_ChannelGroup_SetUserData((FMOD_CHANNELGROUP*)this, ud); }
        inline FMOD_RESULT getUserData(void **ud) { return FMOD_ChannelGroup_GetUserData((FMOD_CHANNELGROUP*)this, ud); }

        inline FMOD_RESULT set3DAttributes(const FMOD_VECTOR *p, const FMOD_VECTOR *v) { return FMOD_ChannelGroup_Set3DAttributes((FMOD_CHANNELGROUP*)this, p, v); }
        inline FMOD_RESULT set3DMinMaxDistance(float min, float max) { return FMOD_ChannelGroup_Set3DMinMaxDistance((FMOD_CHANNELGROUP*)this, min, max); }

        inline FMOD_RESULT getSystemObject(System **s) { return FMOD_ChannelGroup_GetSystemObject((FMOD_CHANNELGROUP*)this, (FMOD_SYSTEM**)s); }
        inline FMOD_RESULT addDSP(int i, DSP *d) { return FMOD_ChannelGroup_AddDSP((FMOD_CHANNELGROUP*)this, i, (FMOD_DSP*)d); }
        inline FMOD_RESULT setCallback(FMOD_CHANNELCONTROL_CALLBACK c) { return FMOD_ChannelGroup_SetCallback((FMOD_CHANNELGROUP*)this, c); }
    };

    // For the Channel class, we override the methods to use the FMOD_Channel_* equivalents.
    // This effectively shadows the ChannelControl versions when called on a Channel object.
    class Channel : public ChannelControl {
    public:
        inline FMOD_RESULT setPosition(unsigned int p, unsigned int ty) { return FMOD_Channel_SetPosition((FMOD_CHANNEL*)this, p, ty); }
        inline FMOD_RESULT getPosition(unsigned int *p, unsigned int ty) { return FMOD_Channel_GetPosition((FMOD_CHANNEL*)this, p, ty); }
        inline FMOD_RESULT setFrequency(float f) { return FMOD_Channel_SetFrequency((FMOD_CHANNEL*)this, f); }
        inline FMOD_RESULT setLoopCount(int l) { return FMOD_Channel_SetLoopCount((FMOD_CHANNEL*)this, l); }
        inline FMOD_RESULT setPriority(int p) { return FMOD_Channel_SetPriority((FMOD_CHANNEL*)this, p); }
        inline FMOD_RESULT setChannelGroup(ChannelGroup *cg) { return FMOD_Channel_SetChannelGroup((FMOD_CHANNEL*)this, (FMOD_CHANNELGROUP*)cg); }

        // Re-implement these to use the Channel specific C API if the generic one fails
        inline FMOD_RESULT stop() { return FMOD_Channel_Stop((FMOD_CHANNEL*)this); }
        inline FMOD_RESULT setPaused(bool p) { return FMOD_Channel_SetPaused((FMOD_CHANNEL*)this, p); }
        inline FMOD_RESULT getPaused(bool *p) { return FMOD_Channel_GetPaused((FMOD_CHANNEL*)this, (FMOD_BOOL*)p); }
        inline FMOD_RESULT setVolume(float v) { return FMOD_Channel_SetVolume((FMOD_CHANNEL*)this, v); }
        inline FMOD_RESULT getVolume(float *v) { return FMOD_Channel_GetVolume((FMOD_CHANNEL*)this, v); }
        inline FMOD_RESULT setMute(bool m) { return FMOD_Channel_SetMute((FMOD_CHANNEL*)this, m); }
        inline FMOD_RESULT getMute(bool *m) { return FMOD_Channel_GetMute((FMOD_CHANNEL*)this, (FMOD_BOOL*)m); }
        inline FMOD_RESULT setMode(FMOD_MODE m) { return FMOD_Channel_SetMode((FMOD_CHANNEL*)this, m); }
        inline FMOD_RESULT getMode(FMOD_MODE *m) { return FMOD_Channel_GetMode((FMOD_CHANNEL*)this, m); }
        inline FMOD_RESULT isPlaying(bool *ip) { return FMOD_Channel_IsPlaying((FMOD_CHANNEL*)this, (FMOD_BOOL*)ip); }
        inline FMOD_RESULT set3DAttributes(const FMOD_VECTOR *p, const FMOD_VECTOR *v) { return FMOD_Channel_Set3DAttributes((FMOD_CHANNEL*)this, p, v); }
        inline FMOD_RESULT set3DMinMaxDistance(float min, float max) { return FMOD_Channel_Set3DMinMaxDistance((FMOD_CHANNEL*)this, min, max); }
        inline FMOD_RESULT setUserData(void *ud) { return FMOD_Channel_SetUserData((FMOD_CHANNEL*)this, ud); }
        inline FMOD_RESULT getUserData(void **ud) { return FMOD_Channel_GetUserData((FMOD_CHANNEL*)this, ud); }
        inline FMOD_RESULT setCallback(FMOD_CHANNELCONTROL_CALLBACK c) { return FMOD_Channel_SetCallback((FMOD_CHANNEL*)this, c); }
    };

    class ChannelGroup : public ChannelControl {
       // Inherits generic behavior, which maps to FMOD_ChannelGroup_*
    };

    class DSP {
    public:
        inline FMOD_RESULT release() { return FMOD_DSP_Release((FMOD_DSP*)this); }
        inline FMOD_RESULT setBypass(bool b) { return FMOD_DSP_SetBypass((FMOD_DSP*)this, b); }
    };
}

#endif
