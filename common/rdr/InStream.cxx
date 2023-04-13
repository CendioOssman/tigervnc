#include <InStream.h>

namespace rdr {

InStream::InStream()
    : restorePoint(NULL)
#ifdef RFB_INSTREAM_CHECK
    ,checkedBytes(0)
#endif
{
}

}
