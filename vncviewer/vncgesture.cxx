#include "vncgesture.h"

QVNCGesture::QVNCGesture()
 : gestureType(Qt::CustomGesture)
 , state(Qt::NoGesture)
 , isHotSpotSet(false)
 , gestureCancelPolicy(0)
 , isNewSequence(true)
{
}

QVNCGesture::~QVNCGesture()
{
}
