/**
 * Globals.h
 */
#pragma once
#include <Arduino.h>

class DisplayTask;
class IrGesture;
class WebApi;

void globalsInit();
DisplayTask* globalsGetDisplay();
IrGesture*   globalsGetIr();
WebApi*      globalsGetWeb();
