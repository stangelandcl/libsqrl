/** \file SqrlUrlEncode.cpp
 *
 * \author Adam Comley
 *
 * This file is part of libsqrl.  It is released under the MIT license.
 * For more details, see the LICENSE file included with this package.
**/

#include "sqrl_internal.h"
#include "SqrlUrlEncode.h"

SQRL_STRING *SqrlUrlEncode::encode( SQRL_STRING *dest, const uint8_t *src, size_t src_len, bool append ) {
	static const char hex[] = "0123456789ABCDEF";
	if( !dest ) {
		dest = new SQRL_STRING();
	} else {
		if( !append ) SQRL_STRING_CLEAR( dest );
	}
	const char *p;
	char str[4];
	str[3] = 0;
	str[0] = '%';
	for( p = (char*)src; p[0] != 0; p++ ) {
		if( p[0] == ' ' ) {
#ifdef ARDUINO
			dest->concat( '+' );
#else
			dest->append( 1, '+' );
#endif
			continue;
		}
		if( (p[0] >= '0' && p[0] <= '9') ||
			(p[0] >= 'A' && p[0] <= 'Z') ||
			(p[0] >= 'a' && p[0] <= 'z') ) {
#ifdef ARDUINO
			dest->concat( p[0] );
#else
			dest->append( p, 1 );
#endif
			continue;
		}
		str[1] = hex[p[0] >> 4];
		str[2] = hex[p[0] & 0x0F];
#ifdef ARDUINO
		dest->concat( str );
#else
		dest->append( str, 3 );
#endif
	}
	return dest;
}

SQRL_STRING *SqrlUrlEncode::decode( SQRL_STRING *dest, const char *src, size_t src_len, bool append ) {
	if( !dest ) {
		dest = new SQRL_STRING();
	} else {
		if( !append ) SQRL_STRING_CLEAR( dest );
	}
	const char *p;
	char dc;
	int i;
	char tmp = 0;
	for( p = src; p[0] != 0; p++ ) {
		if( p[0] == '+' ) {
#ifdef ARDUINO
			dest->concat( ' ' );
#else
			dest->append( 1, ' ' );
#endif
		} else if( p[0] == '%' && strlen( p ) > 2 ) {
			for( i = 1; i <= 2; i++ ) {
				dc = p[i];
				if( dc >= '0' && dc <= '9' ) {
					dc -= 48;
				} else if( dc >= 'a' && dc <= 'f' ) {
					dc -= 87;
				} else if( dc >= 'A' && dc <= 'F' ) {
					dc -= 55;
				} else {
					dc = (char)255;
				}
				if( dc < 16 ) {
					if( i == 1 ) {
						tmp = dc << 4;
					} else {
						tmp |= dc;
					}
				}
			}
#ifdef ARDUINO
			dest->concat( tmp );
#else
			dest->append( 1, tmp );
#endif
			p += 2;
		} else {
#ifdef ARDUINO
			dest->concat( p[0] );
#else
			dest->append( p, 1 );
#endif
		}
	}
	return dest;
}

