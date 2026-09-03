// Standalone X11 helper: read the WM's _NET_FRAME_EXTENTS for a window.
// Pure Xlib TU on purpose — Xlib's global Font/Time/Drawable clash with JUCE
// class names, so these headers must never share a TU with JUCE headers.
#if defined(__linux__)

#include <X11/Xlib.h>
#include <X11/Xatom.h>

extern "C" int papaloteGetFrameExtents (unsigned long windowH,
                                        int* outFrameW, int* outFrameH)
{
    if (windowH == 0 || outFrameW == nullptr || outFrameH == nullptr)
        return 0;

    Display* display = XOpenDisplay (nullptr);

    if (display == nullptr)
        return 0;

    int ok = 0;
    const Atom extentsAtom = XInternAtom (display, "_NET_FRAME_EXTENTS", True);

    if (extentsAtom != None)
    {
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long numItems = 0, bytesAfter = 0;
        unsigned char* prop = nullptr;

        if (XGetWindowProperty (display, (Window) windowH, extentsAtom,
                                0, 4, False, XA_CARDINAL,
                                &actualType, &actualFormat,
                                &numItems, &bytesAfter, &prop) == Success
            && prop != nullptr && numItems == 4)
        {
            const long* v = (const long*) prop;
            *outFrameW = (int) (v[0] + v[1]); // left + right
            *outFrameH = (int) (v[2] + v[3]); // top + bottom
            ok = 1;
        }

        if (prop != nullptr)
            XFree (prop);
    }

    XCloseDisplay (display);
    return ok;
}

#else

extern "C" int papaloteGetFrameExtents (unsigned long, int*, int*)
{
    return 0;
}

#endif
