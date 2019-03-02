#include "sdkPatch/PalmOSPatched.h"
#include <stdint.h>

#include "debug.h"
#include "globals.h"
#include "traps.h"
#include "palmOsPriv.h"
#include "palmGlobalDefines.h"
#include "specs/emuFeatureRegisterSpec.h"


typedef struct{
   BitmapTypeV3 bitmap;
   uint16_t*    data;
}BitmapTypeV3Indirect;


static Boolean setTungstenWDriverFramebuffer(uint16_t width, uint16_t height){
   if(getGlobalVar(TUNGSTEN_W_DRIVERS_INSTALLED)){
      BitmapTypeV3Indirect* newBitmap;
      uint16_t* newBitmapData;
      WindowType* driverWindow;
      Err error;
      
      driverWindow = WinGetWindowPointer(WinGetDisplayWindow());
      
      newBitmap = MemChunkNew(0, sizeof(BitmapTypeV3Indirect), memNewChunkFlagNonMovable | memNewChunkFlagAllowLarge);
      if(!newBitmap){
         debugLog("Couldnt create new framebuffer bitmap struct!\n");
         return false;
      }
      
      MemSet(newBitmap, sizeof(BitmapTypeV3Indirect), 0x00);
      newBitmap->bitmap.width = width;
      newBitmap->bitmap.height = height;
      newBitmap->bitmap.rowBytes = width * sizeof(uint16_t);
      newBitmap->bitmap.flags.indirect = true;
      newBitmap->bitmap.flags.forScreen = true;
      newBitmap->bitmap.flags.directColor = true;
      newBitmap->bitmap.size = sizeof(BitmapTypeV3);
      newBitmap->bitmap.pixelFormat = pixelFormat565;
      newBitmap->bitmap.compressionType = BitmapCompressionTypeNone;
      newBitmap->bitmap.density = kDensityDouble;

      newBitmap->data = MemChunkNew(0, width * height * sizeof(uint16_t), memNewChunkFlagNonMovable | memNewChunkFlagAllowLarge);
      if(!newBitmap->data){
         debugLog("Couldnt create new framebuffer:%d\n");
         return false;
      }
      
      /*white out framebuffer*/
      MemSet(newBitmap->data, width * height * sizeof(uint16_t), 0xFF);
      
      /*clean up old framebuffer if its not the original one and set new one*/
      driverWindow->bitmapP->flags.forScreen = false;
      BmpDelete(driverWindow->bitmapP);
      
      driverWindow->bitmapP = &newBitmap->bitmap;
      
      /*tell the emu where the framebuffer is*/
      writeArbitraryMemory32(EMU_REG_ADDR(EMU_SRC), (uint32_t)newBitmap->data);
      writeArbitraryMemory32(EMU_REG_ADDR(EMU_VALUE), (uint32_t)width << 16 | height);
      writeArbitraryMemory32(EMU_REG_ADDR(EMU_CMD), CMD_LCD_SET_FB);
      
      return true;
   }
   
   /*driver is not loaded, cant set window*/
   return false;
}

Boolean installTungstenWLcdDrivers(void){
   /*check that all driver files are installed and launch drivers boot entrypoints*/
   if(!getGlobalVar(TUNGSTEN_W_DRIVERS_INSTALLED)){
#if 1
      LocalID database;
      DmOpenRef highDensityDisplay;
      MemHandle highDensityDisplayInitHandle;
      void (*installHighDensityDisplay)(void);
      
      /*Hi Res Blitter.prc, this is the Tungsten W GPU driver, this will crash the device so dont install it*/
      /*PRC ID:bhal hire*/
      /*Resource ID:boot0000*/
      
      /*HighDensityFonts.prc, font pack, only need to check if present*/
      /*PRC ID:data hidd*/
      /*Resource ID:nfnt????*/
      database = DmFindDatabase(0, "HighDensityFonts");
      if(!database){
         /*file does not exist*/
         debugLog("HighDensityFonts.prc not installed!\n");
         return false;
      }
      
      /*HighDensityDisplay.prc, HAL addon, need to jump to exte0000*/
      /*PRC ID:extn hidd*/
      /*Resource ID:????????*/
      database = DmFindDatabase(0, "HighDensityDisplay");
      if(!database){
         /*file does not exist*/
         debugLog("HighDensityDisplay.prc not installed!\n");
         return false;
      }
      
      highDensityDisplay = DmOpenDatabase(0, database, dmModeReadOnly);
      if(!highDensityDisplay){
         /*cant open file*/
         debugLog("Cant open HighDensityDisplay.prc!\n");
         return false;
      }
      
      highDensityDisplayInitHandle = DmGetResource('exte', 0x0000);
      if(!highDensityDisplayInitHandle){
         /*cant open file resource*/
         debugLog("Cant open HighDensityDisplay.prc exte0000!\n");
         return false;
      }
      installHighDensityDisplay = (void(*)(void))MemHandleLock(highDensityDisplayInitHandle);
      
      /*backup original LCD window for SED1376*/
      setGlobalVar(ORIGINAL_FRAMEBUFFER, (uint32_t)WinGetBitmap(WinGetDisplayWindow()));
      
      debugLog("Attempting Tungsten W driver install!\n");
      
      /*need to patch HwrDisplayAttributes before running so driver will install*/
      SysSetTrapAddress(sysTrapHwrDisplayAttributes, emuHwrDisplayAttributes);
      
      /*run HighDensityDisplay.prc exte0000*/
      installHighDensityDisplay();
      debugLog("Tungsten W drivers installed!\n");
      
      /*close everything*/
      MemHandleUnlock(highDensityDisplayInitHandle);
      DmCloseDatabase(highDensityDisplay);
#endif
      
      /*success*/
      setGlobalVar(TUNGSTEN_W_DRIVERS_INSTALLED, true);
      return true;
   }
   
   debugLog("Tungsten W drivers already installed!\n");
   
   /*already installed*/
   return true;
}

Boolean setDeviceResolution(uint16_t width, uint16_t height){
   uint32_t oldResolution = getGlobalVar(CURRENT_RESOLUTION);
   uint32_t newResolution = (uint32_t)width << 16 | height;
   
   debugLog("Setting custom FB size w:%d, h:%d\n", width, height);
   
   if(newResolution != oldResolution){
      if(!setTungstenWDriverFramebuffer(width, height))
         return false;
      
      setGlobalVar(CURRENT_RESOLUTION, newResolution);
   }
   
   return true;
}

void screenSignalApplicationStart(void){
   /*nothing for now*/
}

void screenSignalApplicationExit(void){
   /*put back all defaults, apps must signal they support hires before its enabled and its disabled again when the app is closed*/
   setDeviceResolution(160, 220);
}
