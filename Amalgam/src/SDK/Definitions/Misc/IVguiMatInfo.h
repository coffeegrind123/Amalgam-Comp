#pragma once
#include "IVguiMatInfoVar.h"

// Interface for VGUI material information access
// SAFETY: All implementations must handle null parameters gracefully
class IVguiMatInfo
{
public:
	virtual ~IVguiMatInfo() {}

	// SAFETY: Returns nullptr if varName is null or variable not found
	// SAFETY: found parameter may be null - implementations must check before dereferencing
	// SAFETY: Callers MUST check return value for nullptr before dereferencing
	virtual IVguiMatInfoVar* FindVarFactory(const char* varName, bool* found) = 0;

	// Returns the number of animation frames in this material
	virtual int GetNumAnimationFrames() = 0;
};