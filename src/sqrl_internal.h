/** \file sqrl_internal.h
 *
 * \author Adam Comley
 *
 * This file is part of libsqrl.  It is released under the MIT license.
 * For more details, see the LICENSE file included with this package.
**/
#ifndef SQRL_INTERNAL_H
#define SQRL_INTERNAL_H

#include "config.h"

#include "sqrl.h"
#include <stdio.h>

#ifndef ARDUINO
#define SODIUM_STATIC
#include "sodium.h"
extern "C" {
#include "crypto_scrypt.h"
}
#endif

namespace libsqrl
{
#define SQRL_VERSION_STRING "1"
#define SQRL_KNOWN_VERSIONS_COUNT 1
#define SQRL_CLIENT_VERSIONS {1}
#define SITE_KEY_LOOKUP 0
#define SITE_KEY_SEC 1
#define SITE_KEY_PUB 2
#define SITE_KEY_PSEC 3
#define SITE_KEY_PPUB 4
#define SITE_KEY_SUK 5
#define SITE_KEY_VUK 6
#define SITE_KEY_URSK 7
#define SITE_KEY_URPK 8

	// Site information saved for 5 minutes (600 seconds) past last action
#define SQRL_CLIENT_SITE_TIMEOUT 600

#define FLAG_SET(f,v) f |= v
#define FLAG_CLEAR(f,v) f &= ~(v)
#define FLAG_CHECK(f,v) (v == (f & v))

#define NEXT_STATE(cs) return (cs) + 1;
#define SAME_STATE(cs) return (cs);
#define TO_STATE( s ) return (s);
#define COMPLETE( st ) this->status = (st); \
SqrlClient::getClient()->callActionComplete( this ); \
return SQRL_ACTION_STATE_DELETE;

	struct Sqrl_User_s_callback_data
	{
		SqrlAction *action;
		int adder;
		double multiplier;
		int t1;
		int t2;
		int total;
	};

	double sqrl_get_real_time();
	uint64_t sqrl_get_timestamp();

	extern struct Sqrl_Global_Mutices SQRL_GLOBAL_MUTICES;

	void SqrlInit();
	void sqrl_sleep( int sleepMs );
	bool sqrl_parse_key_value( char **strPtr, char **keyPtr, char **valPtr,
		size_t *key_len, size_t *val_len, char *sep );

	void sqrl_free( void *ptr, size_t len );
	void * sqrl_malloc( const size_t size );
	int sqrl_memcmp( const void * const b1_, const void * const b2_, size_t len );
	void sqrl_memzero( void *buf, size_t len );
	int sqrl_mlock( void *addr, size_t len );
	int sqrl_munlock( void * const addr, size_t len );
	int sqrl_mprotect_noaccess( void *ptr );
	int sqrl_mprotect_readonly( void *ptr );
	int sqrl_mprotect_readwrite( void *ptr );
	uint32_t sqrl_random();
	void sqrl_randombytes( void *ptr, size_t len );
}
#endif // SQRL_INTERNAL_H
