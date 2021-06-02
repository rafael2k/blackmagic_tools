#include "DeckLinkAPI.h"
#include "aux_functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



void check_result(HRESULT result, const char *msg){

    if (result != S_OK){
	fprintf(stderr, "%s\n", msg);
	exit(EXIT_FAILURE);
    }
    
}

void	print_attributes (IDeckLink* deckLink)
{
	IDeckLinkAttributes*				deckLinkAttributes = NULL;
	bool								supported;
	int64_t								count;
	char *								serialPortName = NULL;
	HRESULT								result;
	
	// Query the DeckLink for its attributes interface
	result = deckLink->QueryInterface(IID_IDeckLinkAttributes, (void**)&deckLinkAttributes);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the IDeckLinkAttributes interface - result = %08x\n", result);
		goto bail;
	}
	
	// List attributes and their value
	printf("Attribute list:\n");
	
	result = deckLinkAttributes->GetFlag(BMDDeckLinkHasSerialPort, &supported);
	if (result == S_OK)
	{
		printf(" %-40s %s\n", "Serial port present ?", (supported == true) ? "Yes" : "No");
		
		result = deckLinkAttributes->GetString(BMDDeckLinkSerialPortDeviceName, (const char **) &serialPortName);
		if (result == S_OK)
		{
			printf(" %-40s %s\n", "Serial port name: ", serialPortName);
			free(serialPortName);

		}
		else
		{
			fprintf(stderr, "Could not query the serial port presence attribute- result = %08x\n", result);
		}
		
	}
	else
	{
		fprintf(stderr, "Could not query the serial port presence attribute- result = %08x\n", result);
	}
	
	result = deckLinkAttributes->GetInt(BMDDeckLinkMaximumAudioChannels, &count);
	if (result == S_OK)
	{
		printf(" %-40s %lld\n", "Number of audio channels:",  count);
	}
	else
	{
		fprintf(stderr, "Could not query the number of supported audio channels attribute- result = %08x\n", result);
	}
	
	result = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsInputFormatDetection, &supported);
	if (result == S_OK)
	{
		printf(" %-40s %s\n", "Input mode detection supported ?", (supported == true) ? "Yes" : "No");
	}
	else
	{
		fprintf(stderr, "Could not query the input mode detection attribute- result = %08x\n", result);
	}
	
	result = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsInternalKeying, &supported);
	if (result == S_OK)
	{
		printf(" %-40s %s\n", "Internal keying supported ?", (supported == true) ? "Yes" : "No");
	}
	else
	{
		fprintf(stderr, "Could not query the internal keying attribute- result = %08x\n", result);
	}
	
	result = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsExternalKeying, &supported);
	if (result == S_OK)
	{
		printf(" %-40s %s\n", "External keying supported ?", (supported == true) ? "Yes" : "No");
	}
	else
	{
		fprintf(stderr, "Could not query the external keying attribute- result = %08x\n", result);
	}
	
	result = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsHDKeying, &supported);
	if (result == S_OK)
	{
		printf(" %-40s %s\n", "HD-mode keying supported ?", (supported == true) ? "Yes" : "No");
	}
	else
	{
		fprintf(stderr, "Could not query the HD-mode keying attribute- result = %08x\n", result);
	}
	
bail:
	printf("\n");
	if(deckLinkAttributes != NULL)
		deckLinkAttributes->Release();
	
}

void	print_output_modes (IDeckLink* deckLink)
{
	IDeckLinkOutput*					deckLinkOutput = NULL;
	IDeckLinkDisplayModeIterator*		displayModeIterator = NULL;
	IDeckLinkDisplayMode*				displayMode = NULL;
	HRESULT								result;	
	
	// Query the DeckLink for its configuration interface
	result = deckLink->QueryInterface(IID_IDeckLinkOutput, (void**)&deckLinkOutput);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the IDeckLinkOutput interface - result = %08x\n", result);
		goto bail;
	}
	
	// Obtain an IDeckLinkDisplayModeIterator to enumerate the display modes supported on output
	result = deckLinkOutput->GetDisplayModeIterator(&displayModeIterator);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the video output display mode iterator - result = %08x\n", result);
		goto bail;
	}
	
	// List all supported output display modes
	printf("Supported video output display modes:\n");
	while (displayModeIterator->Next(&displayMode) == S_OK)
	{
		char *			displayModeString = NULL;
		
		result = displayMode->GetName((const char **) &displayModeString);
		if (result == S_OK)
		{
			char			modeName[64];
			int				modeWidth;
			int				modeHeight;
			BMDTimeValue	frameRateDuration;
			BMDTimeScale	frameRateScale;
			
			
			// Obtain the display mode's properties
			modeWidth = displayMode->GetWidth();
			modeHeight = displayMode->GetHeight();
			displayMode->GetFrameRate(&frameRateDuration, &frameRateScale);
			printf(" %-20s \t %d x %d \t %g FPS\n", displayModeString, modeWidth, modeHeight, (double)frameRateScale / (double)frameRateDuration);
			free(displayModeString);
		}
		
		// Release the IDeckLinkDisplayMode object to prevent a leak
		displayMode->Release();
	}
	
	printf("\n");
	
bail:
	// Ensure that the interfaces we obtained are released to prevent a memory leak
	if (displayModeIterator != NULL)
		displayModeIterator->Release();
	
	if (deckLinkOutput != NULL)
		deckLinkOutput->Release();
}


void	print_capabilities (IDeckLink* deckLink)
{
	IDeckLinkConfiguration*		deckLinkConfiguration = NULL;
	IDeckLinkConfiguration*		deckLinkValidator = NULL;
	int							itemCount;
	HRESULT						result;	
	
	// Query the DeckLink for its configuration interface
	result = deckLink->QueryInterface(IID_IDeckLinkConfiguration, (void**)&deckLinkConfiguration);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the IDeckLinkConfiguration interface - result = %08x\n", result);
		goto bail;
	}
	
	// Obtain a validator object from the IDeckLinkConfiguration interface.
	// The validator object implements IDeckLinkConfiguration, however, all configuration changes are ignored
	// and will not take effect.  However, you can use the returned result code from the validator object
	// to determine whether the card supports a particular configuration.
	
	result = deckLinkConfiguration->GetConfigurationValidator(&deckLinkValidator);
	if (result != S_OK)
	{
		fprintf(stderr, "Could not obtain the configuration validator interface - result = %08x\n", result);
		goto bail;
	}
	
	// Use the validator object to determine which video output connections are available
	printf("Supported video output connections:\n  ");
	itemCount = 0;
	if (deckLinkValidator->SetVideoOutputFormat(bmdVideoConnectionSDI) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("SDI");
	}
	if (deckLinkValidator->SetVideoOutputFormat(bmdVideoConnectionHDMI) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("HDMI");
	}
	if (deckLinkValidator->SetVideoOutputFormat(bmdVideoConnectionComponent) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("Component");
	}
	if (deckLinkValidator->SetVideoOutputFormat(bmdVideoConnectionComposite) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("Composite");
	}
	if (deckLinkValidator->SetVideoOutputFormat(bmdVideoConnectionSVideo) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("S-Video");
	}
	if (deckLinkValidator->SetVideoOutputFormat(bmdVideoConnectionOpticalSDI) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("Optical SDI");
	}
	
	printf("\n\n");
	
	// Use the validator object to determine which video input connections are available
	printf("Supported video input connections:\n  ");
	itemCount = 0;
	if (deckLinkValidator->SetVideoInputFormat(bmdVideoConnectionSDI) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("SDI");
	}
	if (deckLinkValidator->SetVideoInputFormat(bmdVideoConnectionHDMI) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("HDMI");
	}
	if (deckLinkValidator->SetVideoInputFormat(bmdVideoConnectionComponent) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("Component");
	}
	if (deckLinkValidator->SetVideoInputFormat(bmdVideoConnectionComposite) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("Composite");
	}
	if (deckLinkValidator->SetVideoInputFormat(bmdVideoConnectionSVideo) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("S-Video");
	}
	if (deckLinkValidator->SetVideoInputFormat(bmdVideoConnectionOpticalSDI) == S_OK)
	{
		if (itemCount++ > 0)
			printf(", ");
		printf("Optical SDI");
	}
	
	printf("\n");
	
bail:
	if (deckLinkValidator != NULL)
		deckLinkValidator->Release();
	
	if (deckLinkConfiguration != NULL)
		deckLinkConfiguration->Release();
}

// Deserves a SSE or MMX version....
void packed_to_planar (void* PackedFrame, void* PlanarFrame, int num_bytes,
                    int width, int height)
{
#ifdef VERBOSE
    struct timeb tp1;
    struct timeb tp2;;
    ftime (&tp1);
#endif

    // our pAcked offset
    int pa = 0;

    // our pLanar offsets; need two because we're going to move through the
    // Y data twice as fast as the Cb and Cr data
    int pl = 0;
    int pl2 = 0;

    int offset_cb = width * height;
    int offset_cr = offset_cb * 1.5;

    for (int pa = 0; pa < num_bytes; pa += 4)
    {
	// Y'
	memcpy (((char*)PlanarFrame + pl2), ((char*)PackedFrame + pa + 1), 1);
	memcpy (((char*)PlanarFrame + pl2 + 1), ((char*)PackedFrame + pa + 3), 1);
	
	pl2 += 2;
	
	// Cb
	memcpy (((char*)PlanarFrame + offset_cb + pl), ((char*)PackedFrame + pa), 1);
	// Cr
	memcpy (((char*)PlanarFrame + offset_cr + pl), ((char*)PackedFrame + pa + 2), 1);

	pl++;
    }
    

#ifdef VERBOSE
    ftime (&tp2);
    int elapsed_time;
    elapsed_time = (tp2.time - tp1.time) * 1000 + (tp2.millitm - tp1.millitm);

    if (g_opt_verbosity > 2)
    {
        std::cout << "Packed-to-planar conversion time:\n";
        std::cout << "Start time:   " << tp1.time << "." << tp1.millitm << "\n";
        std::cout << "End time:     " << tp2.time << "." << tp2.millitm << "\n";
        std::cout << "Elapsed time: " << elapsed_time << " ms\n";
    }
#endif
    
}
