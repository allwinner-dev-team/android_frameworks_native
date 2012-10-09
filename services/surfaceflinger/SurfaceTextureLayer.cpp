/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <hardware/hwcomposer.h>
#include <utils/Errors.h>

#include "Layer.h"
#include "SurfaceTextureLayer.h"

namespace android {
// ---------------------------------------------------------------------------


SurfaceTextureLayer::SurfaceTextureLayer()
    : BufferQueue(true) {
}

SurfaceTextureLayer::~SurfaceTextureLayer() {
}

status_t SurfaceTextureLayer::connect(int api, QueueBufferOutput* output) {
    status_t err = BufferQueue::connect(api, output);
    if (err == NO_ERROR) {
        switch(api) {
            case NATIVE_WINDOW_API_MEDIA_HW:
            case NATIVE_WINDOW_API_CAMERA_HW:
                usehwcomposer = true;
                break;
            case NATIVE_WINDOW_API_MEDIA:
            case NATIVE_WINDOW_API_CAMERA:
                // Camera preview and videos are rate-limited on the producer
                // side.  If enabled for this build, we use async mode to always
                // show the most recent frame at the cost of requiring an
                // additional buffer.
#ifndef NEVER_DEFAULT_TO_ASYNC_MODE
                err = setSynchronousMode(false);
                break;
#endif
                // fall through to set synchronous mode when not defaulting to
                // async mode.
            default:
                err = setSynchronousMode(true);
                break;
        }
        if (err != NO_ERROR) {
            disconnect(api);
        }
    }
    return err;
}

status_t SurfaceTextureLayer::disconnect(int api) 
{
    status_t err = BufferQueue::disconnect(api);

    switch (api) 
    {
	case NATIVE_WINDOW_API_MEDIA_HW:
        case NATIVE_WINDOW_API_CAMERA_HW:
        {
            sp<Layer> layer(mLayer.promote());
            usehwcomposer = false;
            usehwinit     = false;
            if (layer != NULL) 
            {
                layer->setTextureInfo(0,0,0);
            }
        }
        default:
            break;
    }
    return err;
}

int SurfaceTextureLayer::setParameter(uint32_t cmd,uint32_t value) 
{
    int res = 0;

	BufferQueue::setParameter(cmd,value);
	
    sp<Layer> layer(mLayer.promote());
    if (layer != NULL) 
    {
	    	if(cmd == HWC_LAYER_SETINITPARA)
	    	{
	    		layerinitpara_t  *layer_info;
	    		
	    		layer_info = (layerinitpara_t  *)value;

                if(IsHardwareRenderSupport())
                {
	    		    layer->setTextureInfo(layer_info->w,layer_info->h,layer_info->format);

                    usehwinit = true;
                }
	    	}

            if(usehwinit == true)
            {
            	res = layer->setDisplayParameter(cmd,value);
            }
    }
    
    return res;
}


uint32_t SurfaceTextureLayer::getParameter(uint32_t cmd) 
{
    uint32_t res = 0;
    
    sp<Layer> layer(mLayer.promote());
    if (layer != NULL) 
    {
        res = layer->getDisplayParameter(cmd);
    }
    
    return res;
}
// ---------------------------------------------------------------------------
}; // namespace android
