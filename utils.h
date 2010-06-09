#ifndef __UTILS_H__
#define __UTILS_H__

#include <wx/image.h>

inline void ClearToWhite(wxImage *image)
{

	size_t size = image->GetWidth() * image->GetHeight() *3;

	memset(image->GetData(), 255, size);
}


#endif
