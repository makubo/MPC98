#ifndef __MPC98_INTSAFE_H__
#define __MPC98_INTSAFE_H__

#include <windows.h>

#ifndef INTSAFE_E_ARITHMETIC_OVERFLOW
#define INTSAFE_E_ARITHMETIC_OVERFLOW ((HRESULT)0x80070216L)
#endif

static __inline HRESULT DWordAdd(DWORD dwAugend, DWORD dwAddend, DWORD* pdwResult)
{
    DWORD result;

    if (!pdwResult) {
        return E_INVALIDARG;
    }

    result = dwAugend + dwAddend;
    if (result < dwAugend) {
        *pdwResult = 0;
        return INTSAFE_E_ARITHMETIC_OVERFLOW;
    }

    *pdwResult = result;
    return S_OK;
}

#endif
