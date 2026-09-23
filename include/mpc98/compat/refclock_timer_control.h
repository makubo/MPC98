#ifndef MPC98_REFCLOCK_TIMER_CONTROL_COMPAT_H
#define MPC98_REFCLOCK_TIMER_CONTROL_COMPAT_H

// IReferenceClockTimerControl was added to DirectShow after the old
// Win2K SDK headers used by this project. The Microsoft BaseClasses source
// already implements it, so provide the Vista-era declaration externally
// instead of patching Microsoft's refclock.h/refclock.cpp files.
#include <strmif.h>

// SAL and COM helper definitions expected by newer DirectShow headers but
// absent from the old Win2K Platform SDK. They must be visible before the
// unmodified BaseClasses streams.h includes amvideo.h.
#ifndef __field_ecount_opt
#define __field_ecount_opt(size)
#endif
#ifndef __range
#define __range(min, max)
#endif
#ifndef __out_range
#define __out_range(min, max)
#endif
#ifndef __deref_out_range
#define __deref_out_range(min, max)
#endif
#ifndef __success
#define __success(expr)
#endif
#ifndef COINIT_DISABLE_OLE1DDE
#define COINIT_DISABLE_OLE1DDE 0x4
#endif
#ifndef SAFE_DIBSIZE
#define SAFE_DIBSIZE(pbmi, pcbImage) \
    (((pbmi) == NULL || (pcbImage) == NULL) ? E_POINTER : \
        (SUCCEEDED(DWordAdd((DWORD)DIBSIZE(*pbmi), 0, (pcbImage))) ? S_OK : E_FAIL))
#endif

#ifndef __IReferenceClockTimerControl_INTERFACE_DEFINED__
#define __IReferenceClockTimerControl_INTERFACE_DEFINED__

// IID from the Vista+ DirectShow strmif.h definition. Selectany keeps this
// compatibility declaration self-contained when old Strmiids.lib lacks it.
EXTERN_C __declspec(selectany) const IID IID_IReferenceClockTimerControl =
{ 0xebec459c, 0x2eca, 0x4d42, { 0xa8, 0xaf, 0x30, 0xdf, 0x55, 0x76, 0x14, 0xb8 } };

MIDL_INTERFACE("ebec459c-2eca-4d42-a8af-30df557614b8")
IReferenceClockTimerControl : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetDefaultTimerResolution(
        REFERENCE_TIME timerResolution) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetDefaultTimerResolution(
        REFERENCE_TIME* pTimerResolution) = 0;
};

#endif

#endif
