#include "DeckLinkAPI.h"

#ifndef AUX_FUNCTIONS_H_
#define AUX_FUNCTIONS_H_

void    check_result(HRESULT result, const char *msg);
void    print_attributes (IDeckLink* deckLink);
void	print_output_modes (IDeckLink* deckLink);
void	print_capabilities (IDeckLink* deckLink);
void    packed_to_planar (void* PackedFrame, void* PlanarFrame, int num_bytes,
			  int width, int height);

#endif /* AUX_FUNCTIONS_H */
