/* add your code here */
#include "Bontouch.h"

// This required macro defines the class's constructors, destructors,
// and several other methods I/O Kit requires.
OSDefineMetaClassAndStructors(com_bonxeon_usb_driver_Bontouch, IOHIDEventService)

// Define the driver's superclass.
#define super IOHIDEventService

//IOReturn com_bonxeon_usb_driver_Bontouch::message(UInt32 type, IOService *provider)
//{
//    IOReturn res = 0;
//    IOLog("%s message\n", DEBUG_HEADER);
//    return res;
//}
bool com_bonxeon_usb_driver_Bontouch::init(OSDictionary *dict)
{
#if 0
    bool result = super::init(dict);
    IOLog("%s Initializing\n", DEBUG_HEADER);

//    PE_enter_debugger("init debuging !!!");

    return result;
#endif
    if (!super::init(dict))  return false;
    
    mSoundUpIsPressed = false;
    mSoundDownIsPressed = false;
    mNeedToClose = false;
    isRightClick = false;
    isSelected = false;
    
    mOutstandingIO = 0;
    lastDownT = 0;
    prevX = 0;
    prevY = 0;
    mBufferLock = IOLockAlloc();
    if (!mBufferLock) {
        return false;
    }
    return true;
}

//void com_bonxeon_usb_driver_Bontouch::free(void)
//{
//    IOLog("%s Freeing\n", DEBUG_HEADER);
//    super::free();
//}

//IOService *com_bonxeon_usb_driver_Bontouch::probe(IOService *provider,
//                                                SInt32 *score)
//{
//    IOService *result = super::probe(provider, score);
//    IOLog("%s Probing\n", DEBUG_HEADER);
//    
////    testnum = 100;
////    IOLog("%s testnum = %d\n", DEBUG_HEADER, testnum);
//    return result;
//}

//====================================================================================================
// ReadHandler
//====================================================================================================

void
com_bonxeon_usb_driver_Bontouch::InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining)
{
    com_bonxeon_usb_driver_Bontouch *	me =
    OSDynamicCast(com_bonxeon_usb_driver_Bontouch, target);
    
    me->InterruptReadHandler(status, bufferSizeRemaining);
    me->DecrementOutstandingIO();
}

//bool com_bonxeon_usb_driver_Bontouch::isInSelectScope(int currx, int curry)
//{
//    bool res = false;
//    int Lx = 0; // lastX<selectX2?lastX:selectX2;
//    int Hx = 0; // lastY<selectY2?lastY:selectY2;
//    int Ly = 0;
//    int Hy = 0;
////    Lx = lastX<selectX2?(Hx=selectX2, lastX):(Hx=lastX, selectX2);
////    Ly = lastY<selectY2?(Hy=selectY2, lastY):(Hy=lastY, selectY2);
//    
//    if ((currx>Lx && currx<Hx) ||
//        (curry>Ly && curry<Hy)) {
//        return true;
//    }
//    return res;
//}

bool com_bonxeon_usb_driver_Bontouch::isSelectedHandler(int currx, int curry)
{
    bool res = false;
    int interx = (currx-lastX);
    int intery = (curry-lastY);
    intery = intery<0?-intery:intery;
    interx = interx<0?-interx:interx;

    if (interx > TOUCH_SCORE||intery > TOUCH_SCORE) {
        return true;
    }
    return res;
}
bool com_bonxeon_usb_driver_Bontouch::touchScore(int currx, int curry)
{
    bool res = false;
    int interx = (currx-lastX);
    int intery = (curry-lastY);
    intery = intery<0?-intery:intery;
    interx = interx<0?-interx:interx;
    
    if ((interx < TOUCH_SCORE && intery < TOUCH_SCORE)) {
        return true;
    }
    return  res;
}
bool com_bonxeon_usb_driver_Bontouch::isRightClickHandler(int lx, int ly, uint64_t thisT)
{
    bool res = false;
    uint64_t intert = (thisT-lastDownT);
    if ( (intert > ABSOLUTE_TIME_TOP
          && intert < ABSOLUTE_TIME_BOTTOM)
          && touchScore(lx, ly) ) {
        return true;
    }

    return res;
}

void com_bonxeon_usb_driver_Bontouch::dispatchRelativePointerEventX(UInt8 *packet, IOByteCount len)
{
//    IOLog("%s%d x = %d, y = %d, touch = %d, buffer = %d\n");
//    static int itest = 0;
    
    float xscale = DISPLAY_SCREEN_DPI_W/TOUCH_SCREEN_DPI;
    float yscale = DISPLAY_SCREEN_DPI_H/TOUCH_SCREEN_DPI;
    int x = 0,
        y = 0,
        buttonState = 0;
    SInt32 dx, dy, npx, npy;
    IOGBounds bounds;
    
    x = ((packet[3] << 8) | packet[2]) << 3;
	y = ((packet[5] << 8) | packet[4]) << 3;
    
    dx= (float)x*xscale;
    dy = (float)y*yscale;
    bounds.minx = 0;
    bounds.maxx = 32768;
    bounds.miny = 0;
    bounds.maxy = 32768;
    
    AbsoluteTime now_abs;
    uint64_t nams = 0;
    clock_get_uptime(&now_abs);
    
    absolutetime_to_nanoseconds(now_abs, &nams);
    
    switch (packet[1]) {
        case 0x81:// Touch down.
            lastDownT = nams;
            lastX = dx;
            lastY = dy;
            isRightClick = false;

        case 0x82:// Touch move.
            buttonState = 1;
            if ((!isRightClick) && isRightClickHandler(dx, dy, nams)) {
                buttonState = 2;
                dispatchAbsolutePointerEvent(now_abs, dx, dy,
                                             &bounds, buttonState,
                                             true, 0, 0, 0);

                isRightClick = true;
                isSelected = false;
                return;
            }
            if (isSelected && touchScore(dx, dy)) {
                buttonState = 0;
            }
            break;
        case 0x83:// Right click.
            buttonState = 2;
            IOLog("%s right click!\n",DEBUG_HEADER);
            break;
        case 0x84:// Touch up.
            buttonState = 0;
            lastDownT = 0;
            lastY = 0;
            lastX = 0;
            isRightClick = false;
            
            if ((!isSelected)&&isSelectedHandler(dx, dy)) {
                isSelected = true;
            }
            if (isSelected/*&&isInSelectScope(dx, dy)*/) {
                    dispatchAbsolutePointerEvent(now_abs, dx, dy,
                                                 &bounds, 1,
                                                 true, 0, 0, 0);
                    dispatchAbsolutePointerEvent(now_abs, dx, dy,
                                                 &bounds, 0,
                                                 true, 0, 0, 0);
                    return;
            }
            break;
        default:
            break;
    }

//    IOLog("%s dispatch pointer %d x = %d, y = %d, touch = %d, absoluteTime: %llu\n",DEBUG_HEADER,itest++, dx, dy, buttonState, nams);
//    dispatchRelativePointerEvent(now_abs, dx, dy, buttonState);
//
    dispatchAbsolutePointerEvent(now_abs, dx, dy,
                                     &bounds, buttonState,
                                     true, 0, 0, 0);

    
//    dispatchTabletPointerEvent(now, 0, x, y, 0, &bounds, buttonState, 1, 0, 1, 0, 0, 1, 0, 0, 0);
}
void
com_bonxeon_usb_driver_Bontouch::InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining)
{
    bool            queueAnother = true;
    IOReturn		err = kIOReturnSuccess;
    UInt8           *recvBuf; //= bufferSizeRemaining;
    IOByteCount     buffLen =0;
    
    IOLockLock(mBufferLock);
    if (mReadDataBuffer) {
        buffLen = mReadDataBuffer->getLength();
        recvBuf = (UInt8 *)mReadDataBuffer->getBytesNoCopy();
    }
//    IOLog("%s buff len: %llu\n",DEBUG_HEADER, buffLen);
    if (buffLen < 8) {
        IOLockUnlock(mBufferLock);
        return;
    }
//    IOLog("%s %d: in read handler!\n", DEBUG_HEADER, itest++);
//    USBLog(5, "%s[%p]::InterruptReadHandler status: 0x%x, bufferSizeRemaining: %d", getName(), this, status, bufferSizeRemaining);
    
    switch ( status )
    {
        case kIOReturnSuccess:
            // We got some good stuff, so jump right to our special
            // button handler.
            
            dispatchRelativePointerEventX(recvBuf, buffLen);
            IODelay(2);

            break;

        case kIOReturnOverrun:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnOverrun error", getName(), this);
            // Not sure what to do with this error.  It means more data
            // came back than the size of a descriptor.  Hmmm.  For now
            // just ignore it and assume the data that did come back is
            // useful.
            // INTENTIONAL FALL THROUGH
            
        case kIOReturnNotResponding:
            USBLog(3, "%s[%p]::InterruptReadHandler kIOReturnNotResponding error", getName(), this);
            if ( isInactive() )
            {
                queueAnother = false;
            }
            // if we are not yet inactive, then we should just try again. if we are really disconnected, then
            // we will eventually be terminated and isInactive will return true
            break;
            
        case kIOReturnAborted:
            // This generally means that we are done, because we were unplugged, but not always
            if (isInactive())
            {
                USBLog(3,"%s[%p]::InterruptReadHandler Read aborted. We are terminating", getName(), this);
                queueAnother = false;
            }
            else
            {
                USBLog(3,"%s[%p]::InterruptReadHandler Read aborted. Don't know why. Trying again", getName(), this);
            }
            break;
            
        default:
            // We should handle other errors more intelligently, but
            // for now just return and assume the error is recoverable.
            USBLog(3, "%s[%p]::InterruptReadHandler error (0x%x) reading interrupt pipe", getName(), this, status);
            break;
    }
    
    if (queueAnother)
    {
        // Reset the buffer - i doubt this is really necessary
        if (mReadDataBuffer) {
            mReadDataBuffer->setLength( mMaxPacketSize );
        }
        // Queue up another one before we leave.
        IncrementOutstandingIO();
    	err = mInterruptPipe->Read( mReadDataBuffer, &mCompletionRoutine );
        if ( err != kIOReturnSuccess)
        {
            // This is bad.  We probably shouldn't continue on from here.
            USBError(1, "%s[%p]::InterruptReadHandler -  immediate error 0x%x queueing read\n", getName(), this, err);
            DecrementOutstandingIO();
//            err = mInterruptPipe->ClearPipeStall(true);
            err = mInterruptPipe->Reset();
            
            if (err != kIOReturnSuccess) {
                IOLog("%s  pipe reset failed!\n", DEBUG_HEADER);
            }
            else{
                IOLog("%s  pipe reset success!\n", DEBUG_HEADER);
            }

        }
    }
    
    IOLockUnlock(mBufferLock);
}




bool com_bonxeon_usb_driver_Bontouch::start(IOService *provider)
{
    
#if 1
    IOReturn			err = kIOReturnSuccess;
    IOWorkLoop			*wl;
    
    IOLog("%s %s[%p]::start - beginning - retain count = %d\n",DEBUG_HEADER, getName(), this, getRetainCount());

    USBLog(3, "%s %s[%p]::start - beginning - retain count = %d",DEBUG_HEADER, getName(), this, getRetainCount());

    mInterface = OSDynamicCast(IOUSBInterface, provider);
    

#if 0
    mDevice = OSDynamicCast(IOUSBDevice, provider);
    if (!mDevice){
        IOLog("%s get IOUSBDevice failed!\n",DEBUG_HEADER);
        return false;
    }
    IOUSBFindInterfaceRequest interfaceRequest;
//    interfaceRequest.bInterfaceClass = 255;
//    interfaceRequest.bAlternateSetting = 0;
//    interfaceRequest.bInterfaceProtocol = 0;
//    interfaceRequest.bInterfaceSubClass = 0;
    
    interfaceRequest.bInterfaceClass = kIOUSBFindInterfaceDontCare;
    interfaceRequest.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
    interfaceRequest.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
    interfaceRequest.bAlternateSetting = kIOUSBFindInterfaceDontCare;

//    mInterface =  mDevice->FindNextInterface(NULL, &interfaceRequest);
    
    OSIterator *intfIterator = mDevice->CreateInterfaceIterator(&interfaceRequest);
    if (!intfIterator->isValid()) {
        IOLog("%s get intfIterator failed!\n",DEBUG_HEADER);
        return false;
    }
    
    
    mInterface = (IOUSBInterface*)intfIterator->getNextObject();
#endif
    if (!mInterface){
        IOLog("%s get IOUSBInterface failed!\n",DEBUG_HEADER);
        return false;
    }
//    mInterface->retain();
    
    if( mInterface->open( this ) == false )
    {
        USBError(1, "%s %s[%p]::start - unable to open provider. returning false",DEBUG_HEADER, getName(), this);
        IOLog("%s %s[%p]::start - unable to open provider. returning false\n",DEBUG_HEADER, getName(), this);

        return false;
    }
    
    do {
        mGate = IOCommandGate::commandGate(this);
        
        if(!mGate)
        {
           IOLog("%s %s[%p]::start - unable to create command gate\n",DEBUG_HEADER, getName(), this);
            break;
        }
        
        wl = getWorkLoop();
        if (!wl)
        {
            IOLog("%s %s[%p]::start - unable to find my workloop\n",DEBUG_HEADER, getName(), this);
            break;
        }
        
        if (wl->addEventSource(mGate) != kIOReturnSuccess)
        {
            IOLog("%s %s[%p]::start - unable to add gate to work loop\n",DEBUG_HEADER, getName(), this);
            break;
        }
        
        IOUSBFindEndpointRequest	endpointRequest;
        
        endpointRequest.type 		= kUSBInterrupt;
        endpointRequest.direction	= kUSBIn;
        
        mInterruptPipe = mInterface->FindNextPipe( NULL, &endpointRequest );
        
        if( !mInterruptPipe )
        {
            IOLog("%s %s[%p]::start - unable to get interrupt pipe\n",DEBUG_HEADER, getName(), this);
            break;
        }
        
        // Check for all the usages we expect to find on this device. If we don't find what we're
        // looking for, we're gonna bail.
        
//        if( VerifyNewDevice() == false )
//        {
//            USBError(1, "%s[%p]::start - VerifyNewDevice was not successful. Ignoring this device", getName(), this);
//            break;
//        }
        
        // Setup the read buffer.
        
        mMaxPacketSize = endpointRequest.maxPacketSize;
        mReadDataBuffer = IOBufferMemoryDescriptor::withCapacity( mMaxPacketSize, kIODirectionIn );
		
        mCompletionRoutine.target = (void *) this;
        mCompletionRoutine.action = (IOUSBCompletionAction)
        com_bonxeon_usb_driver_Bontouch::InterruptReadHandlerEntry;
        mCompletionRoutine.parameter = (void *) 0;  // not used
        
        mReadDataBuffer->setLength( mMaxPacketSize );
        
        // The way to set us up to recieve reads is to call it directly. Each time our read handler
        // is called, we'll have to do another to make sure we get the next read.
        
        IncrementOutstandingIO();
        if( (err = mInterruptPipe->Read( mReadDataBuffer, &mCompletionRoutine )) )
        {
            IOLog("%s %s[%p]::start - err (%x) in interrupt read, retain count %d after release\n",DEBUG_HEADER, getName(), this, err, getRetainCount());
            DecrementOutstandingIO();
            break;
        }
        
        IOLog("%s %s[%p]::start AppleUSBProKeyboard @ %d (0x%lx)\n",DEBUG_HEADER, getName(), this, mInterface->GetDevice()->GetAddress(), strtol(mInterface->GetDevice()->getLocation(), (char **)NULL, 16));
        
        // OK- so this is not totally kosher in the IOKit world. You are supposed to call super::start near the BEGINNING
        // of your own start method. However, the IOHIKeyboard::start method invokes registerService, which we don't want to
        // do if we get any error up to this point. So we wait and call IOHIKeyboard::start here.
        if( !super::start(mInterface))
        {
            IOLog("%s %s[%p]::start - unable to start superclass. returning false\n",DEBUG_HEADER, getName(), this);
            break;	// error
        }
	    
        return true;
        
    } while (false);
	
    IOLog("%s %s[%p]::start aborting.  err = 0x%x\n",DEBUG_HEADER, getName(), this, err);
    
//    if( mPreparsedReportDescriptorData ) 
//        HIDCloseReportDescriptor( mPreparsedReportDescriptorData );
	
    if ( mInterruptPipe )
    {
        mInterruptPipe->Abort();
        mInterruptPipe = NULL;
    }
    
    // stop will clean up everything else
    stop( provider );
    provider->close( this );
	
    return false;
    
#endif
    
    
}

void com_bonxeon_usb_driver_Bontouch::stop(IOService *provider)
{
    super::stop( provider );

    IOLog("%s Stopping\n", DEBUG_HEADER);
//    if( mPreparsedReportDescriptorData )
//        HIDCloseReportDescriptor( mPreparsedReportDescriptorData );
    
    if (mGate)
    {
        mGate->disable();
        IOWorkLoop	*wl = getWorkLoop();
        if (wl)
            wl->removeEventSource(mGate);
        if (mGate) {
            mGate->release();
        }
        mGate = NULL;
    }
    
    if (mReadDataBuffer) {
        IOLockLock(mBufferLock);

        mReadDataBuffer->release();
        mReadDataBuffer = NULL;
        
        IOLockUnlock(mBufferLock);
    }

#if 0
    IOLog("%s testnum = %d\n", DEBUG_HEADER, testnum);
    
    if (interruptSource) {
        interruptSource->disable();
        myWorkLoop->removeEventSource(interruptSource);
        interruptSource->release();
        interruptSource = 0;
    }
    if (interruptFilterSource) {
        interruptFilterSource->disable();
        myWorkLoop->removeEventSource(interruptFilterSource);
        interruptFilterSource->release();
        interruptFilterSource = 0;
    }
    if (timerSource) {
        timerSource->cancelTimeout();
        myWorkLoop->removeEventSource(timerSource);
        timerSource->release();
        timerSource = 0;
    }
    super::stop(provider);
#endif
}
/*
bool com_bonxeon_usb_driver_Bontouch::checkForInterrupt(IOFilterInterruptEventSource * src)
{
    // check if this interrupt belongs to me
    IOLog("%s in check interrupt !!!\n", DEBUG_HEADER);
    testnum += 110;

    return true; // go ahead and invoke completion routine
}

// void (*Action)(OSObject *, IOInterruptEventSource *, int count);
void com_bonxeon_usb_driver_Bontouch::interruptOccurred(IOInterruptEventSource * src, int cnt)
{
    // handle the interrupt
    
    IOLog("%s in interrupt !!!\n", DEBUG_HEADER);
    
    testnum += 100;
}
void com_bonxeon_usb_driver_Bontouch::timeoutOccurred(IOTimerEventSource *src)
{
    IOLog("%s in timeout !!!\n", DEBUG_HEADER);
    testnum += 120;

}
*/

//====================================================================================================
// message
//====================================================================================================

IOReturn com_bonxeon_usb_driver_Bontouch::message( UInt32 type, IOService * provider,  void * argument )
{
    switch ( type )
    {
        case kIOMessageServiceIsTerminated:
            USBLog(6, "%s[%p]::message - kIOMessageServiceIsTerminated - ignoring now", getName(), this);
            break;
            
        case kIOMessageServiceIsSuspended:
        case kIOMessageServiceIsResumed:
        case kIOMessageServiceIsRequestingClose:
        case kIOMessageServiceWasClosed:
        case kIOMessageServiceBusyStateChange:
        default:
            break;
    }
    
    return kIOReturnSuccess;
}



//====================================================================================================
// willTerminate
//====================================================================================================
//
bool
com_bonxeon_usb_driver_Bontouch::willTerminate( IOService * provider, IOOptionBits options )
{
    // this method is intended to be used to stop any pending I/O and to make sure that
    // we have begun getting our callbacks in order. by the time we get here, the
    // isInactive flag is set, so we really are marked as being done. we will do in here
    // what we used to do in the message method (this happens first)
    USBLog(5, "%s[%p]::willTerminate isInactive = %d", getName(), this, isInactive());
    if (mReadDataBuffer)
    {
        mReadDataBuffer->release();
        mReadDataBuffer = NULL;
    }
    if (mInterruptPipe)
    {
        mInterruptPipe->Abort();
        mInterruptPipe = NULL;
    }
    return super::willTerminate(provider, options);
}


//====================================================================================================
// didTerminate
//====================================================================================================
//
bool
com_bonxeon_usb_driver_Bontouch::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    // this method comes at the end of the termination sequence. Hopefully, all of our outstanding IO is complete
    // in which case we can just close our provider and IOKit will take care of the rest. Otherwise, we need to
    // hold on to the device and IOKit will terminate us when we close it later
    USBLog(5, "%s[%p]::didTerminate isInactive = %d, outstandingIO = %d", getName(), this, isInactive(), mOutstandingIO);
    if (!mOutstandingIO)
        mInterface->close(this);
    else
        mNeedToClose = true;
    return super::didTerminate(provider, options, defer);
}

//====================================================================================================
// DecrementOutstandingIO
//====================================================================================================
//
void
com_bonxeon_usb_driver_Bontouch::DecrementOutstandingIO(void)
{
    if (!mGate)
    {

        if (!--mOutstandingIO && mNeedToClose)
        {
            USBLog(3, "%s[%p]::DecrementOutstandingIO isInactive = %d, outstandingIO = %d - closing device", getName(), this, isInactive(), mOutstandingIO);
            IOLog("%s %s[%p]::DecrementOutstandingIO isInactive = %d, outstandingIO = %d - closing device",DEBUG_HEADER, getName(), this, isInactive(), mOutstandingIO);

            mInterface->close(this);
        }
        return;
    }
    mGate->runAction(ChangeOutstandingIO, (void*)-1);
}


//====================================================================================================
// IncrementOutstandingIO
//====================================================================================================
//
void
com_bonxeon_usb_driver_Bontouch::IncrementOutstandingIO(void)
{
    if (!mGate)
    {
        mOutstandingIO++;
        return;
    }
    mGate->runAction(ChangeOutstandingIO, (void*)1);
}


//====================================================================================================
// ChangeOutstandingIO
//====================================================================================================
//
IOReturn
com_bonxeon_usb_driver_Bontouch::ChangeOutstandingIO(OSObject *target, void *param1, void *param2, void *param3, void *param4)
{
    com_bonxeon_usb_driver_Bontouch *me = OSDynamicCast(com_bonxeon_usb_driver_Bontouch, target);
#if SYSTEM_W64
    int64_t	direction = (int64_t)param1;
#elif SYSTEM_W32
    int32_t direction = (int32_t)param1;
#endif
    if (!me)
    {
        USBLog(1, "AppleUSBProKbd::ChangeOutstandingIO - invalid target");
        return kIOReturnSuccess;
    }
    switch (direction)
    {
        case 1:
            me->mOutstandingIO++;
            break;
            
        case -1:
            if (!--me->mOutstandingIO && me->mNeedToClose)
            {
                USBLog(5, "%s[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device", me->getName(), me, me->isInactive(), me->mOutstandingIO);
                
                IOLog("%s %s[%p]::ChangeOutstandingIO isInactive = %d, outstandingIO = %d - closing device",DEBUG_HEADER, me->getName(), me, me->isInactive(), me->mOutstandingIO);

                me->mInterface->close(me);
            }
            break;
            
        default:
            USBLog(1, "%s[%p]::ChangeOutstandingIO - invalid direction", me->getName(), me);
    }
    return kIOReturnSuccess;
}




