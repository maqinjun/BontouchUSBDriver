/*
 *  Created by maqj on 08/01/14.
 *  Copyright (c) 2013 maqj. All rights reserved.
 *
 */

#if 1

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
//#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/hidevent/IOHIDEventService.h>

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>

#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/IOLib.h>

#include <IOKit/hid/IOHIDDevice.h>

#define DEBUG_HEADER            "Bontouch-->"
#define SYSTEM_W64              1
#define ABSOLUTE_TIME_TOP       1080000000.l
#define ABSOLUTE_TIME_BOTTOM    1180000000.l
#define TOUCH_SCORE             5
#define TOUCH_SCREEN_DPI        32768.f
#define DISPLAY_SCREEN_DPI_W    1366.f
#define DISPLAY_SCREEN_DPI_H    768.f

class com_bonxeon_usb_driver_Bontouch : public IOHIDEventService
{
    OSDeclareDefaultStructors(com_bonxeon_usb_driver_Bontouch)
protected:
//    IOWorkLoop                      *myWorkLoop;
//    IOInterruptEventSource          *interruptSource;
//    IOFilterInterruptEventSource    *interruptFilterSource;
//    IOTimerEventSource              *timerSource;
//    IOHIPointing* hidPointing;
    
    IOUSBInterface              *mInterface;
    IOUSBDevice 				*mDevice;
    IOUSBPipe 					*mInterruptPipe;
    IOBufferMemoryDescriptor 	*mReadDataBuffer;
    IOCommandGate 				*mGate;
    
    IOLock *                    mBufferLock;
    IOUSBCompletion				mCompletionRoutine;
//    HIDPreparsedDataRef			mPreparsedReportDescriptorData;
    UInt32					mOutstandingIO;
    UInt16					mMaxPacketSize;
    
    Boolean					mSoundUpIsPressed;
    Boolean					mSoundDownIsPressed;
    
    unsigned				mEventFlags;
    bool					mCapsLockOn;
    bool					mNeedToClose;
    bool                    isRightClick;
    bool                    isSelected;
    
    AbsoluteTime            lastDownT;
    int                     lastX;
    int                     lastY;
    int                     prevX;
    int                     prevY;
    
//    int testnum;
//    IOUSBInterface *inter;
public:
//    virtual void free(void);
//    virtual IOService *probe(IOService *provider, SInt32 *score);
//    virtual IOReturn message(UInt32 type, IOService *provider);
//    virtual void test();
//    virtual bool checkForInterrupt(IOFilterInterruptEventSource * src);
//    virtual void interruptOccurred(IOInterruptEventSource * src, int cnt);
//    virtual void timeoutOccurred(IOTimerEventSource *src);
    virtual bool init(OSDictionary *dictionary = 0);
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);
    virtual IOReturn 	message( UInt32 type, IOService * provider,  void * argument = 0 );
    virtual bool 	willTerminate( IOService * provider, IOOptionBits options );
    virtual bool 	didTerminate( IOService * provider, IOOptionBits options, bool * defer );
private:
    void	DecrementOutstandingIO(void);
    void	IncrementOutstandingIO(void);
    void 	InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining);
    bool    isRightClickHandler(int lx, int ly, uint64_t thisT);
    bool    isSelectedHandler(int currx, int curry);
//    bool    isInSelectScope(int currx, int curry);
    bool    touchScore(int currx, int curry);
protected:
    static void 	InterruptReadHandlerEntry(OSObject* inTarget, void *inParameter, IOReturn inStatus, UInt32 inBufferSizeRemaining );
    static IOReturn	ChangeOutstandingIO(OSObject *owner, void *arg0, void *arg1, void *arg2, void *arg3);

    virtual void dispatchRelativePointerEventX(UInt8 *packet, IOByteCount len);
    
//    virtual void dispatchAbsolutePointerEvent(IOGPoint *	newLoc,
//                                              IOGBounds *	bounds,
//                                              UInt32	buttonState,
//                                              bool	proximity,
//                                              int		pressure,
//                                              int		pressureMin,
//                                              int		pressureMax,
//                                              int		stylusAngle,
//                                              AbsoluteTime	ts);
//    
//    virtual void dispatchScrollWheelEvent(short deltaAxis1,
//                                          short deltaAxis2,
//                                          short deltaAxis3,
//                                          AbsoluteTime ts);
};
#endif
