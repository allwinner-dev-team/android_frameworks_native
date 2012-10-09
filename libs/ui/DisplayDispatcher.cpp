//
// Copyright 2010 The Android Open Source Project
//
// The Display dispatcher.
//
#define LOG_TAG "DisplayDispatcher"

//#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <powermanager/PowerManager.h>
#include <ui/DisplaySemaphore.h>
#include <ui/DisplayDispatcher.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <cutils/properties.h>

#define INDENT " "
#define INDENT2 " "

#define  MAX_FRAMEID 2147483640

namespace android {
void DisplayDispatcherThread::enqueuebuf(int frameidx) {
	int i;
	int tmp;

	tmp = mFrameidx[frameidx];
	//ALOGD("frameidx = %d\n",frameidx);
	//ALOGD("mFrameidx0[0] = %d\n",mFrameidx[0]);
	//ALOGD("mFrameidx0[1] = %d\n",mFrameidx[1]);
	//ALOGD("mFrameidx0[2] = %d\n",mFrameidx[2]);
	for (i = frameidx; i > 0; i--) {
		mFrameidx[i] = mFrameidx[i - 1];
	}

	mFrameidx[0] = tmp;

	//ALOGD("mFrameidx1[0] = %d\n",mFrameidx[0]);
	//ALOGD("mFrameidx1[1] = %d\n",mFrameidx[1]);
	//ALOGD("mFrameidx1[2] = %d\n",mFrameidx[2]);
}

void DisplayDispatcherThread::setSrcBuf(int srcfb_id, int srcfb_offset) {
	mSrcfbid = srcfb_id;
	mSrcfboffset = srcfb_offset;
}

void DisplayDispatcherThread::signalEvent() {
	//ALOGD("signalEvent!\n");
	mSemaphore->up();
}

void DisplayDispatcherThread::waitForEvent() {
	//ALOGD("waitForEvent!\n");
	mSemaphore->down();
}

void DisplayDispatcherThread::resetEvent() {
	//ALOGD("waitForEvent!\n");
	mSemaphore->reset();
}

// --- InputDispatcherThread ---
void DisplayDispatcherThread::LooperOnce() {
	int writebufid;
	int showbufid;
	int ret;
	int write_index;

	//ALOGD("before waitForEvent!\n");
	waitForEvent();
	//ALOGD("after waitForEvent!\n");

	mDispDevice->request_modelock(mDispDevice);

	if (mFrameidx[DISPLAYDISPATCH_MAXBUFNO - 1] == mFbOffset) {
		writebufid = mFrameidx[DISPLAYDISPATCH_MAXBUFNO - 2];

		write_index = DISPLAYDISPATCH_MAXBUFNO - 2;
	} else {
		writebufid = mFrameidx[DISPLAYDISPATCH_MAXBUFNO - 1];

		write_index = DISPLAYDISPATCH_MAXBUFNO - 1;
	}

	//ALOGD("writebufid = %d\n",writebufid);

	ret = mDispDevice->copysrcfbtodstfb(mDispDevice, mSrcfbid, 1 - mSrcfboffset,
			1 - mSrcfbid, writebufid);
	if (ret != 0) {
		//ALOGE("copy src fb failed!\n");

		mDispDevice->release_modelock(mDispDevice);

		return;
	}

	enqueuebuf(write_index);
	showbufid = mFrameidx[0];
	mFbOffset = showbufid;
	//ALOGD("showbufid = %d,mSrcfbid = %d\n",showbufid,mSrcfbid);
	mDispDevice->pandisplay(mDispDevice, 1 - mSrcfbid, showbufid);
	mDispDevice->release_modelock(mDispDevice);
}

DisplayDispatcherThread::DisplayDispatcherThread(display_device_t* mDevice) :
		Thread(true), mDispDevice(mDevice) {
	for (int i = 0; i < DISPLAYDISPATCH_MAXBUFNO; i++) {
		mFrameidx[i] = i;
	}
	mSemaphore = new DisplaySemaphore(0);
}

DisplayDispatcherThread::~DisplayDispatcherThread() {

}

bool DisplayDispatcherThread::threadLoop() {
	this->LooperOnce();
	return true;
}

DisplayDispatcher::DisplayDispatcher() {
	int err;
	hw_module_t* module;
	status_t result;
	char property[PROPERTY_VALUE_MAX];
	err = hw_get_module(DISPLAY_HARDWARE_MODULE_ID,
			(hw_module_t const**) &module);
	if (err == 0) {
		ALOGD("DisplayDispatcher createing1 err = %d!\n", err);

		err = display_open(module, &mDevice);
		if (err != 0) {
			ALOGE("Open Display Device Failed!\n");
		}
	} else {
		ALOGD("hw_get display module Failed!\n");
	}

	if (property_get("ro.display.switch", property, NULL) > 0) {
		if (atoi(property) == 1) {
			ALOGW("display dispatcher enabled");
			mThread = new DisplayDispatcherThread(mDevice);
			result = mThread->run("DisplayDispatcheR", PRIORITY_HIGHEST);
			if (result) {
				ALOGE("Could not start DisplayDispatcheR thread due to error %d.",result);
				mThread->requestExit();
			}
		} else {
			ALOGW("display dispatcher disable");
		}
	} else {
		ALOGW("display dispatcher disable");
	}
}

DisplayDispatcher::~DisplayDispatcher() {

}

int DisplayDispatcher::changeDisplayMode(int displayno, int value0,int value1) {
	int ret;

	if (mDevice) {
		return mDevice->changemode(mDevice, displayno, value0, value1);
	}

	return -1;
}

int DisplayDispatcher::setDisplayParameter(int displayno, int value0,int value1) {
	if (displayno == 0) {
		if (value0 == DISPLAY_DEVICE_LCD) {
			mDisplayType0 = DISPLAY_DEVICE_LCD;
		} else if (value0 == DISPLAY_DEVICE_HDMI) {
			mDisplayType0 = DISPLAY_DEVICE_HDMI;
			mDisplayFormat0 = value1;
		} else {
			mDisplayPixelFormat0 = value1;
		}
	} else {
		if (value0 == DISPLAY_DEVICE_LCD) {
			mDisplayType1 = DISPLAY_DEVICE_LCD;
		} else if (value0 == DISPLAY_DEVICE_HDMI) {
			mDisplayType1 = DISPLAY_DEVICE_HDMI;
			mDisplayFormat1 = value1;
		} else {
			mDisplayPixelFormat1 = value1;
		}
	}

	return 0;
}

int DisplayDispatcher::setDisplayMode(int mode) {
	if (mDevice) {
		struct display_modepara_t disp_para;

		disp_para.d0type = mDisplayType0;
		disp_para.d1type = mDisplayType1;
		disp_para.d0format = mDisplayFormat0;
		disp_para.d1format = mDisplayFormat1;
		disp_para.d0pixelformat = mDisplayPixelFormat0;
		disp_para.d1pixelformat = mDisplayPixelFormat1;
		disp_para.masterdisplay = mDisplayMaster;

		return mDevice->setdisplaymode(mDevice, mode, &disp_para);
	}

	return -1;
}

int DisplayDispatcher::openDisplay(int displayno) {
	if (mDevice) {
		if (displayno == 0) {
			mDisplayOpen0 = true;
		} else {
			mDisplayOpen1 = true;
		}

		return mDevice->opendisplay(mDevice, displayno);
	}

	return -1;
}

int DisplayDispatcher::closeDisplay(int displayno) {
	if (mDevice) {
		if (displayno == 0) {
			mDisplayOpen0 = false;
		} else {
			mDisplayOpen1 = false;
		}
		return mDevice->closedisplay(mDevice, displayno);
	}

	return -1;
}

int DisplayDispatcher::getHdmiStatus(void) {
	if (mDevice) {
		return mDevice->gethdmistatus(mDevice);
	}

	return -1;
}

int DisplayDispatcher::getDisplayParameter(int displayno, int param) {
	if (mDevice) {
		return mDevice->getdisplayparameter(mDevice, displayno, param);
	}

	return -1;
}

int DisplayDispatcher::setMasterDisplay(int displayno) {
	if (mDevice) {
		return mDevice->setmasterdisplay(mDevice, displayno);
	}

	return -1;
}

int DisplayDispatcher::getMasterDisplay() {
	if (mDevice) {
		return mDevice->getmasterdisplay(mDevice);
	}

	return -1;
}

int DisplayDispatcher::getMaxWidthDisplay() {
	if (mDevice) {
		return mDevice->getmaxwidthdisplay(mDevice);
	}

	return -1;
}

int DisplayDispatcher::getMaxHdmiMode() {
	if (mDevice) {
		return mDevice->gethdmimaxmode(mDevice);
	}

	return -1;
}

int DisplayDispatcher::getDisplayMode() {
	if (mDevice) {
		return mDevice->getdisplaymode(mDevice);
	}

	return -1;
}

int DisplayDispatcher::getDisplayCount() {
	if (mDevice) {
		return mDevice->getdisplaycount(mDevice);
	}

	return -1;
}
int DisplayDispatcher::setDisplayBacklightMode(int mode) {
	if (mDevice) {
		return mDevice->setdisplaybacklightmode(mDevice, mode);
	}

	return -1;
}
int DisplayDispatcher::setDispProp(int cmd, int param0, int param1,int param2) {
	switch (cmd) {
	case DISPLAY_CMD_SETDISPPARA:
		return setDisplayParameter(param0, param1, param2);

	case DISPLAY_CMD_CHANGEDISPMODE:
		return changeDisplayMode(param0, param1, param2);

	case DISPLAY_CMD_CLOSEDISP:
		return closeDisplay(param0);

	case DISPLAY_CMD_OPENDISP:
		return openDisplay(param0);

	case DISPLAY_CMD_GETDISPCOUNT:
		return getDisplayCount();

	case DISPLAY_CMD_GETDISPLAYMODE:
		return getDisplayMode();

	case DISPLAY_CMD_GETDISPPARA:
		return getDisplayParameter(param0, param1);

	case DISPLAY_CMD_GETHDMISTATUS:
		return getHdmiStatus();

	case DISPLAY_CMD_GETMASTERDISP:
		return getMasterDisplay();

	case DISPLAY_CMD_GETMAXHDMIMODE:
		return getMaxHdmiMode();

	case DISPLAY_CMD_GETMAXWIDTHDISP:
		return getMaxWidthDisplay();

	case DISPLAY_CMD_SETMASTERDISP:
		return setMasterDisplay(param0);

	case DISPLAY_CMD_SETDISPMODE:
		return setDisplayMode(param0);

	case DISPLAY_CMD_SETBACKLIGHTMODE:
		return setDisplayBacklightMode(param0);

	default:
		ALOGE("Display Cmd not Support!\n");
		return -1;
	}
}

int DisplayDispatcher::getDispProp(int cmd, int param0, int param1) {
	return 0;
}

void DisplayDispatcher::startSwapBuffer() {
	int master_bufid;
	int master_display;
	int mode;
	int outputtype;
	int plugin;

	mode = mDevice->getdisplaymode(mDevice);
	master_display = mDevice->getmasterdisplay(mDevice);
	outputtype = mDevice->getdisplayparameter(mDevice, 1 - master_display,
			DISPLAY_OUTPUT_TYPE);
	if (outputtype == DISPLAY_DEVICE_HDMI) {
		plugin = mDevice->gethdmistatus(mDevice);
		if (plugin == 0) {
			mThread->resetEvent();

			return;
		}
	}

	if (mode == DISPLAY_MODE_DUALSAME) {
		master_bufid = mDevice->getdisplaybufid(mDevice, master_display);

		mThread->setSrcBuf(master_display, master_bufid);
		mThread->signalEvent();
	}
}

} // namespace android
