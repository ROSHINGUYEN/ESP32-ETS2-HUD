/**
 * Globals.cpp — Singleton factory
 */
#include "DisplayTask.h"
#include "IrGesture.h"
#include "WebApi.h"
#include "Globals.h"

static DisplayTask* sDisplay = nullptr;
static IrGesture*  sIr      = nullptr;
static WebApi*     sWeb     = nullptr;

void globalsInit() {
  if (!sDisplay) sDisplay = new DisplayTask();
  if (!sIr)      sIr      = new IrGesture();
  if (!sWeb)     sWeb     = new WebApi();
}

DisplayTask* globalsGetDisplay() { return sDisplay; }
IrGesture*   globalsGetIr()      { return sIr; }
WebApi*      globalsGetWeb()     { return sWeb; }
