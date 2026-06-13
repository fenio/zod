#include "event_handler.h"

Event::Event(int event_type_, int event_number_, int player_, char* data_, int size_, bool domemcpy)
{
	player = player_;
	event_type = event_type_;
	event_number = event_number_;
	size = size_;
	
	if(domemcpy)
	{
		data = (char*)malloc(size);
		//only copy when there's an actual source: payload-less events (e.g. a
		//bot's CONNECT_EVENT) pass data_=NULL, size=0, and memcpy from a null
		//pointer is UB even for 0 bytes (caught by UBSan on the Linux CI).
		if(size > 0 && data_)
			memcpy(data, data_, size);
	}
	else
		data = data_;
}

Event::~Event()
{
	free(data);
}



