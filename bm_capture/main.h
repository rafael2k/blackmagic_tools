
#ifndef MAIN_H_
#define MAIN_H_

#include "DeckLinkAPI.h"

class BM_Capture : public IDeckLinkInputCallback{
 public:
    BM_Capture();

    HRESULT VideoInputFormatChanged (/* in */ BMDVideoInputFormatChangedEvents notificationEvents, /* in */ IDeckLinkDisplayMode *newDisplayMode, /* in */ BMDDetectedVideoInputFormatFlags detectedSignalFlags);
    HRESULT VideoInputFrameArrived (/* in */ IDeckLinkVideoInputFrame *videoFrame, /* in */ IDeckLinkAudioInputPacket *audioPacket);
    
    // *** DeckLink API implementation of IDeckLinkVideoOutputCallback IDeckLinkAudioOutputCallback *** //
    // IUnknown needs only a dummy implementation
    virtual HRESULT STDMETHODCALLTYPE	QueryInterface (REFIID iid, LPVOID *ppv)	{return E_NOINTERFACE;}
    virtual ULONG STDMETHODCALLTYPE		AddRef ()									{return 1;}
    virtual ULONG STDMETHODCALLTYPE		Release ()									{return 1;}
    
    // local variables
    IDeckLinkIterator *deckLinkIterator;
    IDeckLink *deckLink;
    IDeckLinkInput *deckLinkInput;
    IDeckLinkConfiguration *deckLinkConfiguration;
    FILE *video_output;
    FILE *audio_output;

    // virtual HRESULT STDMETHODCALLTYPE	VideoInputFormatChanged (/* in */ IDeckLinkDisplayMode *newDisplayMode);
    
    // virtual HRESULT STDMETHODCALLTYPE	VideoInputFrameArrived (/* in */ IDeckLinkVideoInputFrame *videoFrame, /* in */ IDeckLinkAudioInputPacket *audioPacket);

    ~BM_Capture ();
};

#endif /* MAIN_H */


