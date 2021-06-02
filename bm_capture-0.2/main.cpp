#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>

#include "DeckLinkAPI.h"
#include "aux_functions.h"
#include "main.h"

// Comment this out for production...
// #define VERBOSE 1

int main(int argc, char **argv){
    HRESULT result;
    char *deviceNameString = NULL;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    BM_Capture bm_capture;

    if (argc != 3 && argc != 2) {
    usage:
	fprintf(stderr, "Usage:\n bm_capture output.y4m audio.pcm\n");
	fprintf(stderr, "or\n");
	fprintf(stderr, " bm_capture -s\n");
	fprintf(stderr, "to show capture card information.\n");
	return EXIT_SUCCESS;	
    }

    // Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
    bm_capture.deckLinkIterator = CreateDeckLinkIteratorInstance();
    if (bm_capture.deckLinkIterator == NULL){
	fprintf(stderr, "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.\n");
	return EXIT_FAILURE;
    }
    
    // Open get the pointer to the first card
    if ((bm_capture.deckLinkIterator)->Next(&(bm_capture.deckLink)) != S_OK){
	fprintf(stderr, "No Blackmagic Design devices were found.\n");
	return EXIT_FAILURE;
    }
    
    if (!strcmp(argv[1], "-s")){
	
	// *** Print the model name of the DeckLink card
	result = (bm_capture.deckLink)->GetModelName((const char **) &deviceNameString);
	if (result == S_OK){
	    printf("\n\n");
	
	    // char deviceName[64];
	    printf("=============== %s ===============\n\n", deviceNameString);
	    free(deviceNameString);
	    
	    // ** List the attributes supported by the card
	    print_attributes(bm_capture.deckLink);
		
	    // ** List the video output display modes supported by the card
	    print_output_modes(bm_capture.deckLink);
		
	    // ** List the input and output capabilities of the card
	    print_capabilities(bm_capture.deckLink);
		
	    // Release the IDeckLink instance when we've finished with it to prevent leaks
	    bm_capture.deckLink->Release();

	    (bm_capture.deckLinkIterator)->Release();
	}
	
	printf("\n");
	return EXIT_SUCCESS;
    }
    else{
	if (argc != 3)
	    goto usage;
    }

    bm_capture.audio_output = fopen(argv[2], "w");
    bm_capture.video_output = fopen(argv[1], "w");
    
    if (!bm_capture.audio_output || !bm_capture.video_output){
	if (!bm_capture.audio_output)
	    fprintf(stderr, "%s could not be opened.\n", argv[2]);
	if (!bm_capture.video_output)
	    fprintf(stderr, "%s could not be opened.\n", argv[1]);
	return EXIT_FAILURE;
    }

    // Obtain the audio/video input interface (IDeckLinkOutput)
    result = (bm_capture.deckLink)->QueryInterface(IID_IDeckLinkInput, (void**)&(bm_capture.deckLinkInput));
    check_result(result, "Error at QueryInterface/IID_IDeckLinkInput");

    printf("StopStreams() called.\n");
    result = (bm_capture.deckLinkInput)->StopStreams();
    if (result != S_OK){
      fprintf(stderr, "Streaming was not active.\n");
    }
    
    // Obtain the audio/video configuration interface (IDeckLinkConfiguration)
    result = (bm_capture.deckLink)->QueryInterface(IID_IDeckLinkConfiguration, (void**)&(bm_capture.deckLinkConfiguration));
    check_result(result, "Error at QueryInterface/IID_IDeckLinkConfiguration");
    
    // Set video input capture interface
    // here we could use bmdVideoConnectionSDI, bmdVideoConnectionHDMI, bmdVideoConnectionOpticalSDI
    // bmdVideoConnectionComponent, bmdVideoConnectionComposite, bmdVideoConnectionSVideo 
    result = (bm_capture.deckLinkConfiguration)->SetVideoInputFormat(bmdVideoConnectionHDMI);
    // result = (bm_capture.deckLinkConfiguration)->SetVideoInputFormat(bmdVideoConnectionComposite);
    check_result(result, "Error at SetVideoInputFormat");

    // Set audio input capture interface
    // here we could use bmdAudioConnectionEmbedded, bmdAudioConnectionAESEBU, bmdAudioConnectionAnalog
    // result = (bm_capture.deckLinkConfiguration)->SetAudioInputFormat(bmdAudioConnectionAnalog);
    result = (bm_capture.deckLinkConfiguration)->SetAudioInputFormat(bmdAudioConnectionEmbedded);
    check_result(result, "Error at SetAudioInputFormat");

    // here we could use bmdModeNTSC, bmdModeNTSC2398, bmdModePAL, bmdModeHD1080p2398, bmdModeHD1080p24,
    // bmdModeHD1080i50, bmdModeHD1080i5994, bmdModeHD720p50, bmdModeHD720p5994, bmdModeHD720p60
    // HARDCODED VALUES: bmdModeNTSC, bmdFormat8BitYUV
    result = (bm_capture.deckLinkInput)->EnableVideoInput(bmdModeHD1080i5994, bmdFormat8BitYUV, 0);
    // result = (bm_capture.deckLinkInput)->EnableVideoInput(bmdModeNTSC, bmdFormat8BitYUV, 0);
    check_result(result, "Error at EnableVideoInput");
    
    // Audio stuff
    // HARDCODED VALUES: bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2 (channels)
    result = (bm_capture.deckLinkInput)->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2);
    check_result(result, "Error at EnableAudioInput");
    
    result = (bm_capture.deckLinkInput)->SetCallback(&bm_capture);
    check_result(result, "Error at SetCallback");

    result = (bm_capture.deckLinkInput)->StartStreams();
    check_result(result, "Error at StartStreams");
    
    pthread_cond_wait(&cond, &mutex);
    // pthread_cond_wait(&cond, &mutex);

    result = (bm_capture.deckLinkInput)->Release();
    // check_result(result, "Error at Release");

    result = (bm_capture.deckLink)->Release();
    // check_result(result, "Error at Release");

    return 0;
}


BM_Capture::BM_Capture(){
    
}

HRESULT BM_Capture::VideoInputFrameArrived (/* in */ IDeckLinkVideoInputFrame *videoFrame, /* in */ IDeckLinkAudioInputPacket *audioPacket){
    static long long frame_count = 0;
    long audio_sample_count;
    HRESULT result;
    BMDTimeValue frameTime, frameDuration, packetTime;
    int	hours, minutes, seconds, frames;
    void *VideoFrame;
    void *AudioFrame;

    // Audio
    // GetSampleCount Get number of sample frames in packet
    audio_sample_count = audioPacket->GetSampleFrameCount();

#ifdef VERBOSE
    fprintf(stderr, "VideoInputFrameArrived %lld\n", frame_count++);
    
    // GetAudioPacketTime Get corresponding audio timestamp
    audioPacket->GetPacketTime (&packetTime, bmdAudioSampleRate48kHz);
    fprintf(stderr, "audio: %d samples, time %d\n", audio_sample_count, packetTime);

    // GetStreamTime Get corresponding video timestamp
    videoFrame->GetStreamTime(&frameTime, &frameDuration, 600);
 	
    hours = (frameTime / (600 * 60*60));
    minutes = (frameTime / (600 * 60)) % 60;
    seconds = (frameTime / 600) % 60;
    frames = (frameTime / 6) % 100;
    fprintf(stderr, "video frame timestamp: %02d:%02d:%02d:%02d ", hours, minutes, seconds, frames);
    fprintf(stderr, "\t %dx%d\n", videoFrame->GetWidth(), videoFrame->GetHeight() );

#endif

    // Get the video and audio data
    videoFrame->GetBytes(&VideoFrame);
    audioPacket->GetBytes(&AudioFrame);
   
    fwrite(VideoFrame, 1, videoFrame->GetWidth() * videoFrame->GetHeight() * 2, video_output); // 4:2:2 chroma sub-sampling
    fflush(video_output);

    // fwrite(AudioFrame, 1, audio_sample_count*2*2, audio_output); // 2-byte per sample, 2 channels
    fwrite(AudioFrame, 1, audio_sample_count*4, audio_output); // 2-byte per sample, 2 channels
    fflush(audio_output);
    
    return S_OK;

}

HRESULT BM_Capture::VideoInputFormatChanged (/* in */ BMDVideoInputFormatChangedEvents notificationEvents, /* in */ IDeckLinkDisplayMode *newDisplayMode, /* in */ BMDDetectedVideoInputFormatFlags detectedSignalFlags){
    static long long aux = 0;
    fprintf(stderr, "VideoInputFormatChanged %lld\n", aux++);

    return S_OK;
}

BM_Capture::~BM_Capture () {}


