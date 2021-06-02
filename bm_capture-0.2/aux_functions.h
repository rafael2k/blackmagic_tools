
#ifndef AUX_FUNCTIONS_H_
#define AUX_FUNCTIONS_H

#include "DeckLinkAPI.h"

void    check_result(HRESULT result, const char *msg);
void    print_attributes (IDeckLink* deckLink);
void	print_output_modes (IDeckLink* deckLink);
void	print_capabilities (IDeckLink* deckLink);

#endif /* AUX_FUNCTIONS_H */
