
#ifndef MAIN_H_
#define MAIN_H_

#include "DeckLinkAPI.h"

#define SDI_CHANNELS 8

class BM_Play : public IDeckLinkVideoOutputCallback, public IDeckLinkAudioOutputCallback {
 public:
    BM_Play();

    virtual HRESULT ScheduledFrameCompleted (IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult frame_result);
    virtual HRESULT ScheduledPlaybackHasStopped ();
    virtual HRESULT RenderAudioSamples (bool preroll);


    // IUnknown needs only a dummy implementation
    virtual HRESULT		QueryInterface (REFIID iid, LPVOID *ppv)	{return E_NOINTERFACE;}
    virtual ULONG		AddRef ()									{return 1;}
    virtual ULONG		Release ()									{return 1;}
    

    // local variables
    IDeckLinkIterator *deckLinkIterator;
    IDeckLink *deckLink;
    IDeckLinkOutput *deckLinkOutput;
    IDeckLinkConfiguration *deckLinkConfiguration;
    IDeckLinkDisplayModeIterator *displayModeIterator;
    IDeckLinkDisplayMode *displayMode;


    // video stuff
    uint32_t frameWidth;
    uint32_t frameHeight;
    BMDTimeValue frameDuration;
    BMDTimeScale frameTimescale;
    uint32_t framesPerSecond;
    IDeckLinkMutableVideoFrame *VideoFrame;
    uint32_t totalFramesScheduled;

    // audio atuff
    void *audio_buffer;
    uint32_t audioBufferSampleLength;
    uint32_t audioSamplesPerFrame;
    uint32_t audioChannelCount;
    BMDAudioSampleRate audioSampleRate;
    uint32_t audioSampleDepth;
    uint32_t audioBufferOffset;

    FILE *video_input;
    FILE *audio_input;






    ~BM_Play ();
};

#endif /* MAIN_H */


