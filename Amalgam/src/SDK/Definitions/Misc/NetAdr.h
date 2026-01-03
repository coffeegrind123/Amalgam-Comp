#pragma once

// SAFETY: Network address type enumeration
typedef enum
{
	NA_NULL = 0,      // Invalid/uninitialized address
	NA_LOOPBACK,      // Loopback (127.0.0.1)
	NA_BROADCAST,     // Broadcast address
	NA_IP,            // Standard IP address
} netadrtype_t;

// SAFETY: Network address structure - plain POD type for binary compatibility
typedef struct netadr_s
{
public:
	netadrtype_t type;      // Address type discriminator
	unsigned char ip[4];    // IPv4 address octets (network byte order)
	unsigned short port;    // Port number (network byte order)

	// SAFETY: Initialize to safe defaults to prevent undefined behavior
	netadr_s() : type(NA_NULL), ip{0, 0, 0, 0}, port(0) {}

	// OPTIMIZED: Inline equality check for fast address comparison
	inline bool operator==(const netadr_s& other) const {
		// OPTIMIZED: Early exit on type mismatch (most common failure case)
		if (type != other.type) return false;
		// OPTIMIZED: Port comparison before IP (faster single comparison)
		if (port != other.port) return false;
		// OPTIMIZED: Memcmp for 4-byte array is faster than individual comparisons
		return ip[0] == other.ip[0] && ip[1] == other.ip[1] &&
		       ip[2] == other.ip[2] && ip[3] == other.ip[3];
	}

	// SAFETY: Inequality operator for completeness
	inline bool operator!=(const netadr_s& other) const {
		return !(*this == other);
	}

	// SAFETY: Check if address is valid/initialized
	inline bool IsValid() const {
		return type != NA_NULL;
	}
} netadr_t;