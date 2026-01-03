#pragma once

class CBaseHandle;

// SAFETY: Base interface for entities that can be referenced via handles
// IMPORTANT: Always check if entity pointers are null before dereferencing
// IMPORTANT: Always validate handle with IsValid() before calling Get()
class IHandleEntity
{
public:
	// OPTIMIZED: noexcept since destructors should not throw exceptions
	virtual ~IHandleEntity() noexcept {}

	// SAFETY: Sets the entity handle reference - implementations must validate handle
	// @param handle - The handle to associate with this entity
	virtual void SetRefEHandle(const CBaseHandle& handle) = 0;

	// SAFETY: Gets the entity handle reference - always returns valid reference
	// OPTIMIZED: const method for read-only access, no side effects
	// @return Reference to this entity's handle (never null, but handle may be invalid)
	virtual const CBaseHandle& GetRefEHandle() const = 0;
};